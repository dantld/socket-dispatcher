/*
 * SocketPoller.h
 *
 *  Created on: Nov 10, 2020
 *      Author: dantld
 */

#ifndef SOCKETSPOLLER_H_
#define SOCKETSPOLLER_H_

#include <memory>

#include "SocketsList.h"

namespace dsockets {

class SocketsPoller {
public:
	using Ptr = std::shared_ptr<SocketsPoller>;

	virtual ~SocketsPoller() {}

	virtual ErrorType pollSockets(const SocketsList::Ptr inputSockets, SocketsList::Ptr outputSockets) = 0;
private:
};

SocketsPoller::Ptr createSocketsPoller();
}

#endif /* SOCKETSPOLLER_H_ */
