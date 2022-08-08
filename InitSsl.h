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
