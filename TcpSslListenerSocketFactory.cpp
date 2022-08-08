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

#include "SocketFactory.h"
#include <memory>
#include <stdexcept>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include "utils.h"
#include "InitSsl.h"

namespace dsockets {

namespace utility {

class TcpSslListenerSocket : public Socket {
	SSL_CTX *_sslCtx;
public:
	TcpSslListenerSocket(
		int descriptor,
		SSL_CTX *sslCtx
		) :
		Socket(descriptor, SocketType::TCP, true),
		_sslCtx(sslCtx)
		{
	}

	Socket::Ptr acceptConnection() override {
	    int newfd = 0, retVal = 0;
	    struct sockaddr_in peer_addr;
	    socklen_t peer_size = sizeof(peer_addr);
	    newfd = accept(descriptor(),(sockaddr*)&peer_addr,&peer_size);
	    if( newfd < 0 ) {
	        logger->error("accept failed: {}", strerror(errno));
	        return {};
	    }
	    logger->info("Incoming Tcp SSL Connection Has Accepted");
		BIO *sbio = BIO_new_socket(newfd, BIO_NOCLOSE);
		SSL *ssl = SSL_new(_sslCtx);
		SSL_set_bio(ssl, sbio, sbio);
		retVal = SSL_accept(ssl);
		if( retVal <= 0 ) {
	        logger->error("SSL accept error: {}", SSL_get_error(ssl,retVal));
	        return {};
		}
	    return std::make_shared<Socket>(newfd, SocketType::SSL, false);
	}
};

class TcpSslListenerSocketFactory : public SocketFactory {
	std::uint16_t _port;
	std::uint16_t _queueLength;
	SSL_CTX      *_sslCtx;
public:
	TcpSslListenerSocketFactory(std::uint16_t port,std::uint16_t queueLength,SSL_CTX *sslCtx)
		:
		_port(port),
		_queueLength(queueLength),
		_sslCtx(sslCtx)
		{}
	Socket::Ptr createSocket() override;
};

Socket::Ptr TcpSslListenerSocketFactory::createSocket()
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
    if( retVal == -1 ) { 
		logger->error("set reuse address: {}",strerror(errno));
	}

    retVal = bind( socketfd, (sockaddr*)&srv_addr, sizeof(srv_addr) );
    if(retVal < 0 ) return {};
    retVal = listen( socketfd, _queueLength);
    if(retVal < 0 ) return {};

	return std::make_shared<TcpSslListenerSocket>(socketfd,_sslCtx);
}

SocketFactory::Ptr createTcpSslListenerSocketFactory(std::uint16_t port, std::uint16_t queueLength)
{
	if(dsockets::ssl::sslGlobalCtx == nullptr) {
		throw std::runtime_error("SSL global context is NULL");
	}
	return std::make_shared<TcpSslListenerSocketFactory>(port,queueLength,dsockets::ssl::sslGlobalCtx);
}

} // utility
} // dsockets



