/*++
/* NAME
/*	cleanup_body_region 3
/* SUMMARY
/*	manage body content regions
/* SYNOPSIS
/*	#include "cleanup.h"
/*
/*	int	cleanup_body_region_start(state)
/*	CLEANUP_STATE *state;
/*
/*	int	cleanup_body_region_write(state, type, buf)
/*	CLEANUP_STATE *state;
/*	int	type;
/*	VSTRING	*buf;
/*
/*	int	cleanup_body_region_finish(state)
/*	CLEANUP_STATE *state;
/*
/*	void	cleanup_body_region_free(state)
/*	CLEANUP_STATE *state;
/* DESCRIPTION
/*	This module maintains queue file regions with body content.
/*	Regions are created on the fly, and can be reused multiple
/*	times. This module must not be called until the queue file
/*	is complete.
/*
/*	cleanup_body_region_start() performs initialization and
/*	sets the queue file write pointer to the start of the
/*	first body content segment.
/*
/*	cleanup_body_region_write() adds a queue file record to the
/*	current queue file. When the current queue file region fills
/*	up, some other region is reused or a new one is created.
/*
/*	cleanup_body_region_finish() makes some final adjustments
/*	after the last body content record is written.
/*
/*	cleanup_body_region_free() frees up memory that was allocated
/*	by cleanup_body_region_start() and cleanup_body_region_write().
/*
/*	Arguments:
/* .IP state
/*	Queue file and message processing state. This state is updated
/*	as records are processed and as errors happen.
/* .IP type
/*	Record type.
/* .IP buf
/*	Record content.
/* BUGS
/*	Currently, queue file region management is intertwined with
/*	body content management. Eventually the two should be
/*	decoupled, so that space freed up after body editing may
/*	be reused for header updates and vice versa.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <vstream.h>
#include <vstring.h>

/* Global library. */

#include <rec_type.h>
#include <record.h>

/* Application-specific. */

#include <cleanup.h>

#define LEN(s) VSTRING_LEN(s)

/* cleanup_body_region_alloc - create body content region */

static CLEANUP_BODY_REGION *cleanup_body_region_alloc(off_t start, off_t len)
{
    CLEANUP_BODY_REGION *rp;

    rp = (CLEANUP_BODY_REGION *) mymalloc(sizeof(*rp));
    rp->start = start;
    rp->len = len;
    rp->next = 0;

    return (rp);
}

/* cleanup_body_region_free - destroy all body content regions */

void    cleanup_body_region_free(CLEANUP_STATE *state)
{
    CLEANUP_BODY_REGION *rp;
    CLEANUP_BODY_REGION *next;

    for (rp = state->body_regions; rp != 0; rp = next) {
	next = rp->next;
	myfree((char *) rp);
    }
}

/* cleanup_body_region_start - rewrite body buffer pool */

int     cleanup_body_region_start(CLEANUP_STATE *state)
{
    const char *myname = "cleanup_body_region_write";

    /*
     * Calculate the payload size sans body.
     */
    state->cont_length = state->append_hdr_pt_target - state->data_offset;

    /*
     * Craft the first body region on the fly, from circumstantial evidence.
     */
    if (state->body_regions == 0)
	state->body_regions =
	    cleanup_body_region_alloc(state->append_hdr_pt_target,
			  state->xtra_offset - state->append_hdr_pt_target);

    /*
     * Select the first region and initialize the write position.
     */
    state->curr_body_region = state->body_regions;
    state->body_write_offs = state->curr_body_region->start;

    /*
     * Move the file write pointer to the start of the current region.
     */
    if (vstream_fseek(state->dst, state->body_write_offs, SEEK_SET) < 0) {
	msg_warn("%s: seek file %s: %m", myname, cleanup_path);
	return (-1);
    }
    return (0);
}

/* cleanup_body_region_write - add record to body buffer pool */

