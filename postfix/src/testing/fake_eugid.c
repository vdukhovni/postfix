/*++
/* NAME
/*	fake_eugid 3
/* SUMMARY
/*	Override system UID and GID manipulating system calls
/* SYNOPSIS
/*	LD_PRELOAD="/path/to/fake_eugid.so ..." command....	(ELF)
/*
/*	DYLD_INSERT_LIBRARIES=/path/to/fake_eugid.dylib command....	(MacOS)
/*	
/*	uid_t	getuid(void)
/*	
/*	uid_t	geteuid(void)
/*	
/*	gid_t	getgid(void)
/*	
/*	gid_t	getegid(void)
/*
/*	int	setuid(uid_t uid)
/*	
/*	int	seteuid(uid_t euid)
/*	
/*	int	setgid(gid_t gid)
/*	
/*	int	setegid(gid_t egid)
/*
/*	int	setgroups(int ngroups, const gid_t *gidset)
/*
/*	int	initgroups(const char *user, gid_t group)
/* DESCRIPTION
/*	These fakes pretend that a process starts with root privileges.
/*	Instead of manipulating process privileges, they maintain internal
/*	state and always succeed. There is no enforcement of privileges;
/*	for example, setuid(0) succeeds unconditionally, and non-fake
/*	system calls will use the privileges of the user that runs
/*	this code.
/*
/*	setgroups() and initgroups() do not yet maintain fake state.
/*	That would have to change when getgroups() support is needed.
/*
/*	Portability: with ELF dynamic linking (Linux/BSD/Solaris/HP)
/*	a preloaded definition overrides the libc symbol by name. MacOS
/*	binds each reference to a specific library (two-level namespace),
/*	so a plain same-name definition in a DYLD_INSERT_LIBRARIES object
/*	is ignored. There, each replacement is named fake_xxx and
/*	registered in a __DATA,__interpose table that dyld applies to
/*	all images. The FAKE() and INTERPOSE() macros hide the difference;
/*	the ELF code path is unchanged.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

#include <sys_defs.h>
#include <sys/param.h>
#include <unistd.h>
#include <grp.h>

#if defined(MACOSX)

 /*
  * MacOS: define distinctly-named replacements and register interpose tuples.
  * The replacements must not be static (the address is taken here, but keeping
  * external linkage matches the ELF path and avoids surprises). dyld rewrites
  * every bound reference to 'name' so that it calls 'fake_name' instead.
  */
#define FAKE(name)	fake_##name
#define INTERPOSE(name) \
    __attribute__((used)) static struct { \
	const void *repl; \
	const void *orig; \
    } _interpose_##name __attribute__((section("__DATA,__interpose"))) = { \
	(const void *) &fake_##name, (const void *) &name \
    }

#else					/* ELF: preloading overrides by name */

#define FAKE(name)	name
#define INTERPOSE(name)	struct _interpose_unused_##name

#endif

 /*
  * The initial privilege for a Postfix daemon process is 'root' privilege.
  */
static uid_t fake_ruid = 0;
static uid_t fake_euid = 0;

static gid_t fake_rgid = 0;
static gid_t fake_egid = 0;

#define FAKE_NGROUPS_MAX	10

static int fake_ngroups;
static gid_t fake_gidset[FAKE_NGROUPS_MAX];

int     FAKE(setuid) (uid_t uid)
{
    fake_ruid = fake_euid = uid;
    return (0);
}
INTERPOSE(setuid);

int     FAKE(setgid) (gid_t gid)
{
    fake_rgid = fake_egid = gid;
    return (0);
}
INTERPOSE(setgid);

int     FAKE(seteuid) (uid_t euid)
{
    fake_euid = euid;
    return (0);
}
INTERPOSE(seteuid);

int     FAKE(setegid) (gid_t egid)
{
    fake_egid = egid;
    return (0);
}
INTERPOSE(setegid);

uid_t   FAKE(getuid) (void)
{
    return (fake_ruid);
}
INTERPOSE(getuid);

uid_t   FAKE(geteuid) (void)
{
    return (fake_euid);
}
INTERPOSE(geteuid);

gid_t   FAKE(getgid) (void)
{
    return (fake_rgid);
}
INTERPOSE(getgid);

gid_t   FAKE(getegid) (void)
{
    return (fake_egid);
}
INTERPOSE(getegid);

int     FAKE(setgroups) (SETGRPS_NUM_TYPE size, const gid_t * gidset)
{
    int     n;

    for (n = 0; n < FAKE_NGROUPS_MAX; n++)
	fake_gidset[n] = gidset[n];
    fake_ngroups = n;
    return (0);
}
INTERPOSE(setgroups);

int     FAKE(initgroups) (const char *user, INITGRPS_ARG_TYPE group)
{
    return (0);
}
INTERPOSE(initgroups);
