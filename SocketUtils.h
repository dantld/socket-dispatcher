/*
 * SocketUtil.h
 *
 *  Created on: 23 нояб., 2020 ел
 *      Author: dantld
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
