
 /*
  * System library.
  */
#include <sys_defs.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>			/* ssscanf() */
#include <ctype.h>

 /*
  * Utility library.
  */
#include <msg.h>
#include <msg_vstream.h>
#include <vstring.h>
#include <vstream.h>
#include <stringops.h>

 /*
  * Global library.
  */
#include <been_here.h>
#include <record.h>
#include <rec_type.h>
#include <cleanup_user.h>
#include <mail_params.h>
#include <smtputf8.h>

 /*
  * Application-specific.
  */
#include <cleanup.h>

 /*
  * Stubs for parameter dependencies.
  */
int     var_delay_warn_time = 0;
int     var_dup_filter_limit = DEF_DUP_FILTER_LIMIT;
char   *var_remote_rwr_domain = DEF_REM_RWR_DOMAIN;
int     var_qattr_count_limit = DEF_QATTR_COUNT_LIMIT;
VSTRING *cleanup_strip_chars = 0;
MILTERS *cleanup_milters = 0;
VSTRING *cleanup_trace_path = 0;
MAPS   *cleanup_virt_alias_maps = 0;
char   *cleanup_path = "fixed";

 /*
  * Stubs for cleanup_message.c dependencies. TODO(wietse) replace function
  * stubs with mocks that can have expectations and that can report
  * unexpected calls.
  */
void    cleanup_message(CLEANUP_STATE *state, int type, const char *buf,
			        ssize_t len)
{
    msg_panic("cleanup_message");
}

 /*
  * Stubs for cleanup_milter.c dependencies.
  */
void    cleanup_milter_receive(CLEANUP_STATE *state, int count)
{
    msg_panic("cleanup_milter_receive");
}

void    cleanup_milter_emul_mail(CLEANUP_STATE *state, MILTERS *milters,
				         const char *sender)
{
    msg_panic("cleanup_milter_emul_mail");
}

void    cleanup_milter_emul_rcpt(CLEANUP_STATE *state, MILTERS *milters,
				         const char *recipient)
{
    msg_panic("cleanup_milter_emul_rcpt");
}

 /*
  * Stubs for cleanup_addr.c dependencies.
  */
off_t   cleanup_addr_sender(CLEANUP_STATE *state, const char *addr)
{
    msg_panic("cleanup_addr_sender");
}

void    cleanup_addr_recipient(CLEANUP_STATE *state, const char *addr)
{
    msg_panic("cleanup_addr_recipient");
}

 /*
  * Stubs for cleanup_region.c dependencies.
  */
void    cleanup_region_done(CLEANUP_STATE *state)
{
}

 /*
  * Tests and test cases.
  */
typedef struct TEST_CASE {
    const char *label;			/* identifies test case */
    int     (*action) (const struct TEST_CASE *);
} TEST_CASE;

#define PASS	1
#define FAIL	0

