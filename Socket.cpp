/*
 * Socket.cpp
 *
 *  Created on: Nov 10, 2020
 *      Author: dantld
 */

#include "Socket.h"
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>

namespace dsockets {

Socket::~Socket() {
	close(_descriptor);
}

Socket::Ptr Socket::acceptConnection() {
	assert(0);
	return {};
}

namespace utils {

static struct cmsghdr *cmptr = NULL;
#define CONTROLLEN  CMSG_LEN(sizeof(int))

/*
 * Pass a file descriptor to another process.
 * If fd<0, then -fd is sent back instead as the error status.
 * TODO: need error diagnostic here.
 */
bool sendSocket(Socket::Ptr socketSender, Socket::Ptr socketToSend)
{
	int fd = socketSender->descriptor();
	int fd_to_send = socketToSend->descriptor();

    struct iovec    iov[1];
    struct msghdr   msg;
    char            buf[2]; /* send_fd()/recv_fd() 2-byte protocol */

    iov[0].iov_base = buf;
    iov[0].iov_len  = 2;
    msg.msg_iov     = iov;
    msg.msg_iovlen  = 1;
    msg.msg_name    = NULL;
    msg.msg_namelen = 0;
    if (fd_to_send < 0) {
        msg.msg_control    = NULL;
        msg.msg_controllen = 0;
        buf[1] = -fd_to_send;   /* nonzero status means error */
        if (buf[1] == 0)
            buf[1] = 1; /* -256, etc. would screw up protocol */
    } else {
        if (cmptr == NULL && (cmptr = (cmsghdr*)malloc(CONTROLLEN)) == NULL)
            return false;
        cmptr->cmsg_level  = SOL_SOCKET;
        cmptr->cmsg_type   = SCM_RIGHTS;
        cmptr->cmsg_len    = CONTROLLEN;
        msg.msg_control    = cmptr;
        msg.msg_controllen = CONTROLLEN;
        *(int *)CMSG_DATA(cmptr) = fd_to_send;     /* the fd to pass */
        buf[1] = 0;          /* zero status means OK */
    }
    buf[0] = 0;              /* null byte flag to recv_fd() */
    if (sendmsg(fd, &msg, 0) != 2)
        return false;
    return true;
}

} // utils

} // dsockets


