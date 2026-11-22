/*++
/* NAME
/*	deliver_include 3
/* SUMMARY
/*	deliver to addresses listed in include file
/* SYNOPSIS
/*	#include "local.h"
/*
/*	int	deliver_include(state, usr_attr, path)
/*	LOCAL_STATE state;
/*	USER_ATTR usr_attr;
/*	char	*path;
/* DESCRIPTION
/*	deliver_include() processes the contents of the named include
/*	file and delivers to each address listed. Some sanity checks
/*	are done on the include file permissions and type.
/*
/*	Arguments:
/* .IP state
/*	The attributes that specify the message, recipient and more.
/*	Attributes describing alias, include, or forward expansion.
/*	A table with the results from expanding aliases or lists.
/*	A table with delivered-to: addresses taken from the message.
/* .IP usr_attr
/*	Attributes describing user rights and environment.
/* .IP path
/*	Pathname of the include file.
/* DIAGNOSTICS
/*	Fatal errors: out of memory. Warnings: bad include file type
/*	or permissions. The result is non-zero when delivery should be
/*	tried again.
/* SEE ALSO
/*	token(3) tokenize list
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
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

/* Utility library. */

#include <msg.h>
#include <htable.h>
#include <mymalloc.h>
#include <vstream.h>
#include <open_as.h>
#include <stat_as.h>
#include <iostuff.h>
#include <mypwd.h>
#include <stringops.h>

/* Global library. */

#include <bounce.h>
#include <defer.h>
#include <been_here.h>
#include <mail_params.h>
#include <ext_prop.h>
#include <sent.h>

/* Application-specific. */

#include "local.h"

/* deliver_include - open include file and deliver */