static int overrides_size_fields(const TEST_CASE *tp)
{

    /*
     * Generate one SIZE record test payload.
     */
    VSTRING *input_buf = vstring_alloc(100);

    vstring_sprintf(input_buf, REC_TYPE_SIZE_FORMAT,
		    (REC_TYPE_SIZE_CAST1) ~ 0,	/* message segment size */
		    (REC_TYPE_SIZE_CAST2) ~ 0,	/* content offset */
		    (REC_TYPE_SIZE_CAST3) ~ 0,	/* recipient count */
		    (REC_TYPE_SIZE_CAST4) ~ 0,	/* qmgr options */
		    (REC_TYPE_SIZE_CAST5) ~ 0,	/* content length */
		    (REC_TYPE_SIZE_CAST6) SOPT_FLAG_ALL);	/* sendopts */

    /*
     * Instantiate CLEANUP_STATE, and save information that isn't expected to
     * change. We only need to save simple-type CLEANUP_STATE fields that
     * correspond with SIZE record fields.
     */
    CLEANUP_STATE *state = cleanup_state_alloc((VSTREAM *) 0);
    CLEANUP_STATE saved_state = *state;

    /*
     * Process the test SIZE record payload, clear some bits from the
     * sendopts field, and write an all-zeroes preliminary SIZE record.
     */
    VSTRING *output_stream_buf = vstring_alloc(100);

    if ((state->dst = vstream_memopen(output_stream_buf, O_WRONLY)) == 0) {
	msg_warn("vstream_memopen(output_stream_buf, O_WRONLY): %m");
	return (FAIL);
    }
    cleanup_envelope(state, REC_TYPE_SIZE, vstring_str(input_buf),
		     VSTRING_LEN(input_buf));
    if (state->errs != CLEANUP_STAT_OK) {
	msg_warn("cleanup_envelope: got: '%s', want: '%s'",
		 cleanup_strerror(state->errs),
		 cleanup_strerror(CLEANUP_STAT_OK));
	return (FAIL);
    }
    vstring_free(input_buf);
    input_buf = 0;

    /*
     * Overwrite the SIZE record with an updated version that includes the
     * modified sendopts field.
     */
    cleanup_final(state);
    if (state->errs != CLEANUP_STAT_OK) {
	msg_warn("cleanup_final: got: '%s', want: '%s'",
		 cleanup_strerror(state->errs),
		 cleanup_strerror(CLEANUP_STAT_OK));
	return (FAIL);
    }
    (void) vstream_fclose(state->dst);
    state->dst = 0;

    /*
     * Read the final SIZE record content. This normally happens in the queue
     * manager, and in the pickup daemon after a message is re-queued.
     */
    VSTREAM *fp;

    if ((fp = vstream_memopen(output_stream_buf, O_RDONLY)) == 0) {
	msg_warn("vstream_memopen(output_stream_buf, O_RDONLY): %m");
	return (FAIL);
    }
    VSTRING *got_size_payload = vstring_alloc(VSTRING_LEN(output_stream_buf));
    int     got_rec_type;

    if ((got_rec_type = rec_get(fp, got_size_payload, 0)) != REC_TYPE_SIZE) {
	msg_warn("rec_get: got: %s, want: %s",
		 rec_type_name(got_rec_type), rec_type_name(REC_TYPE_SIZE));
	return (FAIL);
    }
    (void) vstream_fclose(fp);
    vstring_free(output_stream_buf);

    /*
     * Compare the stored SIZE record content against the expected content.
     * We expect that the fields for data_size, data_offset, rcpt_count,
     * qmgr_opts, and cont_length, are consistent with the saved
     * CLEANUP_STATE, and we expect to see a specific value for the sendopts
     * field that was made by cleanup_envelope().
     */
    int     got_conv;
    long    data_size, data_offset, cont_length;
    int     rcpt_count, qmgr_opts, sendopts;

    if ((got_conv = sscanf(vstring_str(got_size_payload), "%ld %ld %d %d %ld %d",
			   &data_size, &data_offset, &rcpt_count, &qmgr_opts,
			   &cont_length, &sendopts)) != 6) {
	msg_warn("sscanf SIZE record fields: got: %d, want 6", got_conv);
	return (FAIL);
    }
    if (data_size != saved_state.xtra_offset - saved_state.data_offset) {
	msg_warn("SIZE.data_size: got %ld, want: %ld", (long) data_size,
		 (long) (saved_state.xtra_offset - saved_state.data_offset));
	return (FAIL);
    }
    if (data_offset != saved_state.data_offset) {
	msg_warn("SIZE.data_offset: got %ld, want: %ld", (long) data_offset,
		 (long) saved_state.data_offset);
	return (FAIL);
    }
    if (rcpt_count != saved_state.rcpt_count) {
	msg_warn("SIZE.rcpt_count: got: %d, want: %d", rcpt_count,
		 (int) saved_state.rcpt_count);
	return (FAIL);
    }
    if (qmgr_opts != saved_state.qmgr_opts) {
	msg_warn("SIZE.qmgr_opts: got: %d, want: %d", qmgr_opts,
		 saved_state.qmgr_opts);
	return (FAIL);
    }
    if (cont_length != saved_state.cont_length) {
	msg_warn("SIZE.cont_length: got %ld, want: %ld", (long) cont_length,
		 (long) saved_state.cont_length);
	return (FAIL);
    }
    if (sendopts != (SOPT_FLAG_ALL & ~SOPT_FLAG_DERIVED)) {
	msg_warn("SIZE.sendopts: got: 0x%x, want: 0x%x",
		 sendopts, SOPT_FLAG_ALL & ~SOPT_FLAG_DERIVED);
	return (FAIL);
    }

    /*
     * Cleanup.
     */
    vstring_free(got_size_payload);
    cleanup_state_free(state);
    return (PASS);
}

static const TEST_CASE test_cases[] = {
    {"overrides_size_fields",
	overrides_size_fields,
    },
    {0},
};

int     main(int argc, char **argv)
{
    const TEST_CASE *tp;
    int     pass = 0;
    int     fail = 0;

    /* XXX How to avoid linking in mail_params.o? */
    var_line_limit = DEF_LINE_LIMIT;

    msg_vstream_init(sane_basename((VSTRING *) 0, argv[0]), VSTREAM_ERR);

    for (tp = test_cases; tp->label != 0; tp++) {
	msg_info("RUN  %s", tp->label);
	if (tp->action(tp) != PASS) {
	    fail++;
	    msg_info("FAIL %s", tp->label);
	} else {
	    msg_info("PASS %s", tp->label);
	    pass++;
	}
    }
    msg_info("PASS=%d FAIL=%d", pass, fail);
    exit(fail != 0);
}
