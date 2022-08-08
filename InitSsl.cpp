/*
 * InitSssl.cpp
 *
 *  Created on: 23 нояб., 2020 ел
 *      Author: dantld
 */

#include "InitSsl.h"
#include <string>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <iostream>

namespace dsockets {
namespace ssl {

SSL_CTX *sslGlobalCtx = nullptr;
BIO *bio_err = nullptr;
unsigned long sslError = 0;

namespace {
	std::string keyFilePassword;
	int pass_user_password_cb(char *buf, int size, int rwflag, void *userdata)
	{
		std::cout << "IN pass_user_password_cb: \""
				<< keyFilePassword
				<< "\" data size="
				<< keyFilePassword.size()
				<< " try to open file: "
				<< (const char*)userdata
				<< std::endl;
		if(size > 0 && keyFilePassword.size() > static_cast<size_t>(size)) {
			return 0;
		}
		memcpy(buf, keyFilePassword.data(), keyFilePassword.size());
		return keyFilePassword.size();
	}
}

int createContext(
	const std::string& caFile,
	const std::string& crtFile,
	const std::string& keyFile,
	const std::string& password
	)
{
	char errorTextBuffer[1024] = "";

	if(sslGlobalCtx) {
		return 0;
	}

	keyFilePassword = password;

	SSL_library_init();
	SSL_load_error_strings();

	/* An error write context */
	bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);

	/* Create our context*/
	const SSL_METHOD *meth;
	meth = SSLv23_method();
	sslGlobalCtx = SSL_CTX_new(meth);

	/* Load our keys and certificates*/
	int errorCode = 0;

	errorCode = SSL_CTX_use_certificate_chain_file(sslGlobalCtx, crtFile.c_str());
	if(errorCode != 1) {
		sslError = ERR_get_error();
		ERR_error_string(errorCode, errorTextBuffer);
		fprintf(stderr,"Can’t read certificate file\"%s\": %s\n", crtFile.c_str(), errorTextBuffer);
		SSL_CTX_free(sslGlobalCtx);
		sslGlobalCtx = nullptr;
		return sslError;
	}


	SSL_CTX_set_default_passwd_cb_userdata(sslGlobalCtx, (void*)keyFile.c_str());
	SSL_CTX_set_default_passwd_cb(sslGlobalCtx, pass_user_password_cb);
	errorCode = SSL_CTX_use_PrivateKey_file(sslGlobalCtx, keyFile.c_str(), SSL_FILETYPE_PEM);
	if(errorCode != 1) {
		sslError = ERR_get_error();
		ERR_error_string(errorCode, errorTextBuffer);
		fprintf(stderr,"Can’t read key file \"%s\": %s\n", keyFile.c_str(), errorTextBuffer);
		SSL_CTX_free(sslGlobalCtx);
		sslGlobalCtx = nullptr;
		return sslError;
	}

	/* Load the CAs we trust*/
	errorCode = SSL_CTX_load_verify_locations(sslGlobalCtx, caFile.c_str(), 0);
	if(errorCode != 1) {
		sslError = ERR_get_error();
		ERR_error_string(errorCode, errorTextBuffer);
		fprintf(stderr,"Can’t read CA list \"%s\": %s\n", caFile.c_str(), errorTextBuffer);
		SSL_CTX_free(sslGlobalCtx);
		sslGlobalCtx = nullptr;
		return sslError;
	}
	#if (OPENSSL_VERSION_NUMBER < 0x0090600fL)
	SSL_CTX_set_verify_depth(ctx,1);
	#endif
	return sslError;
}

int createClientContext()
{
	if(sslGlobalCtx) {
		return 0;
	}

	SSL_library_init();
	SSL_load_error_strings();

	/* An error write context */
	bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);

	/* Create our context*/
	const SSL_METHOD *meth;
	meth = SSLv23_method();
	sslGlobalCtx = SSL_CTX_new(meth);

	#if (OPENSSL_VERSION_NUMBER < 0x0090600fL)
	SSL_CTX_set_verify_depth(ctx,1);
	#endif
	return sslError;
}

void dropContext()
{
	if(!sslGlobalCtx) {
		return;
	}
	SSL_CTX_free(sslGlobalCtx);
	sslGlobalCtx = nullptr;
	ERR_free_strings();
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
}

} // ssl
} // dsockets
