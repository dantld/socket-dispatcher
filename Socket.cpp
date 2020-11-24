/*
 * Socket.cpp
 *
 *  Created on: Nov 10, 2020
 *      Author: dantld
 */

#include "Socket.h"
#include <unistd.h>

namespace dsockets {

Socket::~Socket() {
	close(_descriptor);
}

Socket::Ptr Socket::acceptConnection() {
	assert(0);
	return {};
}

} // dsockets


