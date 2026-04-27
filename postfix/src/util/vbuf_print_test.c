/*
 * vbuf_print_test - test null safety in vbuf_print / vstring_sprintf
 *
 * vbuf_print() crashes with SIGSEGV when a %s argument is NULL.
 * Standard printf implementations print "(null)" for NULL %s args.
 * Postfix's vbuf_print does VBUF_STRCAT(bp, s) where s is NULL,
 * which dereferences the null pointer.
 *
 * This matters because error-handling code paths (msg_fatal, msg_warn)
 * format messages with variables that may not yet be initialized. For
 * example, mail_params_init() calls msg_fatal() with var_config_dir
 * as a %s argument, but var_config_dir can be NULL if mail_conf_read()
 * was not called first.
 *
 * Compile and run:
 *   cc -g -I. -I../../include -DMACOSX -Wno-comment \
 *      -o vbuf_print_test vbuf_print_test.c \
 *      libutil.a -lresolv
 *   ./vbuf_print_test
 */

#include <sys_defs.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>

#include <msg.h>
#include <msg_vstream.h>
#include <vstream.h>
#include <vstring.h>

static sigjmp_buf jbuf;
static volatile sig_atomic_t caught_signal;

static void catch_segv(int sig)
{
    caught_signal = sig;
    siglongjmp(jbuf, 1);
}

int     main(int argc, char **argv)
{
    VSTRING *buf;
    struct sigaction sa;
    int     failed = 0;

    msg_vstream_init(argv[0], VSTREAM_ERR);

    buf = vstring_alloc(100);

    /*
     * Test 1: vstring_sprintf with a non-NULL %s arg (baseline).
     */
    vstring_sprintf(buf, "hello %s", "world");
    if (strcmp(vstring_str(buf), "hello world") != 0) {
	msg_info("FAIL: test 1: expected 'hello world', got '%s'",
		 vstring_str(buf));
	failed++;
    } else {
	msg_info("PASS: test 1: non-NULL %%s works");
    }

    /*
     * Test 2: vstring_sprintf with a NULL %s arg.
     *
     * This crashes in vbuf_print.c:280 via VBUF_STRCAT(bp, s) where
     * s is the NULL pointer from va_arg(ap, char *).
     *
     * We install a SIGSEGV handler to catch the crash and report it.
     */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = catch_segv;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);

    caught_signal = 0;

    if (sigsetjmp(jbuf, 1) == 0) {
	VSTRING_RESET(buf);
	vstring_sprintf(buf, "value is %s end", (char *) 0);
	/*
	 * If we get here, vstring_sprintf handled the NULL without
	 * crashing. Check the output.
	 */
	msg_info("PASS: test 2: NULL %%s did not crash (output: '%s')",
		 vstring_str(buf));
    } else {
	msg_info("FAIL: test 2: NULL %%s caused signal %d (SEGV/BUS) "
		 "in vbuf_print", (int) caught_signal);
	failed++;
    }

    /*
     * Restore default signal handling.
     */
    sa.sa_handler = SIG_DFL;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);

    vstring_free(buf);

    msg_info("%s: PASS=%d FAIL=%d", argv[0],
	     2 - failed, failed);
    exit(failed ? 1 : 0);
}
