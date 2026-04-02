/*++
/* NAME
/*	match_addr 3
/* SUMMARY
/*	matchers for network address information
/* SYNOPSIS
/*	#include <match_addr.h>
/*
/*	int	eq_addrinfo(
/*	PTEST_CTX *t,
/*	const char *what,
/*	struct addrinfo got,
/*	struct addrinfo want)
/*
/*	int	eq_sockaddr(PTEST_CTX *t,
/*	const char *what,
/*	const struct sockaddr *got,
/*	size_t	gotlen,
/*	const struct sockaddr *want,
/*	size_t wantlen)
/* DESCRIPTION
/*	The functions described here are safe macros that include
/*	call-site information (file name, line number) that may be
/*	used in error messages.
/*
/*	eq_addrinfo() compares two struct addrinfo linked lists,
/*	and eq_sockaddr() compares two struct sockaddr instances.
/*	Both functions return whether their arguments contain the
/*	same values.  If the t argument is not null, both functions
/*	will report values that differ with ptest_error()).
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	Google, Inc.
/*	111 8th Avenue
/*	New York, NY 10011, USA
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

 /*
  * Test library.
  */
#include <ptest.h>
#include <match_basic.h>
#include <match_addr.h>
#include <addrinfo_to_string.h>

#define STR	vstring_str

/* _eq_addrinfo - match struct addrinfo instances */

int     _eq_addrinfo(const char *file, int line,
		             PTEST_CTX *t, const char *what,
		             const struct addrinfo *got,
		             const struct addrinfo *want)
{
    if (got == 0 && want == 0)
	return (1);
    if (got == 0 || want == 0) {
	if (t) {
	    VSTRING *buf = vstring_alloc(100);

	    ptest_error(t, "%s:%d %s: got %s, want %s",
			file, line, what, got ?
			append_addrinfo_to_string(buf, got) : "(null)",
			want ?
			append_addrinfo_to_string(buf, want) : "(null)");
	    vstring_free(buf);
	}
	return (0);
    }
    return (_eq_flags(file, line, t, "ai_flags", got->ai_flags, want->ai_flags,
		      ai_flags_to_string)
	    && _eq_enum(file, line, t, "ai_family", got->ai_family,
			want->ai_family, af_to_string)
	    && _eq_enum(file, line, t, "ai_socktype", got->ai_socktype,
			want->ai_socktype, socktype_to_string)
	    && _eq_enum(file, line, t, "ai_protocol",
			got->ai_protocol, want->ai_protocol,
			ipprotocol_to_string)
	    && _eq_size_t(file, line, t, "ai_addrlen",
			  got->ai_addrlen, want->ai_addrlen)
	    && _eq_sockaddr(file, line, t, "ai_addr", got->ai_addr,
			    got->ai_addrlen, want->ai_addr, want->ai_addrlen)
	    && _eq_addrinfo(file, line, t, what, got->ai_next,
			    want->ai_next));
}

/* _eq_sockaddr - sockaddr matcher */

int     _eq_sockaddr(const char *file, int line,
		             PTEST_CTX *t, const char *what,
		             const struct sockaddr *got, size_t gotlen,
		             const struct sockaddr *want, size_t wantlen)
{
    if (got == 0 && want == 0)
	return (1);
    if (got == 0 || want == 0) {
	if (t) {
	    VSTRING *got_buf = vstring_alloc(100);
	    VSTRING *want_buf = vstring_alloc(100);

	    ptest_error(t, "%s:%d %s: got %s, want %s",
			file, line, what,
			got ? sockaddr_to_string(got_buf, got, gotlen)
			: "(null)",
			want ? sockaddr_to_string(want_buf, want, wantlen)
			: "(null)");
	    vstring_free(want_buf);
	    vstring_free(got_buf);
	}
	return (0);
    }
    if (!_eq_enum(file, line, (PTEST_CTX *) 0, "struct sockaddr address family",
		  got->sa_family, want->sa_family, af_to_string)
	|| !_eq_size_t(file, line, (PTEST_CTX *) 0, "struct sockaddr length",
		       gotlen, wantlen)
	|| memcmp(got, want, gotlen) != 0) {
	VSTRING *got_buf = vstring_alloc(100);
	VSTRING *want_buf = vstring_alloc(100);

	ptest_error(t, "%s:%d %s: got %s, want %s",
		    file, line, what,
		    sockaddr_to_string(got_buf, got, gotlen),
		    sockaddr_to_string(want_buf, want, wantlen));
	vstring_free(want_buf);
	vstring_free(got_buf);
	return (0);
    }
    return (1);
}
