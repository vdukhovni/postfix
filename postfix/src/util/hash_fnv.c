/*++
/* NAME
/*	hash_fnv 3
/* SUMMARY
/*	Fowler/Noll/Vo hash function
/* SYNOPSIS
/*	#include <hash_fnv.h>
/*
/*	HASH_FNV_T hash_fnv(
/*	const void *src,
/*	size_t	len)
/* DESCRIPTION
/*	hash_fnv() implements the FNV type 1a hash function.
/*
/*	To thwart collision attacks, the hash function is seeded
/*	once from /dev/urandom, and if that is unavailable, from
/*	wallclock time, monotonic system clocks, and the process
/*	ID. To disable seeding in tests, specify the NORANDOMIZE
/*	environment variable (the value does not matter).
/*
/*	By default, the function is modified to avoid a sticky state
/*	where a zero hash value remains zero when the next input
/*	byte value is zero. Compile with -DSTRICT_FNV1A to get the
/*	standard behavior.
/*
/*	The default HASH_FNV_T result type is uint64_t. When compiled
/*	with -DNO_64_BITS, the result type is uint32_t.
/* SEE ALSO
/*	http://www.isthe.com/chongo/tech/comp/fnv/index.html
/*	https://softwareengineering.stackexchange.com/questions/49550/
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*--*/

 /*
  * System library
  */
#include <sys_defs.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

 /*
  * Utility library.
  */
#include <msg.h>
#include <hash_fnv.h>

 /*
  * Application-specific.
  */
#ifdef NO_64_BITS
#define FNV_prime 		0x01000193UL
#define FNV_offset_basis	0x811c9dc5UL
#else
#define FNV_prime		0x00000100000001B3ULL
#define FNV_offset_basis	0xcbf29ce484222325ULL
#endif

 /*
  * Fall back to a mix of absolute and time-since-boot information in the
  * rare case that /dev/urandom is unavailable.
  */
#ifdef CLOCK_UPTIME
#define NON_WALLTIME_CLOCK      CLOCK_UPTIME
#elif defined(CLOCK_BOOTTIME)
#define NON_WALLTIME_CLOCK      CLOCK_BOOTTIME
#elif defined(CLOCK_MONOTONIC)
#define NON_WALLTIME_CLOCK      CLOCK_MONOTONIC
#elif defined(CLOCK_HIGHRES)
#define NON_WALLTIME_CLOCK      CLOCK_HIGHRES
#endif

/* fnv_seed - randomize the hash function */

static HASH_FNV_T fnv_seed(void)
{
    HASH_FNV_T result = 0;

    /*
     * Medium-quality seed, for defenses against local and remote attacks.
     */
    int     fd;
    int     count;

    if ((fd = open("/dev/urandom", O_RDONLY)) > 0) {
	count = read(fd, &result, sizeof(result));
	(void) close(fd);
	if (count == sizeof(result) && result != 0)
	    return (result);
    }

    /*
     * Low-quality seed, for defenses against remote attacks. Based on 1) the
     * time since boot (good when an attacker knows the program start time
     * but not the system boot time), and 2) absolute time (good when an
     * attacker does not know the program start time). Assumes a system with
     * better than microsecond resolution, and a network stack that does not
     * leak the time since boot, for example, through TCP or ICMP timestamps.
     * With those caveats, this seed is good for 20-30 bits of randomness.
     */
#ifdef NON_WALLTIME_CLOCK
    {
	struct timespec ts;

	if (clock_gettime(NON_WALLTIME_CLOCK, &ts) != 0)
	    msg_fatal("clock_gettime() failed: %m");
	result += (HASH_FNV_T) ts.tv_sec ^ (HASH_FNV_T) ts.tv_nsec;
    }
#elif defined(USE_GETHRTIME)
    result += gethrtime();
#endif

#ifdef CLOCK_REALTIME
    {
	struct timespec ts;

	if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
	    msg_fatal("clock_gettime() failed: %m");
	result += (HASH_FNV_T) ts.tv_sec ^ (HASH_FNV_T) ts.tv_nsec;
    }
#else
    {
	struct timeval tv;

	if (GETTIMEOFDAY(&tv) != 0)
	    msg_fatal("gettimeofday() failed: %m");
	result += (HASH_FNV_T) tv.tv_sec + (HASH_FNV_T) tv.tv_usec;
    }
#endif
    return (result + getpid());
}

/* hash_fnv - modified FNV 1a hash */

HASH_FNV_T hash_fnv(const void *src, size_t len)
{
    static HASH_FNV_T basis = FNV_offset_basis;
    static int randomize = 1;
    HASH_FNV_T hash;

    /*
     * Initialize.
     */
    while (randomize) {
	if (getenv("NORANDOMIZE")) {
	    randomize = 0;
	} else {
	    basis ^= fnv_seed();
	    if (basis != FNV_offset_basis)
		randomize = 0;
	}
    }

    /*
     * Add 1 to each input character, to avoid a sticky state (with hash ==
     * 0, doing "hash ^= 0" and "hash *= FNV_prime" would not change the hash
     * value.
     */
#ifdef STRICT_FNV1A
#define FNV_NEXT_CHAR(s) ((HASH_FNV_T) * (const unsigned char *) s++)
#else
#define FNV_NEXT_CHAR(s) (1 + (HASH_FNV_T) * (const unsigned char *) s++)
#endif

    hash = basis;
    while (len-- > 0) {
	hash ^= FNV_NEXT_CHAR(src);
	hash *= FNV_prime;
    }
    return (hash);
}
