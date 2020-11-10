/*
 * Socket.h
 *
 *  Created on: Nov 10, 2020
 *      Author: dantld
 */

#ifndef SOCKET_H_
#define SOCKET_H_

#include <cstdint>
#include <memory>
#include <assert.h>

namespace dsockets {

enum class SocketType : std::uint16_t {
	TCP,
	UNIX,
	UNKNOWN,
};

enum class ClientStatus : std::uint16_t {
    HELLO,
    BYE,
    ECHO,
    NONE,
};

class Socket {
public:
	static const int FREE_SOCKET = -1;
	static const int NO_EVENTS   = 0;

	Socket() = delete;

	explicit Socket(int descriptor, SocketType socketType) :
		_descriptor(descriptor),
		_socketType(socketType)
	{
		assert(_descriptor != FREE_SOCKET);
	};

	virtual ~Socket();

	inline int   descriptor() const noexcept  { return _descriptor;  }
	inline short events() const noexcept  { return _events;  }
	inline void  events(short events) noexcept { _events = events; }
	inline short revents() const noexcept { return _revents; }
	inline void  revents(short revents) noexcept { _revents = revents; }
	inline SocketType socketType() const noexcept { return _socketType; }
	inline ClientStatus clientStatus() const noexcept { return _clientStatus; }
	inline void clientStatus(ClientStatus clientStatus) noexcept { _clientStatus = clientStatus; }

	using Ptr = std::shared_ptr<Socket>;
private:
	int   _descriptor;
	short _events  = NO_EVENTS;
	short _revents = NO_EVENTS;
	SocketType _socketType;
	ClientStatus _clientStatus = ClientStatus::NONE;
};

} // sockets
#endif /* SOCKET_H_ */
