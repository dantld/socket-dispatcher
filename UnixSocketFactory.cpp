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

#include "SocketFactory.h"
#include <memory>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <errno.h>
#include <string>
#include "utils.h"

namespace dsockets {

namespace utility {

class UnixSocket : public Socket {
public:
	UnixSocket(int descriptor) : Socket(descriptor, SocketType::UNIX) {
	}
};

class UnixSocketFactory : public SocketFactory {
	std::string _path;
public:
	UnixSocketFactory(const std::string& path) : _path(path) {}
	Socket::Ptr createSocket() override;
};

Socket::Ptr UnixSocketFactory::createSocket()
{
    int sfd;
    struct sockaddr_un my_addr;

    sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd < 0) return {};

    memset(&my_addr, 0, sizeof(struct sockaddr_un));
    my_addr.sun_family = AF_UNIX;
    strncpy( my_addr.sun_path, _path.c_str(), sizeof(my_addr.sun_path) - 1);

    size_t len = strlen(my_addr.sun_path) + sizeof(my_addr.sun_family);
    int retVal = connect(sfd, (struct sockaddr *) &my_addr, len);
    if( retVal < 0 ) {
        logger->error("Connect failed: [{}]", strerror(errno));
        return {};
    }

    setnonblocking(sfd);

    return std::make_shared<UnixSocket>(sfd);
}

SocketFactory::Ptr createUnixSocketFactory(const std::string& path)
{
	return std::make_shared<UnixSocketFactory>(path);
}

} // utility
} // socket
