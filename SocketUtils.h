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

#ifndef SOCKETUTILS_H_
#define SOCKETUTILS_H_

#include "Socket.h"
#include <string>

namespace dsockets {
namespace utils {
	bool sendSocket(Socket::Ptr socketSender, Socket::Ptr socketToSend);

	ssize_t read(Socket::Ptr clientSocket, void *buffer, size_t bufferSize, int flags);
	ssize_t write(Socket::Ptr clientSocket, const std::string& message);
} // utils
} // dsockets




#endif /* SOCKETUTILS_H_ */
