/*++
/* NAME
/*	match_attr 3
/* SUMMARY
/*	matchers for network address information
/* SYNOPSIS
/*	#include <match_attr.h>
/*
/*	int	eq_attr(
/*	PTEST_CTX *t,
/*	const char *what,
/*	VSTRING *got,
/*	VSTRING *want)
/* DESCRIPTION
/*	The functions described here are safe macros that include
/*	call-site information (file name, line number) that may be
/*	used in error messages.
/*
/*	eq_attr() compares two serialized attribute lists and returns
/*	whether their arguments contain the same values. If the t
/*	argument is not null, eq_attr() will report details with
/*	ptest_error()).
/* BUGS
/*	An attribute name can appear only once in an attribute list.
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
  * System library.
  */
#include <sys_defs.h>
#include <string.h>
#include <stdlib.h>

 /*
  * Utility library.
  */
#include <attr.h>
#include <htable.h>
#include <vstring.h>

 /*
  * Test library.
  */
#include <ptest.h>
#include <match_attr.h>

/* compar -qsort callback */

static int compar(const void *a, const void *b)
{
    return (strcmp((*(HTABLE_INFO **) a)->key, (*(HTABLE_INFO **) b)->key));
}

/* _eq_attr - match serialized attributes */

int     _eq_attr(const char *file, int line, PTEST_CTX *t,
	              const char *what, VSTRING *got_buf, VSTRING *want_buf)
{
    static const char myname[] = "eq_attr";
    HTABLE *got_hash;
    HTABLE *want_hash;
    int     count;
    VSTREAM *mp;
    HTABLE_INFO **ht_list;
    HTABLE_INFO **ht;
    char   *ht_value;

    if (VSTRING_LEN(got_buf) == VSTRING_LEN(want_buf)
	&& memcmp(vstring_str(got_buf), vstring_str(want_buf),
		  VSTRING_LEN(got_buf)) == 0)
	return (1);

    if (t != 0) {

	/*
	 * Deserialize the actual attributes into a hash. This loses order
	 * information.
	 */
	got_hash = htable_create(13);
	if ((mp = vstream_memopen(got_buf, O_RDONLY)) == 0)
	    ptest_fatal(t, "%s: vstream_memopen: %m", myname);
	count = attr_scan(mp, ATTR_FLAG_NONE,
			  ATTR_TYPE_HASH, got_hash,
			  ATTR_TYPE_END);
	if (vstream_fclose(mp) != 0 || count <= 0)
	    ptest_fatal(t, "%s: vstream_fclose: %m", myname);

	/*
	 * Deserialize the wanted attributes into a hash. This loses order
	 * information.
	 */
	want_hash = htable_create(13);
	if ((mp = vstream_memopen(want_buf, O_RDONLY)) == 0)
	    ptest_fatal(t, "%s: vstream_memopen: %m", myname);
	count = attr_scan(mp, ATTR_FLAG_NONE,
			  ATTR_TYPE_HASH, want_hash,
			  ATTR_TYPE_END);
	if (vstream_fclose(mp) != 0 || count <= 0)
	    ptest_fatal(t, "%s: vstream_fclose: %m", myname);

	/*
	 * Delete the intersection of the deserialized attribute lists.
	 */
	ht_list = htable_list(got_hash);
	for (ht = ht_list; *ht; ht++) {
	    if ((ht_value = htable_find(want_hash, ht[0]->key)) != 0
		&& strcmp(ht_value, ht[0]->value) == 0) {
		htable_delete(want_hash, ht[0]->key, myfree);
		/* At this point, ht_value is a dangling pointer. */
		htable_delete(got_hash, ht[0]->key, myfree);
		/* At this point, ht is a dangling pointer. */
	    }
	}
	myfree(ht_list);

	/*
	 * If the attributes differ only in order, then say so. We have no
	 * order information. This should never happen with real requests and
	 * responses.
	 */
	if (got_hash->used == 0 && want_hash->used == 0) {
	    ptest_error(t, "%s: attribute order differs", what);
	}

	/*
	 * List differences in name or value.
	 */
	else {
	    ptest_error(t, "%s: attributes differ, +got/-want follows", what);

	    /*
	     * Enumerate the unique attributes.
	     */
	    ht_list = htable_list(got_hash);
	    qsort(ht_list, got_hash->used, sizeof(*ht_list), compar);
	    for (ht = ht_list; *ht; ht++)
		ptest_error(t, "+%s = %s", ht[0]->key, (char *) ht[0]->value);
	    myfree(ht_list);

	    ht_list = htable_list(want_hash);
	    qsort(ht_list, want_hash->used, sizeof(*ht_list), compar);
	    for (ht = ht_list; *ht; ht++)
		ptest_error(t, "-%s = %s", ht[0]->key, (char *) ht[0]->value);
	    myfree(ht_list);
	}
	htable_free(got_hash, myfree);
	htable_free(want_hash, myfree);
    }
    return (0);
}