int     deliver_include(LOCAL_STATE state, USER_ATTR usr_attr, char *path)
{
    const char *myname = "deliver_include";
    struct stat st;
    struct mypasswd *file_pwd = 0;
    int     status;
    VSTREAM *fp;
    int     fd;

    /*
     * Make verbose logging easier to understand.
     */
    state.level++;
    if (msg_verbose)
	MSG_LOG_STATE(myname, state);

    /* 202606 Qualys+Mythos: add missing nesting limit. */
    if (state.level > 100) {
	msg_warn(":include: nesting limit exceeded for %s", path);
	dsb_simple(state.msg_attr.why, "5.4.6",
		   ":include: nesting limit exceeded");
	return (bounce_append(BOUNCE_FLAGS(state.request),
			      BOUNCE_ATTR(state.msg_attr)));
    }

    /*
     * DUPLICATE ELIMINATION
     * 
     * Don't process this include file more than once as this particular user.
     */
    if (been_here(state.dup_filter, "include %ld %s", (long) usr_attr.uid, path))
	return (0);
    state.msg_attr.exp_from = state.msg_attr.local;

    /*
     * Can of worms. Allow this include file to be symlinked, but disallow
     * inclusion of special files or of files with world write permission
     * enabled.
     */
    if (*path != '/') {
	msg_warn(":include:%s uses a relative path", path);
	dsb_simple(state.msg_attr.why, "5.3.5",
		   "mail system configuration error");
	return (bounce_append(BOUNCE_FLAGS(state.request),
			      BOUNCE_ATTR(state.msg_attr)));
    }
    if (stat_as(path, &st, usr_attr.uid, usr_attr.gid) < 0) {
	msg_warn("unable to lookup :include: file %s: %m", path);
	dsb_simple(state.msg_attr.why, "5.3.5",
		   "mail system configuration error");
	return (bounce_append(BOUNCE_FLAGS(state.request),
			      BOUNCE_ATTR(state.msg_attr)));
    }
    if (S_ISREG(st.st_mode) == 0) {
	msg_warn(":include: file %s is not a regular file", path);
	dsb_simple(state.msg_attr.why, "5.3.5",
		   "mail system configuration error");
	return (bounce_append(BOUNCE_FLAGS(state.request),
			      BOUNCE_ATTR(state.msg_attr)));
    }
    if (st.st_mode & S_IWOTH) {
	msg_warn(":include: file %s is world writable", path);
	dsb_simple(state.msg_attr.why, "5.3.5",
		   "mail system configuration error");
	return (bounce_append(BOUNCE_FLAGS(state.request),
			      BOUNCE_ATTR(state.msg_attr)));
    }
    if (st.st_mode & S_IWGRP)
	msg_warn(":include: file %s is group writable", path);

    /*
     * DELIVERY POLICY
     * 
     * Set the expansion type attribute so that we can decide if destinations
     * such as /file/name and |command are allowed at all.
     */
    state.msg_attr.exp_type = EXPAND_TYPE_INCL;

    /*
     * DELIVERY RIGHTS
     * 
     * When an include file is listed in a root-owned alias: (1) use the rights
     * of the parent directory owner if that is a non-root user, else (2) use
     * the rights of the include file owner. Branch (1) defends against an
     * attack reported by Anton Klingspor where a compromised mailing list
     * manager could replace an include file with a hardlink or symlink to a
     * protected file, and reveal its content in bounce messages.
     * 
     * When an include file is listed in a user-owned alias or .forward file,
     * leave the delivery rights alone. Users should not be able to make
     * things happen with someone else's rights just by including some file
     * that is owned by their victim.
     * 
     * In all cases, delivery to command or file will reduce root privilege to
     * default user privilege.
     */
    if (usr_attr.uid == 0) {
	VSTRING *parent_buf = vstring_alloc(100);
	const char *parent_path = sane_dirname(parent_buf, path);
	struct stat parent_st;
	int     stat_res;

	stat_res = stat(parent_path, &parent_st);	/* not lstat */
	if (stat_res == 0 && (parent_st.st_mode & (S_IWGRP | S_IWOTH)))
	    /* Warn not reject, to avoid breaking existing deployments. */
	    msg_warn(":include: file %s has group or world writable "
		     "parent %s", path, parent_path);
	vstring_free(parent_buf);
	if (stat_res < 0) {
	    msg_warn("%s: cannot find :include: file parent", path);
	    dsb_simple(state.msg_attr.why, "4.3.5",
		       "mail system configuration error");
	    return (defer_append(BOUNCE_FLAGS(state.request),
				 BOUNCE_ATTR(state.msg_attr)));
	}
	/* Non-root parent owner rights override file owner rights. */
	if (parent_st.st_uid != 0)
	    st = parent_st;
	if ((errno = mypwuid_err(st.st_uid, &file_pwd)) != 0 || file_pwd == 0) {
	    msg_warn(errno ? "cannot find username for uid %ld: %m" :
		     "cannot find username for uid %ld", (long) st.st_uid);
	    msg_warn("%s: cannot find :include: file owner username", path);
	    dsb_simple(state.msg_attr.why, "4.3.5",
		       "mail system configuration error");
	    return (defer_append(BOUNCE_FLAGS(state.request),
				 BOUNCE_ATTR(state.msg_attr)));
	}
	if (file_pwd->pw_uid != 0)
	    SET_USER_ATTR(usr_attr, file_pwd, state.level);
    }

    /*
     * MESSAGE FORWARDING
     * 
     * When no owner attribute is set (either via an owner- alias, or as part of
     * .forward file processing), set the owner attribute, to disable direct
     * delivery of local recipients. By now it is clear that the owner
     * attribute should have been called forwarder instead.
     */
    if (state.msg_attr.owner == 0)
	state.msg_attr.owner = state.msg_attr.rcpt.address;

    /*
     * From here on no early returns or we have a memory leak.
     * 
     * FILE OPEN RIGHTS
     * 
     * Use the delivery rights to open the include file. When no delivery rights
     * were established sofar, the file containing the :include: is owned by
     * root, so it should be OK to open any file that is accessible to root.
     * The command and file delivery routines are responsible for setting the
     * proper delivery rights. These are the rights of the default user, in
     * case the :include: is in a root-owned alias.
     * 
     * Don't propagate unmatched extensions unless permitted to do so.
     */
#define FOPEN_AS(p,u,g) ((fd = open_as(p,O_RDONLY,0,u,g)) >= 0 ? \
				vstream_fdopen(fd,O_RDONLY) : 0)

    if (msg_verbose)
	msg_info("%s: open_as(path=%s, uid=%lu, gid=%lu)", __func__, path,
		 (unsigned long) usr_attr.uid, (unsigned long) usr_attr.gid);
    if ((fp = FOPEN_AS(path, usr_attr.uid, usr_attr.gid)) == 0) {
	msg_warn("cannot open :include: file %s as uid=%lu gid=%lu: %m", path,
		 (unsigned long) usr_attr.uid, (unsigned long) usr_attr.gid);
	/* 4.x.x Because this may be triggered by new Klingspor defense. */
	dsb_simple(state.msg_attr.why, "4.3.5",
		   "mail system configuration error");
	status = bounce_append(BOUNCE_FLAGS(state.request),
			       BOUNCE_ATTR(state.msg_attr));
    } else {
	if ((local_ext_prop_mask & EXT_PROP_INCLUDE) == 0)
	    state.msg_attr.unmatched = 0;
	close_on_exec(vstream_fileno(fp), CLOSE_ON_EXEC);
	status = deliver_token_stream(state, usr_attr, fp, (int *) 0);
	if (vstream_fclose(fp))
	    msg_warn("close %s: %m", path);
    }

    /*
     * Cleanup.
     */
    if (file_pwd)
	mypwfree(file_pwd);

    return (status);
}
