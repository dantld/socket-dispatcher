/*
 * SslSocket.cpp
 *
 *  Created on: 23 нояб., 2020 ел
 *      Author: dantld
 */

#include "SslSocket.h"
#include <openssl/bio.h>

namespace dsockets {

SslSocket::~SslSocket()
{
	int retVal = BIO_closesocket(descriptor());
}

SslSocket::SslSocket(Socket::Ptr socket) :
	Socket(socket->descriptor(),SocketType::SSL),
	_bio(BIO_new_socket(socket->descriptor(), 0)),
	_ssl(nullptr)
{
}

SslSocket::SslSocket(int descriptor) :
	Socket(descriptor,SocketType::SSL),
	_bio(BIO_new_socket(descriptor, 0)),
	_ssl(nullptr)
{
}

SslSocket::SslSocket(int descriptor,BIO *bio,SSL *ssl) :
	Socket(descriptor,SocketType::SSL),
	_bio(_bio),
	_ssl(_ssl)
{
}

} // dsockets



