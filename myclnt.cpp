/*
 * myclnt.cpp
 *
 *  Created on: 24 нояб., 2020 ел
 *      Author: dantld
 */

#include "SslSocket.h"
#include "SocketFactory.h"
#include "SocketUtils.h"
#include "InitSsl.h"

#include <string>
#include <stdexcept>
#include <iostream>

int main(int argc, char *argv[])
try {
	dsockets::ssl::createClientContext();
	dsockets::utility::SocketFactory::Ptr socketFactory = dsockets::utility::createTcpSslConnectSocketFactory("localhost", 5555);
	dsockets::Socket::Ptr clientSocket = socketFactory->createSocket();
	while(true) {
		char recvBuffer[1024];
		ssize_t bytes = dsockets::utils::read(clientSocket,recvBuffer,sizeof(recvBuffer),0);
		if(bytes > 0) {
			printf("%.*s\n", bytes, recvBuffer);
		}
		std::string userInputLine;
		std::cin >> userInputLine;
		dsockets::utils::write(clientSocket,userInputLine);
		if(userInputLine.empty()) {
			dsockets::utils::write(clientSocket,"EXIT");
			break;
		}
	}
	return 0;
} catch(const std::exception &e) {
	std::cerr << "Exception: " << e.what() << std::endl;
	return 13;
}