int     cleanup_body_region_write(CLEANUP_STATE *state, int rec_type,
				          VSTRING *buf)
{
    const char *myname = "cleanup_body_region_write";
    CLEANUP_BODY_REGION *rp = state->curr_body_region;
    off_t   used;
    ssize_t rec_len;
    off_t   start;

    if (msg_verbose)
	msg_info("%s: where %ld, buflen %ld region start %ld len %ld",
		 myname, (long) state->body_write_offs, (long) LEN(buf),
		 (long) rp->start, (long) rp->len);

    /*
     * Switch to the next body region if we filled up the current one (we
     * always append to an open-ended region). Besides space to write the
     * specified record, we need to leave space for a final pointer record
     * that will link this body region to the next region or to the content
     * terminator record.
     */
    REC_SPACE_NEED(LEN(buf), rec_len);
    if (rp->len > 0 && (used = state->body_write_offs - rp->start,
			rec_len + REC_TYPE_PTR_SIZE > rp->len - used)) {

	/*
	 * Allocate a new body region if we filled up the last one. A newly
	 * allocated region sits at the end of the queue file, and therefore
	 * it starts as open ended. We freeze the region size later.
	 * 
	 * Don't use fstat() to figure out where the queue file ends, in case
	 * file sizes have magic in them. Instead we seek to the end and then
	 * back to where we were. We're not switching body regions often, so
	 * this is not performance critical.
	 */
	if (rp->next == 0) {
	    if ((start = vstream_fseek(state->dst, (off_t) 0, SEEK_END)) < 0) {
		msg_warn("%s: seek file %s: %m", myname, cleanup_path);
		return (-1);
	    }
	    if (vstream_fseek(state->dst, state->body_write_offs, SEEK_SET) < 0) {
		msg_warn("%s: seek file %s: %m", myname, cleanup_path);
		return (-1);
	    }
	    rp->next = cleanup_body_region_alloc(start, 0);
	}

	/*
	 * Update the payload size and select the new body region.
	 */
	state->cont_length += state->body_write_offs - rp->start;
	state->curr_body_region = rp = rp->next;

	/*
	 * Connect the filled up body region to its successor. By design a
	 * region has always space for a final pointer record.
	 */
	if (msg_verbose)
	    msg_info("%s: link %ld -> %ld", myname,
		     (long) state->body_write_offs, (long) rp->start);
	rec_fprintf(state->dst, REC_TYPE_PTR, REC_TYPE_PTR_FORMAT,
		    (long) rp->start);
	if (vstream_fseek(state->dst, rp->start, SEEK_SET) < 0) {
	    msg_warn("%s: seek file %s: %m", myname, cleanup_path);
	    return (-1);
	}
    }

    /*
     * Finally, output the queue file record.
     */
    CLEANUP_OUT_BUF(state, REC_TYPE_NORM, buf);
    state->body_write_offs = vstream_ftell(state->dst);

    return (0);
}

/* cleanup_body_region_finish - wrap up body buffer pool */

int     cleanup_body_region_finish(CLEANUP_STATE *state)
{
    const char *myname = "cleanup_body_region_finish";
    CLEANUP_BODY_REGION *rp;

    /*
     * Link the last body region to the content terminator record.
     */
    rec_fprintf(state->dst, REC_TYPE_PTR, REC_TYPE_PTR_FORMAT,
		(long) state->xtra_offset);
    state->body_write_offs = vstream_ftell(state->dst);

    /*
     * Update the payload size.
     */
    rp = state->curr_body_region;
    state->cont_length += state->body_write_offs - rp->start;

    /*
     * Freeze the size of the last region if it is still open ended. The next
     * Milter application may append more header records, therefore we must
     * not assume that this region can grow further. Nor can we truncate the
     * queue file to the end of this region, as this region may be followed
     * by headers that were appended by an earlier Milter application.
     * 
     * XXX Eventually, split a partially-used region so that the remainder can
     * be returned to a free pool and reused for header updates.
     */
    if (rp->len == 0)
	rp->len = state->body_write_offs - rp->start;
    if (msg_verbose)
	msg_info("%s: freeze start %ld len %ld",
		 myname, (long) rp->start, (long) rp->len);

    return (CLEANUP_OUT_OK(state) ? 0 : -1);
}
