/*++
/* NAME
/*	fake_eugid 3
/* SUMMARY
/*	Override system UID and GID manipulating system calls
/* SYNOPSIS
/*	LD_PRELOAD="/path/to/fake_eugid.so ..." command....
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
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

#include <sys/param.h>
#include <unistd.h>
#include <grp.h>

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

int     setuid(uid_t uid)
{
    fake_ruid = fake_euid = uid;
    return (0);
}

int     setgid(gid_t gid)
{
    fake_rgid = fake_egid = gid;
    return (0);
}

int     seteuid(uid_t euid)
{
    fake_euid = euid;
    return (0);
}

int     setegid(gid_t egid)
{
    fake_egid = egid;
    return (0);
}

uid_t   getuid(void) {
    return (fake_ruid);
}

uid_t   geteuid(void) {
    return (fake_euid);
}

gid_t   getgid(void) {
    return (fake_rgid);
}

gid_t   getegid(void) {
    return (fake_egid);
}

int     setgroups(size_t size, const gid_t * gidset)
{
    int     n;

    for (n = 0; n < FAKE_NGROUPS_MAX; n++)
	fake_gidset[n] = gidset[n];
    fake_ngroups = n;
    return (0);
}

int     initgroups(const char *user, gid_t group)
{
    return (0);
}
