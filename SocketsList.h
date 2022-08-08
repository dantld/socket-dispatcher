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
    INTERRUPTED,
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

	void clear() {
		_sockets.clear();
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
