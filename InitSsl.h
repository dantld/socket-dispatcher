/*
 * InitSsl.h
 *
 *  Created on: 23 нояб., 2020 ел
 *      Author: dantld
 */

#ifndef INITSSL_H_
#define INITSSL_H_

#include <string>
#include <openssl/ssl.h>
#include <openssl/bio.h>

namespace dsockets {
namespace ssl {

extern SSL_CTX *sslGlobalCtx;
extern BIO *bio_err;
extern unsigned long sslError;

int createContext(
	const std::string& caFile,
	const std::string& crtFile,
	const std::string& keyFile,
	const std::string& password
	);

int createClientContext();

void dropContext();

}
}


#endif /* INITSSL_H_ */
