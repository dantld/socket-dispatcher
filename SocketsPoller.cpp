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

#include "SocketsPoller.h"

#include <poll.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

namespace dsockets {

class SocketsPollerImpl : public SocketsPoller {
public:
	SocketsPollerImpl()
	{
	}
	ErrorType pollSockets(const SocketsList::Ptr inputSockets, SocketsList::Ptr outputSockets) override;
};

SocketsPoller::Ptr createSocketsPoller()
{
	return std::make_shared<SocketsPollerImpl>();
}

ErrorType SocketsPollerImpl::pollSockets(const SocketsList::Ptr inputSockets, SocketsList::Ptr outputSockets)
{
    int retVal = 0;
    int error = 0;
    int index = 0;
    assert(inputSockets->size() <= MAX_SOCKETS_TO_POLL);
    struct pollfd pollInfo[MAX_SOCKETS_TO_POLL];

    index = 0;
    for( const auto &socket : *inputSockets ) {
    	pollInfo[index].fd = socket->descriptor();
    	pollInfo[index].events = socket->events();
    	pollInfo[index].revents = 0;
    	// Reset requested input events bits.
    	socket->events(Socket::NO_EVENTS);
    	// Reset output events bits.
    	socket->revents(Socket::NO_EVENTS);
    	index++;
    }

    retVal = ::poll( pollInfo, inputSockets->size(), 1000);

    if(retVal == -1) {
        error = errno;
        if(error == EINTR) {
            fprintf(stderr,"dispatch poll interrupted\n");
        	return ErrorType::INTERRUPTED;
        }
        fprintf(stderr,"dispatch child poll failed: [%d] \"%s\"\n", errno, strerror(errno));
        return ErrorType::ERROR;
    } else if(retVal == 0) {
        return ErrorType::TIMEOUT;
    }

    index = 0;
    for( const auto &socket : *inputSockets ) {
        if(pollInfo[index].revents == 0) {
        	index++;
        	continue;
        }
        printf("check clients socket [%d:%d:0x%0X] \n", pollInfo[index].fd, static_cast<int>(socket->socketType()), pollInfo[index].revents);
    	socket->revents(pollInfo[index].revents);
    	index++;
    	outputSockets->putSocket(socket);
    }

    return ErrorType::NONE;
}

} // dsockets
