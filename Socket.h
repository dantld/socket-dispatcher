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

#ifndef SOCKET_H_
#define SOCKET_H_

#include <cstdint>
#include <memory>
#include <assert.h>
#include <poll.h>

namespace dsockets {
/// @brief Socket types enumeration.
enum class SocketType : std::uint16_t {
	TCP,
	SSL,
	UNIX,
	UNKNOWN,
};

/// @todo Need to remove from Socket. It related to client code.
enum class ClientStatus : std::uint16_t {
    HELLO,
    BYE,
    ECHO,
    NONE,
};

/**
 * @brief Class introduce the socket abstraction.
 */
class Socket {
public:
	/// @brief Constant for checking file descriptor value.
	static const int FREE_SOCKET = -1;
	/// @brief Constant for reseting _events and _revents fields.
	static const int NO_EVENTS   = 0;

	Socket() = delete;

	explicit Socket(int descriptor, SocketType socketType, bool listenSocket = false) :
		_descriptor(descriptor),
		_socketType(socketType),
		_listenSocket(listenSocket)
	{
		assert(_descriptor != FREE_SOCKET);
	};

	virtual ~Socket();

	inline int   descriptor() const noexcept  { return _descriptor;  }
	/// @brief Get specific set of POLL bits, used by poll.
	inline short events() const noexcept { return _events; }
	/// @brief Set specific set of POLL bits.
	inline void  events(short events) noexcept { _events = events; }
	/// @brief Decoded version for setting poll event bits, set read bit.
	inline void pollForRead() noexcept { _events |= POLLIN; }
	/// @brief Decoded version for setting poll event bits, set write bit.
	inline void pollForWrite() noexcept { _events |= POLLOUT; }

	/// @brief Read set of bits set by poll call.
	inline short revents() const noexcept { return _revents; }
	/// @brief Used primary by poll call for set the re-events.
	inline void  revents(short revents) noexcept { _revents = revents; }

	/// @brief Decode re-events: check for read ability.
	inline bool isReadAvailable() const noexcept { return _revents & POLLIN; }
	/// @brief Decode re-events: check for write ability.
	inline bool isWriteAvailable() const noexcept { return _revents & POLLOUT; }
	/// @brief Decode re-events: check for error, hung up, etc.
	inline bool isDropped() const noexcept {
	    if( (_revents & POLLERR) ||
	        (_revents & POLLHUP) ||
			(_revents & POLLNVAL)||
	        (_revents & POLLRDHUP)) {
	    	return true;
	    }
		return false;
	}
	/// @brief Return socket type.
	inline SocketType socketType() const noexcept { return _socketType; }
	/// @brief Return true if socket is listener socket.
	inline bool listenSocket() const noexcept { return _listenSocket; }
	/// @todo Need to remove from here it related to client scope not library.
	inline ClientStatus clientStatus() const noexcept { return _clientStatus; }
	/// @todo Need to remove from here it related to client scope not library.
	inline void clientStatus(ClientStatus clientStatus) noexcept { _clientStatus = clientStatus; }

	/// @brief Disable to close file descriptor when instance died.
	void protectDescriptor() { _protectDescriptor = true; }

	using Ptr = std::shared_ptr<Socket>;

	/// @brief Special method for listener socket.
	virtual Ptr acceptConnection();

private:
	bool  _protectDescriptor = false;
	int   _descriptor;
	short _events  = NO_EVENTS;
	short _revents = NO_EVENTS;
	SocketType _socketType = SocketType::UNKNOWN;
	bool _listenSocket = false;
	ClientStatus _clientStatus = ClientStatus::NONE;
};

} // sockets
#endif /* SOCKET_H_ */
