/*++
/* NAME
/*	recipient_list_test
/* SUMMARY
/*	test integer overflow in recipient list growth
/* SYNOPSIS
/*	recipient_list_test
/* DESCRIPTION
/*	This program tests that recipient_list_add() detects integer
/*	overflow when doubling the list capacity.
/*
/*	The list uses 'int' for the avail field. When avail exceeds
/*	INT_MAX/2, the expression "avail * 2" overflows, producing a
/*	negative or small positive value. This undersized value is
/*	passed to myrealloc(), which allocates too little memory.
/*	The subsequent write to list->info[list->len] overflows the
/*	heap buffer.
/*
/*	This test exposes the bug by setting avail just below the
/*	overflow threshold and adding one more recipient.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/*--*/

#include <sys_defs.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>

#include <msg.h>
#include <msg_vstream.h>
#include <vstream.h>
#include <mymalloc.h>

#include "recipient_list.h"

 /*
  * We can't actually allocate INT_MAX/2 entries — that would be terabytes.
  * Instead, we directly manipulate the list internals to simulate a list
  * that has grown to near the overflow point, then call recipient_list_add()
  * to trigger the overflow.
  *
  * The overflow path:
  *   avail = INT_MAX/2 + 1 (e.g., 1073741825 on 32-bit int)
  *   new_avail = avail * 2  => integer overflow => negative or small value
  *   myrealloc(info, new_avail * sizeof(RECIPIENT)) => tiny allocation
  *   list->info[list->len] = ... => heap buffer overflow
  */

static jmp_buf test_jbuf;
static volatile int test_failed;

static void catch_crash(int sig)
{
    test_failed = 1;
    longjmp(test_jbuf, 1);
}

int     main(int argc, char **argv)
{
    RECIPIENT_LIST list;
    int     overflow_avail;
    int     new_avail_would_be;

    msg_vstream_init(argv[0], VSTREAM_ERR);

    /*
     * Test 1: Verify the arithmetic overflow exists.
     *
     * Compute what avail * 2 produces when avail > INT_MAX/2.
     */
    overflow_avail = INT_MAX / 2 + 1;
    new_avail_would_be = overflow_avail * 2;

    if (new_avail_would_be > overflow_avail) {
	msg_info("SKIP: int overflow does not occur on this platform "
		 "(overflow_avail=%d, doubled=%d)",
		 overflow_avail, new_avail_would_be);
	exit(0);
    }
    msg_info("TEST: avail=%d, avail*2=%d (overflow confirmed)",
	     overflow_avail, new_avail_would_be);

    /*
     * Test 2: Demonstrate that recipient_list_add() passes the
     * overflowed value to myrealloc().
     *
     * We initialize a minimal list, then set avail and len to the
     * overflow threshold. The next recipient_list_add() will compute
     * new_avail = avail * 2, which overflows. On most platforms,
     * myrealloc() will then either:
     *   (a) receive a negative size_t (huge allocation) and fail, or
     *   (b) receive a small positive value, succeed, and the subsequent
     *       write to list->info[list->len] corrupts the heap.
     *
     * Either way, this should not be allowed to happen. The correct
     * behavior would be to detect the overflow and call msg_fatal().
     */
    recipient_list_init(&list, RCPT_LIST_INIT_STATUS);

    /*
     * Set the list to look like it has grown to the overflow point.
     * We keep the original small allocation in list->info — we don't
     * actually need the entries, we just need avail and len to trigger
     * the overflow path in recipient_list_add().
     *
     * Set len = avail so the next add triggers a resize.
     */
    list.avail = overflow_avail;
    list.len = overflow_avail;

    /*
     * Install a signal handler to catch the crash, so we can report
     * the result instead of just dying.
     */
    signal(SIGSEGV, catch_crash);
    signal(SIGBUS, catch_crash);
    signal(SIGABRT, catch_crash);

    test_failed = 0;

    if (setjmp(test_jbuf) == 0) {
	/*
	 * This call will compute new_avail = overflow_avail * 2, which
	 * overflows. myrealloc() receives a bogus size.
	 *
	 * With Guard Malloc or ASAN, this crashes immediately.
	 * Without them, myrealloc() may call msg_fatal() on a huge
	 * (negative-to-size_t) allocation, or may succeed with a
	 * small allocation and crash on the write.
	 */
	recipient_list_add(&list, 0L, "", 0, "test@test", "test@test");

	/*
	 * If we get here without crashing, the overflowed allocation
	 * "succeeded" with a tiny buffer. The heap is now corrupt even
	 * if we haven't crashed yet. This is the silent corruption case.
	 */
	msg_info("FAIL: recipient_list_add() did not detect integer overflow "
		 "(new_avail would be %d, heap is corrupt)",
		 new_avail_would_be);
	exit(1);
    }

    /*
     * We caught a signal — the overflow led to a crash.
     */
    if (test_failed) {
	msg_info("FAIL: recipient_list_add() crashed due to integer overflow "
		 "(avail=%d, avail*2=%d)",
		 overflow_avail, new_avail_would_be);
	exit(1);
    }

    /* Not reached, but keep the compiler happy. */
    exit(0);
}
