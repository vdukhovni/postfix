#include "sys_defs.h"
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stropts.h>
#include "iostuff.h"

#include "msg.h"
#include "msg_vstream.h"
#include "listen.h"

#define FIFO	"/tmp/test-fifo"

static const char *progname;

static  print_fstat(int fd)
{
    struct stat st;

    if (fstat(fd, &st) < 0)
	msg_fatal("fstat: %m");
    vstream_printf("fd	%d\n", fd);
    vstream_printf("dev	%d\n", st.st_dev);
    vstream_printf("ino	%d\n", st.st_ino);
    vstream_fflush(VSTREAM_OUT);
}

static NORETURN usage(void)
{
    msg_fatal("usage: %s [-p] [-n count] [-v]", progname);
}

main(int argc, char **argv)
{
    struct strrecvfd fdinfo;
    int     server_fd;
    int     client_fd;
    int     print_fstats = 0;
    int     count = 1;
    int     ch;
    int     i;

    progname = argv[0];
    msg_vstream_init(argv[0], VSTREAM_ERR);

    /*
     * Parse JCL.
     */
    while ((ch = GETOPT(argc, argv, "pn:v")) > 0) {
	switch (ch) {
	default:
	    usage();
	case 'p':
	    print_fstats = 1;
	    break;
	case 'n':
	    if ((count = atoi(optarg)) < 1)
		usage();
	    break;
	case 'v':
	    msg_verbose++;
	    break;
	}
    }
    server_fd = fifo_listen(FIFO, 0600, NON_BLOCKING);
    if (readable(server_fd))
	msg_fatal("server fd is readable after create");

    /*
     * Connect in client.
     */
    if ((client_fd = open(FIFO, O_RDWR, NON_BLOCKING)) < 0)
	msg_fatal("open %s as client: %m", FIFO);
    if (readable(server_fd))
	msg_warn("server fd is readable after client open");
    if (print_fstats)
	print_fstat(0);
	if (ioctl(client_fd, I_RECVFD, &fdinfo) < 0)
	    msg_fatal("receive fd: %m");
    for (i = 0; i < count; i++) {
	msg_info("send attempt %d", i);
	while (!writable(client_fd))
	    msg_info("wait for client fd to become writable");
	if (ioctl(client_fd, I_SENDFD, 0) < 0)
	    msg_fatal("send fd to server: %m");
    }
    if (close(client_fd) < 0)
	msg_fatal("close client fd: %m");

    /*
     * Accept in server.
     */
    for (i = 0; i < count; i++) {
	msg_info("receive attempt %d", i);
	while (!readable(server_fd))
	    msg_info("wait for server fd to become writable");
	if (ioctl(server_fd, I_RECVFD, &fdinfo) < 0)
	    msg_fatal("receive fd: %m");
	if (print_fstats)
	    print_fstat(fdinfo.fd);
	if (close(fdinfo.fd) < 0)
	    msg_fatal("close received fd: %m");
    }
}
