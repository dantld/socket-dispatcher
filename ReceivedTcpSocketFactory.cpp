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

#ifndef RECEIVEDTCPSOCKETFACTORY_CPP_
#define RECEIVEDTCPSOCKETFACTORY_CPP_

#include "SocketFactory.h"
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <malloc.h>
#include <errno.h>
#include "utils.h"

namespace dsockets {
namespace utility {

class TcpSocket : public Socket {
public:
	TcpSocket( int descriptor ) : Socket(descriptor, SocketType::TCP) {}
};

class ReceivedTcpSocketFactory : public SocketFactory {
	Socket::Ptr _unixSocket;
public:
	ReceivedTcpSocketFactory(Socket::Ptr unixSocket) :
		_unixSocket(unixSocket)
	{}
	Socket::Ptr createSocket() override;
};


Socket::Ptr ReceivedTcpSocketFactory::createSocket()
{
   int             newfd, nr, status;
   char            *ptr;
   const           int MAXLINE=128;
   char            buf[MAXLINE];
   struct iovec    iov[1];
   struct msghdr   msg;
   /* size of control buffer to send/recv one file descriptor */
   const int CONTROLLEN  CMSG_LEN(sizeof(int));
   static struct cmsghdr   *cmptr = NULL;      /* malloc'ed first time */

   status = -1;
   for ( ; ; ) {
       iov[0].iov_base = buf;
       iov[0].iov_len  = sizeof(buf);
       msg.msg_iov     = iov;
       msg.msg_iovlen  = 1;
       msg.msg_name    = NULL;
       msg.msg_namelen = 0;
       if (cmptr == NULL && (cmptr = (cmsghdr*)malloc(CONTROLLEN)) == NULL)
           return {};
       msg.msg_control    = cmptr;
       msg.msg_controllen = CONTROLLEN;
       if ((nr = recvmsg(_unixSocket->descriptor(), &msg, 0)) < 0) {
           logger->error("recvmsg error");
       } else if (nr == 0) {
           logger->error("connection closed by server");
           return {};
       }
       /*
        * See if this is the final data with null & status.  Null
        * is next to last byte of buffer; status byte is last byte.
        * Zero status means there is a file descriptor to receive.
        */
       for (ptr = buf; ptr < &buf[nr]; ) {
           if (*ptr++ == 0) {
               if (ptr != &buf[nr-1])
                   logger->error("message format error");
               status = *ptr & 0xFF;  /* prevent sign extension */
               if (status == 0) {
                   if (msg.msg_controllen != CONTROLLEN)
                       logger->error("status = 0 but no fd");
                   newfd = *(int *)CMSG_DATA(cmptr);
               } else {
                   newfd = -status;
               }
               nr -= 2;
           }
        }
        if (status >= 0) {
        	if(newfd > 0) {
        		return std::make_shared<TcpSocket>(newfd);
        	} else {
        		return {};
        	}
        }
   }
   return {};
}


SocketFactory::Ptr createReceivedTcpSocketFactory(Socket::Ptr unixSocket)
{
	return std::make_shared<ReceivedTcpSocketFactory>(unixSocket);
}

} // utility
} // socket

#endif /* RECEIVEDTCPSOCKETFACTORY_CPP_ */
