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

#include "SslSocket.h"
#include "SocketFactory.h"
#include "InitSsl.h"

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <iostream>

#include <openssl/ssl.h>
#include <openssl/bio.h>

#include "utils.h"

namespace dsockets {

namespace utility {

class SslClientSocket : public SslSocket {
public:
	SslClientSocket(
			int descriptor,
			BIO *bio,
			SSL *ssl
		) :
		SslSocket(descriptor,bio,ssl)
	{
	}
};

class TcpSslConnectSocketFactory : public SocketFactory {
	std::string _host;
	std::uint16_t _port;
	SSL_CTX      *_sslCtx;
public:
	TcpSslConnectSocketFactory(
		const std::string& host,
		std::uint16_t port,
		SSL_CTX *sslCtx
		) :
		_host(host),
		_port(port),
		_sslCtx(sslCtx)
		{}
	Socket::Ptr createSocket() override;
};

Socket::Ptr TcpSslConnectSocketFactory::createSocket()
{
	int error_number = 0;
	hostent *hostEntry = gethostbyname(_host.c_str());
	if(hostEntry == nullptr) {
		logger->error("Host name resolution failed: [{}]", error_number);
		return {};
	}
	if(hostEntry->h_length == 0) {
		logger->error("Host name resolution returns no addresses.");
		return {};
	}
	logger->info("{} -> {}",_host,inet_ntoa(*(in_addr*)(hostEntry->h_addr_list[0])));
	int sockfd = socket(AF_INET, SOCK_STREAM, 0 );
	if( sockfd == -1 ) {
		logger->error("Socket create error: {}", strerror(errno));
		return {};
	}
	sockaddr_in clientAddrIn;
	bzero(&clientAddrIn, sizeof(clientAddrIn));
	clientAddrIn.sin_family = AF_INET;
	memcpy( &clientAddrIn.sin_addr, reinterpret_cast<sockaddr*>(hostEntry->h_addr_list[0]), hostEntry->h_length);
	clientAddrIn.sin_port = htons(_port);
	if( connect(sockfd, (sockaddr*)&clientAddrIn, sizeof(clientAddrIn)) != 0) {
		close(sockfd);
		logger->error("Connect to host \"{}\", ({})", _host, inet_ntoa(*(in_addr*)(hostEntry->h_addr_list[0])));
		return {};
	}
	/* Connect the SSL socket */
	SSL *ssl = SSL_new(_sslCtx);
	BIO *sbio = BIO_new_socket(sockfd,BIO_NOCLOSE);
	SSL_set_bio(ssl,sbio,sbio);
	if( SSL_connect(ssl) <= 0 ) {
		close(sockfd);
		BIO_free(sbio);
		SSL_free(ssl);
		logger->error("SSL connect error");
		return {};
	}
	//if(require_server_auth)
	//check_cert(ssl,host);
	return std::make_shared<SslClientSocket>(sockfd, sbio, ssl);
}


SocketFactory::Ptr createTcpSslConnectSocketFactory(const std::string& host, std::uint16_t port)
{
	if(dsockets::ssl::sslGlobalCtx == nullptr) {
		throw std::runtime_error("SSL global context is NULL");
	}
	return std::make_shared<TcpSslConnectSocketFactory>(host,port,dsockets::ssl::sslGlobalCtx);
}

} // utility

} // dsockets


