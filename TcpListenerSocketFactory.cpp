/*
 * TcpListenerSocketFactory.cpp
 *
 *  Created on: Nov 13, 2020
 *      Author: dantld
 */
#include "SocketFactory.h"
#include <memory>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <errno.h>
#include "utils.h"

namespace dsockets {

namespace utility {

class TcpListenerSocket : public Socket {
public:
	TcpListenerSocket(int descriptor) : Socket(descriptor, SocketType::TCP, true) {
	}

	Socket::Ptr acceptConnection() override {
	    int newfd = 0;
	    struct sockaddr_in peer_addr;
	    socklen_t peer_size = sizeof(peer_addr);
	    newfd = accept(descriptor(),(sockaddr*)&peer_addr,&peer_size);
	    if( newfd < 0 ) {
	        fprintf(stderr,"accept failed: %s\n", strerror(errno));
	        return {};
	    }
	    printf("Incoming Tcp Connection Has Accepted\n");
	    return std::make_shared<Socket>(newfd, SocketType::TCP, false);
	}
};

class TcpListenerSocketFactory : public SocketFactory {
	std::uint16_t _port;
	std::uint16_t _queueLength;
public:
	TcpListenerSocketFactory(std::uint16_t port,std::uint16_t queueLength)
		:
		_port(port),
		_queueLength(queueLength)
		{}
	Socket::Ptr createSocket() override;
};

Socket::Ptr TcpListenerSocketFactory::createSocket()
{
    int retVal;
    int socketfd;
    socketfd = socket(AF_INET,SOCK_STREAM,0);
    if(socketfd < 0 ) return {};
    struct sockaddr_in srv_addr;
    memset( &srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port   = htons(_port);
    srv_addr.sin_addr.s_addr = INADDR_ANY;

    int sockopt = 1;
    retVal = setsockopt( socketfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(int) );
    if( retVal == -1 ) { perror("set reuse address failed"); }

    retVal = bind( socketfd, (sockaddr*)&srv_addr, sizeof(srv_addr) );
    if(retVal < 0 ) return {};
    retVal = listen( socketfd, _queueLength);
    if(retVal < 0 ) return {};

	return std::make_shared<TcpListenerSocket>(socketfd);
}

SocketFactory::Ptr createTcpListenerSocketFactory(std::uint16_t port, std::uint16_t queueLength)
{
	return std::make_shared<TcpListenerSocketFactory>(port,queueLength);
}

} // utility
} // dsockets
