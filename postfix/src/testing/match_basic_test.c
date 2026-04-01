 /*
  * Test program to exercise make_addr functions including logging. See
  * comments in ptest_main.h and pmock_expect_test.c for a documented
  * example.
  */

 /*
  * System library.
  */
#include <sys_defs.h>

 /*
  * Utility library.
  */
#include <name_code.h>
#include <name_mask.h>
#include <vstring.h>

 /*
  * Test library.
  */
#include <match_basic.h>
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;               /* Human-readable description */
    void    (*action) (PTEST_CTX *t, const struct PTEST_CASE *);
} PTEST_CASE;

static void test_eq_int(PTEST_CTX *t, const PTEST_CASE *unused)
{
    expect_ptest_error(t, "got 3, want 5");
    if (eq_int(t, "int", 3, 5)
	|| eq_int((PTEST_CTX *) 0, "int", 3, 5))
	ptest_error(t, "unexpected int match: 3 == 5");
}

static void test_eq_size_t(PTEST_CTX *t, const PTEST_CASE *unused)
{
    expect_ptest_error(t, "got 3, want 5");
    if (eq_size_t(t, "size_t", 3, 5)
	|| eq_size_t((PTEST_CTX *) 0, "size_t", 3, 5))
	ptest_error(t, "unexpected size_t match: 3 == 5");
}

static void test_eq_ssize_t(PTEST_CTX *t, const PTEST_CASE *unused)
{
    expect_ptest_error(t, "got 3, want 5");
    if (eq_ssize_t(t, "ssize_t", 3, 5)
	|| eq_ssize_t((PTEST_CTX *) 0, "ssize_t", 3, 5))
	ptest_error(t, "unexpected ssize_t match: 3 == 5");
}

#define FLAG_ONE	(1<<0)
#define FLAG_TWO	(1<<1)

static const NAME_MASK test_flags[] = {
    "one", FLAG_ONE,
    "two", FLAG_TWO,
    0,
};

static const char *flags_to_string(VSTRING *buf, int flags)
{
    return (str_name_mask_opt(buf, "flags_to_string", test_flags, flags,
		       NAME_MASK_NUMBER | NAME_MASK_PIPE | NAME_MASK_NULL));
}

static void test_eq_flags(PTEST_CTX *t, const PTEST_CASE *unused)
{
int got = FLAG_ONE;
int want = FLAG_ONE | FLAG_TWO;

    expect_ptest_error(t, "got 'one', want 'one|two'");
    if (eq_flags(t, "flags", got, want, flags_to_string)
	|| eq_flags((PTEST_CTX *) 0, "flags", got, want, flags_to_string))
	ptest_error(t, "unexpected flags match: 'one' == 'one|two'");
}

#define CODE_ONE        (1)
#define CODE_TWO        (2)

static const NAME_CODE test_codes[] = {
    "one", CODE_ONE,
    "two", CODE_TWO,
    0,
};

static const char *enum_to_string(int code)
{
    const char *result;

    if ((result = str_name_code(test_codes, code)) == 0)
	result = "unknown";
    return (result);
}

static void test_eq_enum(PTEST_CTX *t, const PTEST_CASE *unused)
{
    int     got = CODE_ONE;
    int     want = CODE_TWO;

    expect_ptest_error(t, "got 'one', want 'two'");

    if (eq_enum(t, "flags", got, want, enum_to_string)
	|| eq_enum((PTEST_CTX *) 0, "flags", got, want, enum_to_string))
	ptest_error(t, "unexpected flags match: 'one' == 'two'");
}

static void test_eq_str(PTEST_CTX *t, const PTEST_CASE *unused)
{
    const char *got = "bar";
    const char *want = "foo";

    (void) eq_str(t, "str", (char *) 0, (char *) 0);

    expect_ptest_error(t, "got '(null)', want 'foo'");
    if (eq_str(t, "str", (char *) 0, want)
	|| eq_str((PTEST_CTX *) 0, "str", (char *) 0, want))
	ptest_error(t, "null str matches non-null str");

    expect_ptest_error(t, "got 'bar', want 'foo'");
    if (eq_str(t, "str", got, want)
	|| eq_str((PTEST_CTX *) 0, "str", got, want))
	ptest_error(t, "unexpected str match: 'bar' == 'foo'");
}

static void test_eq_argv(PTEST_CTX *t, const PTEST_CASE *unused)
{
    ARGV    got = {.argc = 1,.argv = (char *[]) {"one", 0}};
    ARGV    want = {.argc = 2,.argv = (char *[]) {"one", "two", 0}};

    (void) eq_argv(t, "argv", (ARGV *) 0, (ARGV *) 0);

    expect_ptest_error(t, "got '(null)', want '(ARGV)'");
    if (eq_argv(t, "argv", (ARGV *) 0, &want)
	|| eq_argv((PTEST_CTX *) 0, "argv", (ARGV *) 0, &want))
	ptest_error(t, "null ARGV matches non-null ARGV");

    expect_ptest_error(t, "got 1, want 2");
    expect_ptest_error(t, "got '(null)', want 'two'");
    if (eq_argv(t, "argv", &got, &want)
	|| eq_argv((PTEST_CTX *) 0, "argv", &got, &want))
	ptest_error(t, "unexpected argv match: 'got' == 'want'");
}

 /*
  * Test cases.
  */
const PTEST_CASE ptestcases[] = {
    {"Compare int", test_eq_int,},
    {"Compare size_t", test_eq_size_t,},
    {"Compare ssize_t", test_eq_ssize_t,},
    {"Compare flags", test_eq_flags,},
    {"Compare enum", test_eq_enum,},
    {"Compare str", test_eq_str,},
    {"Compare argv", test_eq_argv,},
};

#include <ptest_main.h>
