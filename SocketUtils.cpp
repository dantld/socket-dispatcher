/*
 * Copyright (c) 2022 Daniyar Tleulin <daniyar.tleulin@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "SocketUtils.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>

#include "SslSocket.h"

namespace dsockets {

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

ssize_t read(Socket::Ptr clientSocket, void *buffer, size_t bufferSize, int flags)
{
	ssize_t retVal = 0;
	SslSocket *sslSocket = dynamic_cast<SslSocket*>(clientSocket.get());
	if(sslSocket) {
		retVal = BIO_read(sslSocket->bio(), buffer, bufferSize);
	} else {
		retVal = ::recv(clientSocket->descriptor(), buffer, bufferSize, flags);
	}
	return retVal;
}

ssize_t write(Socket::Ptr clientSocket, const std::string& message)
{
	ssize_t retVal = 0;
	SslSocket *sslSocket = dynamic_cast<SslSocket*>(clientSocket.get());
	if(sslSocket) {
		retVal = BIO_write(sslSocket->bio(), message.data(), message.size());
	} else {
		retVal = ::write(clientSocket->descriptor(), const_cast<char*>(message.data()), message.size());
	}
	return retVal;
}


} // utils
} // dsockets


