/*++
/* NAME
/*	myrand 3
/* SUMMARY
/*	rand wrapper
/* SYNOPSIS
/*	#include <myrand.h>
/*
/*	void	mysrand(seed)
/*	int	seed;
/*
/*	int	myrand()
/* DESCRIPTION
/*	This module implements a wrapper for the portable, pseudo-random
/*	number generator.
/*
/*	mysrand() performs initialization. This call may be skipped.
/*
/*	myrand() returns a pseudo-random number in the range [0, RAND_MAX].
/*	If mysrand() was not called, it is invoked with the process ID.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>
#include <stdlib.h>
#include <unistd.h>

/* Utility library. */

#include <myrand.h>

static int myrand_initdone = 0;

/* mysrand - initialize */

void    mysrand(int seed)
{
    srand(seed);
    myrand_initdone = 1;
}

/* myrand - pseudo-random number */

int     myrand(void)
{
    if (myrand_initdone == 0)
	mysrand(getpid());
    return (rand());
}
