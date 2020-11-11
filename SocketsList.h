/*
 * DispSocket.h
 *
 *  Created on: Nov 10, 2020
 *      Author: dantld
 */

#ifndef SOCKETSLIST_H_
#define SOCKETSLIST_H_

#include <stdint.h>
#include <algorithm>
#include <memory>
#include <list>
#include "Socket.h"

namespace dsockets {

enum class ErrorType : uint16_t {
    ERROR,
    TIMEOUT,
    NONE,
};

class SocketsList {
	using SocketsListImpl = std::list<Socket::Ptr>;
public:
	SocketsList() = delete;

	SocketsList(size_t maxSize) : _maxSize(maxSize) {
	}

	bool putSocket(Socket::Ptr socket) {
		if(_sockets.size() == _maxSize) return false;
		_sockets.emplace_back(std::move(socket));
		return true;
	}

	bool delSocket(Socket::Ptr socket) {
		if(_sockets.size() == _maxSize) return false;
		SocketsListImpl::iterator it = _sockets.begin();
		while(it != _sockets.end()) {
			if((*it)->descriptor() == socket->descriptor()) {
				_sockets.erase(it);
				return true;
			}
			it++;
		}
		return false;
	}

	SocketsListImpl::iterator begin() { return _sockets.begin(); }
	SocketsListImpl::iterator end() { return _sockets.end(); }
	size_t size() { return _sockets.size(); }

	using Ptr = std::shared_ptr<SocketsList>;
private:
	SocketsListImpl _sockets;
	size_t _maxSize;
};

} // dsockets
#endif /* SOCKETSLIST_H_ */
