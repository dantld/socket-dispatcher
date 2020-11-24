/*
 * SslSocket.h
 *
 *  Created on: 23 нояб., 2020 ел
 *      Author: dantld
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
