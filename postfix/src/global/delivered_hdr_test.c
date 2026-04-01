 /*
  * Test program to exercise delivered_hdr.c. See ptest_main.h for a
  * documented example.
  */

 /*
  * System library.
  */
#include <sys_defs.h>
#include <string.h>

 /*
  * Utility library.
  */
#include <msg_vstream.h>
#include <mymalloc.h>

 /*
  * Global library.
  */
#include <mail_params.h>
#include <delivered_hdr.h>
#include <rec_type.h>
#include <record.h>

 /*
  * Test library.
  */
#include <ptest.h>

 /*
  * Satisfy a configuration dependency.
  */
char   *var_drop_hdrs;

 /*
  * Test library adaptor.
  */
typedef struct PTEST_CASE {
    const char *testname;
    void    (*action) (PTEST_CTX *, const struct PTEST_CASE *);
} PTEST_CASE;

#define FOUND		1
#define NOT_FOUND	0

static void test_delivered_hdr_find(PTEST_CTX *t, const PTEST_CASE *unused)
{
    VSTRING *mem_bp;
    VSTREAM *mem_fp;
    DELIVERED_HDR_INFO *dp;
    struct test_case {
	int     rec_type;
	const char *rec_content;
	int     want_found;
    };

    /*
     * This structure specifies the order of records that will be written to
     * a test queue file, and what we expect that delivered_hdr() will find.
     * It should not find the record that immediately follows REC_TYPE_CONT.
     */
    static const struct test_case test_cases[] = {
	REC_TYPE_CONT, "Delivered-To: one", FOUND,
	REC_TYPE_NORM, "Delivered-To: two", NOT_FOUND,
	REC_TYPE_NORM, "Delivered-To: three", FOUND,
	0,
    };
    const struct test_case *tp;
    int     got_found;

    var_drop_hdrs = mystrdup(DEF_DROP_HDRS);

    /*
     * Write queue file records to memory stream.
     */
#define REC_PUT_LIT(fp, type, lit) rec_put((fp), (type), (lit), strlen(lit))

    mem_bp = vstring_alloc(VSTREAM_BUFSIZE);
    if ((mem_fp = vstream_memopen(mem_bp, O_WRONLY)) == 0)
	ptest_fatal(t, "vstream_memopen O_WRONLY failed: %m");
    for (tp = test_cases; tp->rec_content != 0; tp++)
	REC_PUT_LIT(mem_fp, tp->rec_type, tp->rec_content);
    if (vstream_fclose(mem_fp))
	ptest_fatal(t, "vstream_fclose fail: %m");

    /*
     * Reopen the memory stream and populate the Delivered-To: cache.
     */
    if ((mem_fp = vstream_memopen(mem_bp, O_RDONLY)) == 0)
	ptest_fatal(t, "vstream_memopen O_RDONLY failed: %m");
    dp = delivered_hdr_init(mem_fp, 0, FOLD_ADDR_ALL);

    /*
     * Verify that what should be found will be found. XXX delivered_hdr()
     * assumes that Delivered-To: records fit in one queue file record.
     */
#define FOUND_OR_NOT(x) ((x) ? "found" : "not found")

    for (tp = test_cases; tp->rec_content != 0; tp++) {
	got_found =
	    delivered_hdr_find(dp, tp->rec_content + sizeof("Delivered-To:"));
	if (!got_found != !tp->want_found)
	    ptest_error(t, "delivered_hdr_find '%s': got '%s', want '%s'",
			tp->rec_content, FOUND_OR_NOT(got_found),
			FOUND_OR_NOT(tp->want_found));
    }
}

 /*
  * Test library adaptor.
  */
static PTEST_CASE ptestcases[] = {
    "test_delivered_hdr_find", test_delivered_hdr_find,
};

#include <ptest_main.h>
