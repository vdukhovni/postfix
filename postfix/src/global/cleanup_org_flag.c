/*++
/* NAME
/*	cleanup_org_flag 3
/* SUMMARY
/*	cleanup(8) origin flag management
/* SYNOPSIS
/*	#include <cleanup_user.h>
/*
/*	int     cleanup_org_flag_from_source_flag(int source_flag)
/*
/*	const char *cleanup_org_flag_to_name(int org_flag)
/*
/*	int	cleanup_org_flag_from_name(const char *name)
/* DESCRIPTION
/*	cleanup_org_flag_from_source_flag() maps an internal source
/*	flag MAIL_SRC_MASK_{BOUNCE,NOTIFY,VERIFY} to the corresponding
/*	cleanup origin flag. It returns zero for other inputs (no flag,
/*	multiple flags).
/*
/*	cleanup_org_flag_to_name() maps a cleanup origin flag to the
/*	corresponding symbolic name. It returns a null pointer for other
/*	inputs (no flag, multiple flags). Unlike cleanup_strflags()
/*	this function requires that at most one flag is set.
/*
/*	cleanup_org_flag_from_name() maps a symbolic name to the
/*	corresponding cleanup origin flag. It returns zero for other
/*	inputs (empty name, unknown name).
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	porcupine.org
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>

 /*
  * Utility library.
  */
#include <name_code.h>

 /*
  * Global library.
  */
#include <cleanup_user.h>
#include <mail_proto.h>

 /*
  * For now, we care only about provenance of internally-generated messages.
  * Bitflags are not rich enough to capture the provenance of external inputs.
  */
const static NAME_CODE cleanup_org_table[] = {
    CLEANUP_NAME_ORG_BOUNCE, CLEANUP_FLAG_ORG_BOUNCE,
    CLEANUP_NAME_ORG_NOTIFY, CLEANUP_FLAG_ORG_NOTIFY,
    CLEANUP_NAME_ORG_VERIFY, CLEANUP_FLAG_ORG_VERIFY,
    0,
};

/* cleanup_org_flag_from_source_flag - map source flag to cleanup origin */

int     cleanup_org_flag_from_source_flag(int source_flag)
{;
    switch (source_flag) {
    case MAIL_SRC_MASK_BOUNCE:
	return (CLEANUP_FLAG_ORG_BOUNCE);
    case MAIL_SRC_MASK_NOTIFY:
	return (CLEANUP_FLAG_ORG_NOTIFY);
    case MAIL_SRC_MASK_VERIFY:
	return (CLEANUP_FLAG_ORG_VERIFY);
    }
    return (0);
}

/* cleanup_org_flag_to_name - map cleanup origin flag to symbolic name */

const char *cleanup_org_flag_to_name(int flag)
{
    int     org_flag;
    const char *ret;

    org_flag = flag & CLEANUP_FLAG_ORG_ALL;
    ret = str_name_code(cleanup_org_table, org_flag);
    return (ret);
}

/* cleanup_org_flag_from_name - map symbolic name to cleanup origin flag */

int     cleanup_org_flag_from_name(const char *name)
{
    int     org_flag;

    org_flag = name_code(cleanup_org_table, NAME_CODE_FLAG_NONE, name);
    return (org_flag);
}
