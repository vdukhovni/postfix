/* Adapted from libevent. */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef EV_SET
#define EV_SET(kp, id, fi, fl, ffl, da, ud) do { \
	struct kevent *__kp = (kp); \
	__kp->ident = (id); \
	__kp->filter = (fi); \
	__kp->flags = (fl); \
	__kp->fflags = (ffl); \
	__kp->data = (da); \
	__kp->udata = (ud); \
    } while(0)
#endif

int     main(int argc, char **argv)
{
    int     kq;
    struct kevent test_change;
    struct kevent test_result;

    if ((kq = kqueue()) < 0) {
	perror("kqueue");
	exit(1);
    }
#define TEST_FD (-1)

    EV_SET(&test_change, TEST_FD, EVFILT_READ, EV_ADD, 0, 0, 0);
    if (kevent(kq,
	       &test_change, sizeof(test_change) / sizeof(struct kevent),
	       &test_result, sizeof(test_result) / sizeof(struct kevent),
	       (struct timespec *) 0) != 1 ||
	test_result.ident != TEST_FD ||
	test_result.flags != EV_ERROR) {
	fprintf(stderr, "kqueue is broken\n");
	exit(1);
    }
    exit(0);
}
