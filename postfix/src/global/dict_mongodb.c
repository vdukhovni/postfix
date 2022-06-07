/*++
/* NAME
/*	dict_mongodb 3
/* SUMMARY
/*	dictionary interface to mongodb, compatible with libmongoc-1.0
/* SYNOPSIS
/*	#include <dict_mongodb.h>
/*
/*	DICT *dict_mongodb_open(name, open_flags, dict_flags)
/*	const char *name;
/*	int open_flags;
/*	int dict_flags;
/* DESCRIPTION
/*	dict_mongodb_open() opens a mongodb, providing
/*	a dictionary interface for Postfix mappings.
/*	The result is a pointer to the installed dictionary.
/*
/*	Configuration parameters are described in mongodb_table(5).
/*
/*	Arguments:
/* .IP name
/*	Either the path to the MongoDB configuration file (if it starts
/*	with '/' or '.'), or the prefix which will be used to obtain
/*	main.cf configuration parameters for this search.
/*
/*	In the first case, the configuration parameters below are
/*	specified in the file as \fIname\fR=\fIvalue\fR pairs.
/*
/*	In the second case, the configuration parameters are
/*	prefixed with the value of \fIname\fR and an underscore,
/*	and they are specified in main.cf.  For example, if this
/*	value is \fImongodbconf\fR, the parameters would look like
/*	\fImongodbconf_uri\fR, \fImongodbconf_collection\fR, and so on.
/*
/* .IP open_flags
/*	Must be O_RDONLY
/* 
/* .IP dict_flags
/*	See dict_open(3).
/* 
/* SEE ALSO
/*	dict(3) generic dictionary manager
/* HISTORY
/* .ad
/* .fi
/* 
/* AUTHOR(S)
/*  Hamid Maadani (hamid@dexo.tech)
/*  Dextrous Technologies, LLC
/*  P.O. Box 213
/*  5627 Kanan Rd.,
/*  Agoura Hills, CA, USA
/*  
/* Based on the work done by:
/*  Stephan Ferraro, Aionda GmbH
/*  E-Mail: stephan@ferraro.net
/*--*/

/* System library. */
#include "sys_defs.h"

#ifdef HAS_MONGODB
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

/* Utility library. */

#include "dict.h"
#include "msg.h"
#include "mymalloc.h"
#include "vstring.h"
#include "stringops.h"
#include <auto_clnt.h>
#include <vstream.h>

/* Global library. */

#include "cfg_parser.h"
#include "db_common.h"

/* Application-specific. */

#include <bson/bson.h>
#include <mongoc/mongoc.h>
#include "dict_mongodb.h"

#define QUERY_BUFFER_SIZE 1000
#define INIT_VSTR(buf, len) do { \                                    
	if (buf == 0) \       
		buf = vstring_alloc(len); \
	VSTRING_RESET(buf); \
	VSTRING_TERMINATE(buf); \                            
} while (0)

/* Structure of one mongodb dictionary handle. */
typedef struct {
	DICT dict; /* parent class */
	CFG_PARSER *parser; /* common parameter parser */
	void *ctx;
	char *uri; /* URI like mongodb+srv://localhost:27017 */
	char *dbname; /* database name */
	char *collection; /* collection name */
	char *filter; /* MongoDB query */
	char *options; /* MongoDB query options */
	mongoc_client_pool_t *client_pool; /* Mongo client pool */
} DICT_MONGODB;

/* mongodb_parse_config - parse mongodb configuration file */
static void mongodb_parse_config(DICT_MONGODB *dict_mongodb, const char *mongodbcf)
{
	CFG_PARSER *p = dict_mongodb->parser;

	dict_mongodb->uri = cfg_get_str(p, "uri", "", 1, 0);
	dict_mongodb->dbname = cfg_get_str(p, "dbname", "", 1, 0);
	dict_mongodb->collection = cfg_get_str(p, "collection", "", 1, 0);
	dict_mongodb->filter = cfg_get_str(p, "filter", "{}", 1, 0);
	dict_mongodb->options = cfg_get_str(p, "options", NULL, 1, 0);

	dict_mongodb->ctx = 0;
	(void) db_common_parse(&dict_mongodb->dict, &dict_mongodb->ctx, dict_mongodb->filter, 1);
}

