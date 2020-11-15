/*
 * SocketFactory.h
 *
 *  Created on: Nov 10, 2020
 *      Author: dantld
 */

#ifndef SOCKETFACTORY_H_
#define SOCKETFACTORY_H_

#include "Socket.h"

namespace dsockets {

namespace utility {

class SocketFactory {
public:
	using Ptr = std::shared_ptr<SocketFactory>;

	virtual ~SocketFactory() {}
	virtual Socket::Ptr createSocket() = 0;
};

SocketFactory::Ptr createUnixSocketFactory(const std::string& path);
SocketFactory::Ptr createUnixListenerSocketFactory(const std::string& path);
SocketFactory::Ptr createTcpListenerSocketFactory(std::uint16_t port, std::uint16_t queueLength = 32);
SocketFactory::Ptr createReceivedTcpSocketFactory(Socket::Ptr unixSocket);

} // end name space utility
} // end name space socket
#endif /* SOCKETFACTORY_H_ */
