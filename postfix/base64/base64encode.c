/* base64encode - transform printable form to base 64 data */

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

static VSTRING *unescape(VSTRING *unescaped, VSTRING *input)
{
    char   *cp = STR(input);
    int     ch;
    int     oval;
    int     i;

    VSTRING_RESET(unescaped);
    while ((ch = *UCHAR(cp++)) != 0) {
	switch (ch) {
	case '\\':
	    oval = 0;
	    for (oval = 0, i = 0; i < 3 && (ch = *UCHAR(cp)) != 0; i++) {
		if (!ISDIGIT(ch) || ch == '8' || ch == '9')
		    break;
		oval = (oval << 3) | (ch - '0');
		cp++;
	    }
	    VSTRING_ADDCH(unescaped, oval);
	    break;
	default:
	    VSTRING_ADDCH(unescaped, ch);
	    break;
	}
    }
    VSTRING_TERMINATE(unescaped);
    return (unescaped);
}

int     main(int unused_argc, char **argv)
{
    VSTRING *input = vstring_alloc(100);
    VSTRING *unescaped = vstring_alloc(100);
    VSTRING *result = vstring_alloc(100);
    int     result_len;
    int     len;

    msg_vstream_init(argv[0], VSTREAM_ERR);

    while (vstring_get_nonl(input, VSTREAM_IN) != VSTREAM_EOF) {
	unescape(unescaped, input);
	result_len = ((LEN(unescaped) + 2) / 3) * 4 + 1;
	VSTRING_SPACE(result, result_len);
	if (sasl_encode64(STR(unescaped), LEN(unescaped), STR(result),
			  result_len, &len) != SASL_OK)
	    msg_panic("sasl_encode64 botch");
	vstream_printf("%.*s\n", len, STR(result));
	vstream_fflush(VSTREAM_OUT);
    }
    return (0);
}
