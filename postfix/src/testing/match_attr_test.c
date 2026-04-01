/*
  * Test program to exercise match_attr functions including logging. See
  * documentation in PTEST_README for the structure of this file.
  */

 /*
  * System library.
  */
#include <sys_defs.h>

 /*
  * Utility library.
  */
#include <attr.h>
#include <vstring.h>

 /*
  * Test library.
  */
#include <make_attr.h>
#include <match_attr.h>
#include <ptest.h>

typedef struct PTEST_CASE {
    const char *testname;		/* Human-readable description */
    void    (*action) (PTEST_CTX *t, const struct PTEST_CASE *);
} PTEST_CASE;

static void test_eq_attr_equal(PTEST_CTX *t, const PTEST_CASE *unused)
{
    VSTRING *want_attr;

    /*
     * Serialize some attributes.
     */
    want_attr = make_attr(ATTR_FLAG_NONE,
			  SEND_ATTR_STR("this-key", "this-value"),
			  SEND_ATTR_STR("that-key", "that-value"),
			  ATTR_TYPE_END);

    /*
     * VERIFY that this serialized attribute list matches ifself.
     */
    if (!eq_attr(t, "want_attr", want_attr, want_attr))
	ptest_fatal(t, "eq_attr() returned false for identical objects");

    /*
     * Clean up.
     */
    vstring_free(want_attr);
}

static void test_eq_attr_swapped(PTEST_CTX *t, const PTEST_CASE *unused)
{
    VSTRING *want_attr;
    VSTRING *swapped_attr;

    /*
     * Serialize some attributes.
     */
    want_attr = make_attr(ATTR_FLAG_NONE,
			  SEND_ATTR_STR("this-key", "this-value"),
			  SEND_ATTR_STR("that-key", "that-value"),
			  ATTR_TYPE_END);
    swapped_attr = make_attr(ATTR_FLAG_NONE,
			     SEND_ATTR_STR("that-key", "that-value"),
			     SEND_ATTR_STR("this-key", "this-value"),
			     ATTR_TYPE_END);

    /*
     * VERIFY that eq_attr() report attributes that differ only in order.
     */
    expect_ptest_error(t, "attribute order differs");
    if (eq_attr(t, "want_attr", swapped_attr, want_attr))
	ptest_fatal(t, "eq_attr() returned true for swapped objects");

    /*
     * Clean up.
     */
    vstring_free(want_attr);
}

static void test_eq_attr_diff(PTEST_CTX *t, const PTEST_CASE *unused)
{
    VSTRING *want_attr;
    VSTRING *swapped_attr;

    /*
     * Serialize some attributes.
     */
    want_attr = make_attr(ATTR_FLAG_NONE,
			  SEND_ATTR_STR("this-key", "this-value"),
			  SEND_ATTR_STR("that-key", "that-value"),
			  SEND_ATTR_STR("same-key", "same-value"),
			  ATTR_TYPE_END);
    swapped_attr = make_attr(ATTR_FLAG_NONE,
			     SEND_ATTR_STR("not-this-key", "this-value"),
			     SEND_ATTR_STR("that-key", "not-that-value"),
			     SEND_ATTR_STR("same-key", "same-value"),
			     ATTR_TYPE_END);

    /*
     * Verify that match_attr reports the expected differences.
     */
    expect_ptest_error(t, "attributes differ");
    expect_ptest_error(t, "+not-this-key = this-value");
    expect_ptest_error(t, "+that-key = not-that-value");
    expect_ptest_error(t, "-that-key = that-value");
    expect_ptest_error(t, "-this-key = this-value");
    if (eq_attr(t, "want_attr", swapped_attr, want_attr))
	ptest_fatal(t, "eq_attr() returned true for different objects");

    /*
     * Clean up.
     */
    vstring_free(want_attr);
    vstring_free(swapped_attr);
}

 /*
  * Test cases.
  */
const PTEST_CASE ptestcases[] = {
    {
	"Compare identical attribute lists", test_eq_attr_equal,
    },
    {
	"Compare swapped attribute lists", test_eq_attr_swapped,
    },
    {
	"Compare different attribute lists", test_eq_attr_diff,
    },
};

#include <ptest_main.h>
