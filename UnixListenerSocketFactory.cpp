/*
 * UnixSocketListenerFactory.cpp
 *
 *  Created on: Nov 13, 2020
 *      Author: dantld
 */
#include "SocketFactory.h"
#include <memory>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
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
	    struct sockaddr_un peer_addr;
	    socklen_t peer_addr_size;

	    peer_addr_size = sizeof(struct sockaddr_un);
	//    int disp_fd = accept( unix_fd, (struct sockaddr *) &peer_addr, &peer_addr_size);
	    int new_fd = accept( descriptor(), NULL, NULL);
	    if (new_fd < 0) {
	        fprintf(stderr,"accept dispatcher connection failed: %s\n", strerror(errno));
	        return {};
	    }
	    printf("Dispatcher Connection Has Accepted\n");
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



