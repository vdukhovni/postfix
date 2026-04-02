 /*
  * Test program to exercise dict_pipe.c. See PTEST_README for documentation.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <argv.h>
#include <dict_pipe.h>
#include <vstring.h>

 /*
  * Test library.
  */
#include <ptest.h>
#include <dict_test_helper.h>

 /*
  * The following needs to be large enough to include a null terminator in
  * every ptestcase.want field.
  */
#define MAX_PROBE	5

struct probe {
    const char *query;
    const char *want_value;
    int     want_error;
};

typedef struct PTEST_CASE {
    const char *testname;
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
    const char *type_name;
    const struct probe probes[MAX_PROBE];
} PTEST_CASE;

#define STR_OR_NULL(s)	((s) ? (s) : "null")
#define STR(x)  vstring_str(x)

static void valid_refcounts_for_good_composite_syntax(PTEST_CTX *t,
					        const struct PTEST_CASE *tp)
{
    DICT   *dict;
    int     open_flags = O_RDONLY;
    int     dict_flags = DICT_FLAG_LOCK;
    VSTRING *composite_spec = vstring_alloc(100);
    ARGV   *reg_component_specs = argv_alloc(3);
    const char *component_specs[] = {
	"static:foo",
	"inline:{xx=yy}",
	"unix:passwd.byname",
	0,
    };
    char  **cpp;

    dict_compose_spec(DICT_TYPE_PIPE, component_specs, open_flags, dict_flags,
		      composite_spec, reg_component_specs);
    dict = dict_open(STR(composite_spec), open_flags, dict_flags);

    for (cpp = reg_component_specs->argv; *cpp; cpp++) {
	if (dict_handle(*cpp) == 0) {
	    ptest_fatal(t, "table '%s' is not registered after dict_open()",
			*cpp);
	}
    }
    dict_close(dict);
    for (cpp = reg_component_specs->argv; *cpp; cpp++) {
	if (dict_handle(*cpp) != 0) {
	    ptest_fatal(t, "table '%s' is still registered after dict_close()",
			*cpp);
	}
    }
    vstring_free(composite_spec);
    argv_free(reg_component_specs);
}

static void test_dict_pipe(PTEST_CTX *t, const struct PTEST_CASE *tp)
{
    DICT   *dict;
    const struct probe *pp;
    const char *got_value;
    int     got_error;

    if ((dict = dict_open(tp->type_name, O_RDONLY, 0)) == 0)
	ptest_fatal(t, "dict_open(\"%s\", O_RDONLY, 0) failed: %m",
		    tp->type_name);
    for (pp = tp->probes; pp < tp->probes + MAX_PROBE && pp->query != 0; pp++) {
	got_value = dict_get(dict, pp->query);
	got_error = dict->error;
	if (got_value == 0 && pp->want_value == 0)
	    continue;
	if (got_value == 0 || pp->want_value == 0) {
	    ptest_error(t, "dict_get(dict, \"%s\"): got '%s', want '%s'",
			pp->query, STR_OR_NULL(got_value),
			STR_OR_NULL(pp->want_value));
	    break;
	}
	if (strcmp(got_value, pp->want_value) != 0) {
	    ptest_error(t, "dict_get(dict, \"%s\"): got '%s', want '%s'",
			pp->query, got_value, pp->want_value);
	}
	if (got_error != pp->want_error)
	    ptest_error(t, "dict_get(dict,\"%s\") error: got %d, want %d",
			pp->query, got_error, pp->want_error);
    }
    dict_free(dict);
}

static const PTEST_CASE ptestcases[] = {
    {
	.testname = "valid refcounts for good composite syntax",
	.action = valid_refcounts_for_good_composite_syntax,
    }, {
	.testname = "successful lookup: inline map + inline map",
	.action = test_dict_pipe,
	.type_name = "pipemap:{inline:{k1=v1,k2=v2},inline:{v2=v3}}",
	.probes = {
	    {"k0", 0},
	    {"k1", 0,},
	    {"k2", "v3"},
	},
    }, {
	.testname = "error propagation: inline map + fail map",
	.action = test_dict_pipe,
	.type_name = "pipemap:{inline:{k1=v1},fail:fail}",
	.probes = {
	    {"k0", 0, 0},
	    {"k1", 0, DICT_ERR_RETRY},
	},
    }, {
	.testname = "error propagation: fail map + inline map",
	.action = test_dict_pipe,
	.type_name = "pipemap:{fail:fail,inline:{k1=v1}}",
	.probes = {
	    {"k1", 0, DICT_ERR_RETRY},
	},
    },
};

#include <ptest_main.h>
