/*
 * SslSocket.cpp
 *
 *  Created on: 23 нояб., 2020 ел
 *      Author: dantld
 */

#include "SslSocket.h"
#include "InitSsl.h"

#include <openssl/bio.h>
#include <openssl/ssl.h>

namespace dsockets {

SslSocket::~SslSocket()
{
	SSL_free(_ssl);
}

SslSocket::SslSocket(Socket::Ptr socket) :
	Socket(socket->descriptor(),SocketType::SSL),
	_bio(BIO_new_socket(socket->descriptor(), BIO_NOCLOSE)),
	_ssl(SSL_new(dsockets::ssl::sslGlobalCtx))
{
	SSL_set_bio(_ssl, _bio, _bio);
	socket->protectDescriptor();
}

SslSocket::SslSocket(int descriptor) :
	Socket(descriptor,SocketType::SSL),
	_bio(BIO_new_socket(descriptor, 0)),
	_ssl(SSL_new(dsockets::ssl::sslGlobalCtx))
{
	SSL_set_bio(_ssl, _bio, _bio);
}

SslSocket::SslSocket(int descriptor,BIO *bio,SSL *ssl) :
	Socket(descriptor,SocketType::SSL),
	_bio(bio),
	_ssl(ssl)
{
}

} // dsockets



