#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>

static int count = 0;

int     main(int argc, char **argv)
{
    sigjmp_buf env;
    int     retval;

    switch (retval = sigsetjmp(env, 1)) {
    case 0:
	siglongjmp(env, 12345);
    case 12345:
	break;
    default:
	fprintf(stderr, "Error: siglongjmp ignores second argument\n");
	exit(1);
    }

    switch (retval = sigsetjmp(env, 1)) {
    case 0:
	if (count++ > 0) {
	    fprintf(stderr, "Error: not overriding siglongjmp(env, 0)\n");
	    exit(1);
	}
	siglongjmp(env, 0);
    case 1:
	break;
    default:
	fprintf(stderr, "Error: overriding siglongjmp(env, 0) with %d\n",
		retval);
	exit(1);
    }
    exit(0);
}
