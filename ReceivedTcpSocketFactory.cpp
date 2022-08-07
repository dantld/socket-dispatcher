/*
 * ReceivedTcpSocketFactory.cpp
 *
 *  Created on: Nov 10, 2020
 *      Author: dantld
 */

#ifndef RECEIVEDTCPSOCKETFACTORY_CPP_
#define RECEIVEDTCPSOCKETFACTORY_CPP_

#include "SocketFactory.h"
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <malloc.h>
#include <errno.h>

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


#define err_sys(msg)  fprintf(stderr,"ERROR:  %s\n",msg)
#define err_ret(msg)  fprintf(stderr,"RETURN: %s\n",msg)
#define err_dump(msg) fprintf(stderr,"DUMP:   %s\n",msg)


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
           err_sys("recvmsg error");
       } else if (nr == 0) {
           err_ret("connection closed by server");
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
                   err_dump("message format error");
               status = *ptr & 0xFF;  /* prevent sign extension */
               if (status == 0) {
                   if (msg.msg_controllen != CONTROLLEN)
                       err_dump("status = 0 but no fd");
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
