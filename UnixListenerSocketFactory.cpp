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
#include <unistd.h>
#include <string>
#include "utils.h"

namespace dsockets {

namespace utility {

class UnixListenerSocket : public Socket {
public:
	explicit UnixListenerSocket(
		int descriptor,
		const std::string path
		)
		: Socket(descriptor, SocketType::UNIX, true),
		  _path(path)
	{
	}
	~UnixListenerSocket() {
		/// FIXME: I'm not sure who is responsible for deleting socket file.
	    unlink(_path.c_str());
	}
	Socket::Ptr acceptConnection() override {
	    int new_fd = accept( descriptor(), NULL, NULL);
	    if (new_fd < 0) {
	        logger->error("accept dispatcher connection failed: {}", strerror(errno));
	        return {};
	    }
	    logger->info("Dispatcher Connection Has Accepted");
	    return std::make_shared<Socket>(new_fd, SocketType::UNIX, false);
	}

private:
	std::string _path;
};

class UnixListenerSocketFactory : public SocketFactory {
	std::string _path;
public:
	UnixListenerSocketFactory(const std::string& path) : _path(path) {}
	Socket::Ptr createSocket() override;
};

Socket::Ptr UnixListenerSocketFactory::createSocket()
{
	int sfd;
	struct sockaddr_un my_addr;

	sfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sfd < 0) return {};

	unlink(_path.c_str());
	memset(&my_addr, 0, sizeof(struct sockaddr_un));
	my_addr.sun_family = AF_UNIX;
	strncpy( my_addr.sun_path, _path.c_str(), sizeof(my_addr.sun_path) - 1);

	int retVal = 0;
	size_t len = strlen(my_addr.sun_path) + sizeof(my_addr.sun_family);
	retVal = bind(sfd, (struct sockaddr *) &my_addr, len);
	if( retVal < 0 ) return {};
	retVal = listen( sfd, 3);
	if(retVal < 0 ) return {};

	return std::make_shared<UnixListenerSocket>(sfd,_path);
}

SocketFactory::Ptr createUnixListenerSocketFactory(const std::string& path)
{
	return std::make_shared<UnixListenerSocketFactory>(path);
}

} // utility
} // dosockets



