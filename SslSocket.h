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

#ifndef SSLSOCKET_H_
#define SSLSOCKET_H_

#include "Socket.h"
#include <openssl/bio.h>
#include <openssl/ssl.h>

namespace dsockets {

class SslSocket : public Socket
{
	BIO* _bio;
	SSL* _ssl;
public:
	SslSocket() = delete;
	~SslSocket();
	explicit SslSocket(Socket::Ptr socket);
	explicit SslSocket(int descriptor);
	explicit SslSocket(int descriptor,BIO *bio,SSL *ssl);

	BIO* bio() const noexcept { return _bio; }
	SSL* ssl() const noexcept { return _ssl; }
};

} // dsockets


#endif /* SSLSOCKET_H_ */
