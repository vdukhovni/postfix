/*++
/* NAME
/*      map_search 3h
/* SUMMARY
/*      lookup table search list support
/* SYNOPSIS
/*      #include <map_search.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <name_code.h>

 /*
  * External interface.
  * 
  * The map_search module maintains a lookup table with MAP_SEARCH results,
  * indexed by the unparsed form. The conversion from unparsed form to
  * MAP_SEARCH result is controlled by a NAME_CODE table, and there can be
  * only one mapping per unparsed name. To keep things sane every MAP_SEARCH
  * result in the lookup table must be built using the same NAME_CODE table.
  * 
  * Alternative 1: let the MAP_SEARCH user store each MAP_SEARCH pointer. But
  * that would clumsify code that wants to use MAP_SEARCH functionality.
  * 
  * Alternative 2: change map_search_init() to return a pointer to {HTABLE *,
  * NAME_CODE *}, and require that the MAP_SEARCH user pass that pointer to
  * other map_search_xxx() calls. That would be as clumsy as Alternative 1.
  */
typedef struct {
    char   *map_type_name;		/* "type:name", owned */
    char   *search_list;		/* null or owned */
} MAP_SEARCH;

extern void map_search_init(const NAME_CODE *);
extern const MAP_SEARCH *map_search_create(const char *);
extern const MAP_SEARCH *map_search_lookup(const char *);

#define MAP_SEARCH_ATTR_NAME_SEARCH	"search"

#define MAP_SEARCH_CODE_UNKNOWN		127

/* LICENSE
/* .ad
/* .fi
/*      The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*      Wietse Venema
/*      Google, Inc.
/*      111 8th Avenue
/*      New York, NY 10011, USA
/*--*/
