/* mac_expand_update_va - update engine */

static MAC_EXP *mac_expand_update_va(MAC_EXP *mc, int key, va_list ap)
{
    HTABLE_INFO **ht_info;
    HTABLE_INFO **ht;
    HTABLE *table;
    char   *name;
    char   *value;

#define HTABLE_CLOBBER(t, n, v) do { \
	HTABLE_INFO *_ht; \
	if ((_ht = htable_locate(t, n)) != 0) \
	    _ht->value = v; \
	else \
	    htable_enter(t, n, v); \
    } while(0);

    /*
     * Optionally create expansion context.
     */
    if (mc == 0) {
	mc = (MAC_EXP *) mymalloc(sizeof(*mc));
	mc->table = htable_create(0);
	mc->result = 0;
	mc->flags = 0;
	mc->filter = 0;
	mc->clobber = '_';
	mc->level = 0;
    }

    /*
     * Stash away the attributes.
     */
    for ( /* void */ ; key != 0; key = va_arg(ap, int)) {
	switch (key) {
	case MAC_EXP_ARG_ATTR:
	    name = va_arg(ap, char *);
	    value = va_arg(ap, char *);
	    HTABLE_CLOBBER(mc->table, name, value);
	    break;
	case MAC_EXP_ARG_TABLE:
	    table = va_arg(ap, HTABLE *);
	    ht_info = htable_list(table);
	    for (ht = ht_info; *ht; ht++)
		HTABLE_CLOBBER(mc->table, ht[0]->key, ht[0]->value);
	    myfree((char *) ht_info);
	    break;
	case MAC_EXP_ARG_FILTER:
	    mc->filter = va_arg(ap, char *);
	    break;
	case MAC_EXP_ARG_CLOBBER:
	    mc->clobber = va_arg(ap, int);
	    break;
	}
    }
    return (mc);
}

/* mac_expand_update - update or create macro expansion context */

MAC_EXP *mac_expand_update(MAC_EXP *mc, int key,...)
{
    va_list ap;

    va_start(ap, key);
    mc = mac_expand_update(mc, key, ap);
    va_end(ap);
    return (mc);
}

/* .IP key
/*	The attribute information is specified as a null-terminated list.
/*	Attributes are defined left to right; only the last definition
/*	of an attribute is remembered.
/*	The following keys are understood (types of arguments indicated
/*	in parentheses):
/* .RS
/* .IP "MAC_EXP_ARG_ATTR (char *, char *)"
/*	The next two arguments specify an attribute name and its attribute
/*	string value.  Specify a null string value for an attribute that is
/*	known but unset. Attribute string values are not copied.
/* .IP "MAC_EXP_ARG_TABLE (HTABLE *)"
/*	The next argument is a hash table with attribute names and values.
/*	Specify a null string value for an attribute that is known but unset. 
/*	Attribute string values are not copied.
/* .RE
/* .IP MAC_EXP_ARG_END
/*	A manifest constant that indicates the end of the argument list.
