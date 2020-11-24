/*
 * TcpSslConnectSocketFactory.cpp
 *
 *  Created on: 24 нояб., 2020 ел
 *      Author: dantld
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

namespace dsockets {

namespace utility {

class SslClientSocket : public SslSocket {
	BIO *_bio;
	SSL *_ssl;
public:
	SslClientSocket(
			int descriptor,
			BIO *bio,
			SSL *ssl
		) :
		SslSocket(descriptor,_bio,_ssl)
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
		std::cerr << "Host name resolution failed: [" << error_number << "]" << std::endl;
		return {};
	}
	if(hostEntry->h_length == 0) {
		std::cerr << "Host name resolution returns no addresses." << std::endl;
		return {};
	}
	std::cerr << _host << " -> " << inet_ntoa(*(in_addr*)(hostEntry->h_addr_list[0])) << std::endl;
	int sockfd = socket(AF_INET, SOCK_STREAM, 0 );
	if( sockfd == -1 ) {
		std::cerr << "Socket create error: " << strerror(errno) << std::endl;
		return {};
	}
	sockaddr_in clientAddrIn;
	bzero(&clientAddrIn, sizeof(clientAddrIn));
	clientAddrIn.sin_family = AF_INET;
	memcpy( &clientAddrIn.sin_addr, reinterpret_cast<sockaddr*>(hostEntry->h_addr_list[0]), hostEntry->h_length);
	clientAddrIn.sin_port = htons(_port);
	if( connect(sockfd, (sockaddr*)&clientAddrIn, sizeof(clientAddrIn)) != 0) {
		close(sockfd);
		std::cerr << "Connect to host \"" << _host << "\" (" << inet_ntoa(*(in_addr*)(hostEntry->h_addr_list[0])) << ") failed." << std::endl;
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
		std::cerr << "SSL connect error" << std::endl;
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


