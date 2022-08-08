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
#include "SocketFactory.h"
#include "SocketUtils.h"
#include "InitSsl.h"
#include "utils.h"
#include <string>
#include <stdexcept>
#include <iostream>

int main(int argc, char *argv[])
try {
	create_logger("client");
	dsockets::ssl::createClientContext();
	dsockets::utility::SocketFactory::Ptr socketFactory = dsockets::utility::createTcpSslConnectSocketFactory("localhost", 5555);
	dsockets::Socket::Ptr clientSocket = socketFactory->createSocket();
	if(!clientSocket) {
		logger->critical("connect failed.");
		return 1;
	}
	while(true) {
		char recvBuffer[1024];
		ssize_t bytes = dsockets::utils::read(clientSocket,recvBuffer,sizeof(recvBuffer),0);
		if(bytes > 0) {
			printf("%.*s\n", static_cast<int>(bytes), recvBuffer);
		}
		std::string userInputLine;
		std::cin >> userInputLine;
		if(userInputLine.empty()) {
			dsockets::utils::write(clientSocket,"EXIT");
			break;
		}
		dsockets::utils::write(clientSocket,userInputLine);
	}
	return 0;
} catch(const std::exception &e) {
	logger->critical("Exception: {}", e.what());
	return 1;
}