/* dict_mongodb_lookup - find database entry using mongo query language */
static const char *dict_mongodb_lookup(DICT *dict, const char *name)
{
	DICT_MONGODB *dict_mongodb = (DICT_MONGODB *) dict;
	mongoc_client_t *client = NULL;
	mongoc_collection_t *coll = NULL;
	mongoc_cursor_t *cursor = NULL;
	const bson_t *doc = NULL;
	bson_t *query = NULL;
	bson_t *options = NULL;
	bson_error_t error;
	char *result = NULL;
	static VSTRING *queryString = NULL;

	dict->error = DICT_STAT_SUCCESS;

	client = mongoc_client_pool_try_pop(dict_mongodb->client_pool);
	if (!client) {
		msg_error("Failed to fetch a connection from mongo connection pool!");
		DICT_ERR_VAL_RETURN(dict, DICT_STAT_ERROR, NULL);
	}

	coll = mongoc_client_get_collection(client, dict_mongodb->dbname, dict_mongodb->collection);
	if (!coll) {
		msg_error("Failed to get collection [%s] from [%s]", dict_mongodb->collection, dict_mongodb->dbname);
		dict->error = DICT_STAT_ERROR;
		goto cleanup;
	}

	if (dict_mongodb->options) {
		options = bson_new_from_json((uint8_t *)dict_mongodb->options, -1, &error);
		if (!options) {
			msg_error("Failed to create query options from '%s' : %s", dict_mongodb->options, error.message);
			dict->error = DICT_STAT_ERROR;
			goto cleanup;
		}
	}

	INIT_VSTR(queryString, QUERY_BUFFER_SIZE);
	db_common_expand(dict_mongodb->ctx, dict_mongodb->filter, name, 0, queryString, 0);
	query = bson_new_from_json((uint8_t *)vstring_str(queryString), -1, &error);
	if (! query) {
		msg_error("Failed to create a query from '%s' : %s", vstring_str(queryString), error.message);
		dict->error = DICT_STAT_ERROR;
		goto cleanup;
	}

	cursor = mongoc_collection_find_with_opts(coll, query, options, NULL);
	if (mongoc_cursor_error(cursor, &error)) {
		msg_error("Cursor error: %s", error.message);
		dict->error = DICT_STAT_ERROR;
		goto cleanup;
	}

	if (mongoc_cursor_next(cursor, &doc)) {
		result = bson_as_canonical_extended_json(doc, NULL);
	} else {
		// Should we return a failure if query has no match? Sticking with DICT_STAT_SUCCESS for now.
		//dict->error = DICT_STAT_FAIL;
	}

cleanup:
	mongoc_cursor_destroy(cursor);
	bson_destroy(query);
	bson_destroy(options);

	mongoc_collection_destroy(coll);
	mongoc_client_pool_push(dict_mongodb->client_pool, client);

	return result;
}

/* dict_mongodb_close - close MongoDB database */
static void dict_mongodb_close(DICT *dict)
{
	DICT_MONGODB *dict_mongodb = (DICT_MONGODB *) dict;

	cfg_parser_free(dict_mongodb->parser);
	if (dict_mongodb->ctx) {                                                                                         
		db_common_free_ctx(dict_mongodb->ctx);                                                                   
	} 
	myfree(dict_mongodb->collection);
	myfree(dict_mongodb->filter);
	myfree(dict_mongodb->options);

	// Release our handles and clean up libmongoc
	mongoc_client_pool_destroy(dict_mongodb->client_pool);
	mongoc_cleanup();

	dict_free(dict);
}

/* dict_mongodb_open - open MongoDB database connection */
DICT *dict_mongodb_open(const char *name, int open_flags, int dict_flags)
{
	DICT_MONGODB *dict_mongodb;
	CFG_PARSER *parser;
	bson_error_t error;

	// Sanity checks.
	if (open_flags != O_RDONLY) {
		return (dict_surrogate(DICT_TYPE_MONGODB, name, open_flags, dict_flags,
				"%s:%s map requires O_RDONLY access mode", DICT_TYPE_MONGODB, name));
	}

	// Open the configuration file.
	if ((parser = cfg_parser_alloc(name)) == 0) {
		return (dict_surrogate(DICT_TYPE_MONGODB, name, open_flags, dict_flags, "open %s: %m", name));
	}

	// Create the dictionary object.
	dict_mongodb = (DICT_MONGODB *)dict_alloc(DICT_TYPE_MONGODB, name, sizeof(*dict_mongodb));

	/* Pass pointer functions */
	dict_mongodb->dict.lookup = dict_mongodb_lookup;
	dict_mongodb->dict.close = dict_mongodb_close;
	dict_mongodb->dict.flags = dict_flags;
	dict_mongodb->parser = parser;
	dict_mongodb->client_pool = NULL;

	/* Parse config */
	mongodb_parse_config(dict_mongodb, name);

	dict_mongodb->dict.owner = cfg_get_owner(dict_mongodb->parser);

	// Initialize libmongoc's internals
	mongoc_init();

	// Safely create a MongoDB URI object from the given string
	mongoc_uri_t *uri = mongoc_uri_new_with_error(dict_mongodb->uri, &error);
	if (! uri) {
		msg_error("Failed to parse URI: %s error message: %s", dict_mongodb->uri, error.message);
		dict_mongodb->dict.error = DICT_STAT_ERROR;
	} else {
		// Create a new client pool
		dict_mongodb->client_pool = mongoc_client_pool_new(uri);

		mongoc_uri_destroy(uri);
	}

	return (DICT_DEBUG(&dict_mongodb->dict));
}

#endif
