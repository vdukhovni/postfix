/* base64decode - transform base 64 data to printable form */

/* System library. */

#include <sys_defs.h>
#include <ctype.h>

/* SASL library. */

#include <sasl.h>
#include <saslutil.h>

/* Utility library. */

#include <vstring.h>
#include <vstream.h>
#include <vstring_vstream.h>
#include <msg.h>
#include <msg_vstream.h>

/* Application-specific. */

#define STR(x)	vstring_str(x)
#define LEN(x)	VSTRING_LEN(x)

#define UCHAR(x) ((unsigned char *) (x))

static VSTRING *escape(VSTRING *escaped, char *input, int len)
{
    char   *cp;
    unsigned ch;

    VSTRING_RESET(escaped);
    for (cp = input; cp < input + len; cp++) {
	ch = *UCHAR(cp);
	if (ISASCII(ch) && ISPRINT(ch))
	    VSTRING_ADDCH(escaped, ch);
	else
	    vstring_sprintf_append(escaped, "\\%03o", ch);
    }
    VSTRING_TERMINATE(escaped);
    return (escaped);
}

int     main(int unused_argc, char **argv)
{
    VSTRING *input = vstring_alloc(100);
    VSTRING *escaped = vstring_alloc(100);
    VSTRING *result = vstring_alloc(100);
    int     len;

    msg_vstream_init(argv[0], VSTREAM_ERR);

    while (vstring_get_nonl(input, VSTREAM_IN) != VSTREAM_EOF) {
	VSTRING_SPACE(result, LEN(input));
	if (sasl_decode64(STR(input), LEN(input),
			  STR(result), &len) != SASL_OK)
	    msg_fatal("malformed input");
	vstream_printf("%s\n", STR(escape(escaped, STR(result), len)));
	vstream_fflush(VSTREAM_OUT);
    }
    return (0);
}
