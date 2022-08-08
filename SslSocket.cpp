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



