/* System  library. */

#include <sys_defs.h>
#include <stdlib.h>			/* smtp_weed_request  */

/* Utility  library. */

#include <msg.h>

/* Global library. */

#include <deliver_request.h>		/* smtp_rcpt_done */
#include <deliver_completed.h>		/* smtp_rcpt_done */
#include <sent.h>			/* smtp_rcpt_done */

/* Application-specific. */

#include <smtp.h>

/* smtp_rcpt_done - mark recipient as done or else */

void    smtp_rcpt_done(SMTP_STATE *state, const char *reply, RECIPIENT *rcpt)
{
    DELIVER_REQUEST *request = state->request;
    SMTP_SESSION *session = state->session;
    int     status;

    /*
     * Report success and delete the recipient from the delivery request.
     * Defer if the success can't be reported.
     */
    status = sent(DEL_REQ_TRACE_FLAGS(request->flags),
		  request->queue_id, rcpt->orig_addr,
		  rcpt->address, rcpt->offset,
		  session->namaddr,
		  request->arrival_time,
		  "%s", reply);
    if (status == 0)
	if (request->flags & DEL_REQ_FLAG_SUCCESS)
	    deliver_completed(state->src, rcpt->offset);
    rcpt->status = SMTP_RCPT_DROP;
    state->status |= status;
}

/* smtp_weed_request_callback - qsort callback */

static int smtp_weed_request_callback(const void *a, const void *b)
{
    return (((RECIPIENT *) a)->status - ((RECIPIENT *) b)->status);
}

/* smtp_weed_request - purge completed recipients from request */

int     smtp_weed_request(RECIPIENT_LIST *rcpt_list)
{
    RECIPIENT *rcpt;
    int     nrcpt;

    /*
     * Status codes one can expect to find: SMTP_RCPT_KEEP (try recipient
     * another time), SMTP_RCPT_DROP (remove recipient from request) and zero
     * (error: after delivery attempt, recipient status should be either KEEP
     * or DROP).
     */
    if (rcpt_list->len > 1)
	qsort((void *) rcpt_list->info, rcpt_list->len,
	      sizeof(rcpt_list->info), smtp_weed_request_callback);

    for (nrcpt = 0; nrcpt < rcpt_list->len; nrcpt++) {
	rcpt = rcpt_list->info + nrcpt;
	if (rcpt->status == SMTP_RCPT_KEEP)
	    rcpt->status = 0;
	if (rcpt->status == SMTP_RCPT_DROP)
	    break;
	else
	    msg_panic("smtp_weed_request: bad status: %d for <%s>",
		      rcpt->status, rcpt->address);
    }
    recipient_list_truncate(rcpt_list, nrcpt);
    return (nrcpt);
}
