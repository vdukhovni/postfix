#include <sys/types.h>
#include <sys/epoll.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int     main(int argc, char **argv)
{
    int     epoll_handle;

    if ((epoll_handle = epoll_create(1)) < 0) {
	perror("epoll_create");
	exit(1);
    }
    exit(0);
}
