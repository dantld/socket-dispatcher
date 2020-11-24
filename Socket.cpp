/*
 * Socket.cpp
 *
 *  Created on: Nov 10, 2020
 *      Author: dantld
 */

#include "Socket.h"
#include <unistd.h>

#include <iostream>

namespace dsockets {

Socket::~Socket() {
	if(!_protectDescriptor) {
		std::cerr << "Socket::~Socket closing descriptor." << std::endl;
		close(_descriptor);
	}
	std::cerr << "Socket::~Socket( " << _descriptor << ", " << static_cast<int>(_socketType) << " )" << std::endl;
}

Socket::Ptr Socket::acceptConnection() {
	assert(0);
	return {};
}

} // dsockets


