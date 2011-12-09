 /*
  * Proof-of-concept test program. Create, update or read a database. When
  * the input is a name=value pair, the database is updated, otherwise the
  * program assumes that the input specifies a lookup key and prints the
  * corresponding value.
  */

/* System library. */

#include <sys_defs.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

/* Utility library. */

#include <msg.h>
#include <stringops.h>
#include <vstring.h>
#include <vstream.h>
#include <msg_vstream.h>
#include <vstring_vstream.h>
#include <dict.h>

static NORETURN usage(char *myname)
{
    msg_fatal("usage: %s type:file read|write|create [fold] [sync]", myname);
}

void    dict_test(int argc, char **argv)
{
    VSTRING *keybuf = vstring_alloc(1);
    VSTRING *inbuf = vstring_alloc(1);
    DICT   *dict;
    char   *dict_name;
    int     open_flags;
    char   *bufp;
    char   *cmd;
    const char *key;
    const char *value;
    int     ch;
    int     dict_flags = DICT_FLAG_LOCK | DICT_FLAG_DUP_REPLACE;
    int     n;

    signal(SIGPIPE, SIG_IGN);

    msg_vstream_init(argv[0], VSTREAM_ERR);
    while ((ch = GETOPT(argc, argv, "v")) > 0) {
	switch (ch) {
	default:
	    usage(argv[0]);
	case 'v':
	    msg_verbose++;
	    break;
	}
    }
    optind = OPTIND;
    if (argc - optind < 2)
	usage(argv[0]);
    if (strcasecmp(argv[optind + 1], "create") == 0)
	open_flags = O_CREAT | O_RDWR | O_TRUNC;
    else if (strcasecmp(argv[optind + 1], "write") == 0)
	open_flags = O_RDWR;
    else if (strcasecmp(argv[optind + 1], "read") == 0)
	open_flags = O_RDONLY;
    else
	msg_fatal("unknown access mode: %s", argv[2]);
    for (n = 2; argv[optind + n]; n++) {
	if (strcasecmp(argv[optind + 2], "fold") == 0)
	    dict_flags |= DICT_FLAG_FOLD_ANY;
	else if (strcasecmp(argv[optind + 2], "sync") == 0)
	    dict_flags |= DICT_FLAG_SYNC_UPDATE;
	else
	    usage(argv[0]);
    }
    dict_name = argv[optind];
    dict = dict_open(dict_name, open_flags, dict_flags);
    dict_register(dict_name, dict);
    while (vstring_fgets_nonl(inbuf, VSTREAM_IN)) {
	bufp = vstring_str(inbuf);
	if (!isatty(0)) {
	    vstream_printf("> %s\n", bufp);
	    vstream_fflush(VSTREAM_OUT);
	}
	if (*bufp == '#')
	    continue;
	if ((cmd = mystrtok(&bufp, " ")) == 0) {
	    vstream_printf("usage: del key|get key|put key=value|first|next\n");
	    vstream_fflush(VSTREAM_OUT);
	    continue;
	}
	if (dict_changed_name())
	    msg_warn("dictionary has changed");
	key = *bufp ? vstring_str(unescape(keybuf, mystrtok(&bufp, " ="))) : 0;
	value = mystrtok(&bufp, " =");
	if (strcmp(cmd, "del") == 0 && key && !value) {
	    if (dict_del(dict, key))
		vstream_printf("%s: not found\n", key);
	    else
		vstream_printf("%s: deleted\n", key);
	} else if (strcmp(cmd, "get") == 0 && key && !value) {
	    if ((value = dict_get(dict, key)) == 0) {
		vstream_printf("%s: %s\n", key,
			       dict_errno == DICT_ERR_RETRY ?
			       "soft error" : "not found");
	    } else {
		vstream_printf("%s=%s\n", key, value);
	    }
	} else if (strcmp(cmd, "put") == 0 && key && value) {
	    /* XXX dict_put returns void, so dict_memcache sets dict_errno. */
	    dict_errno = 0;
	    dict_put(dict, key, value);
	    if (dict_errno)
		vstream_printf("%s: soft error\n", key);
	    else
		vstream_printf("%s=%s\n", key, value);
	} else if (strcmp(cmd, "first") == 0 && !key && !value) {
	    if (dict_seq(dict, DICT_SEQ_FUN_FIRST, &key, &value) == 0)
		vstream_printf("%s=%s\n", key, value);
	    else
		vstream_printf("%s\n",
			       dict_errno == DICT_ERR_RETRY ?
			       "soft error" : "not found");
	} else if (strcmp(cmd, "next") == 0 && !key && !value) {
	    if (dict_seq(dict, DICT_SEQ_FUN_NEXT, &key, &value) == 0)
		vstream_printf("%s=%s\n", key, value);
	    else
		vstream_printf("%s\n",
			       dict_errno == DICT_ERR_RETRY ?
			       "soft error" : "not found");
	} else {
	    vstream_printf("usage: del key|get key|put key=value|first|next\n");
	}
	vstream_fflush(VSTREAM_OUT);
    }
    vstring_free(keybuf);
    vstring_free(inbuf);
    dict_close(dict);
}
