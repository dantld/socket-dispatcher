#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <fcntl.h>
#include <poll.h>

#include <iostream>

#include "utils.h"

#include "SocketFactory.h"
#include "SocketsPoller.h"

const size_t MAX_SOCKETS = 10;


void sig_int_handler(int)
{
    printf("SIGINT\n");
}

bool dispatcherProcess(
		dsockets::Socket::Ptr unixSocket,
		dsockets::utility::SocketFactory::Ptr receivedTcpSocketFactory,
		dsockets::SocketsList::Ptr socketsList
		) {
	if( unixSocket->revents() == POLLHUP ) {
        fprintf(stderr,"ERROR: parent has disconnected from our unix socket, exiting...\n");
		return false;
	}
    std::cout << "dispatcher UNIX socket activity!" << std::endl;
    char message[200];
    ssize_t read_size = recv(unixSocket->descriptor(), message, 200, MSG_PEEK);
    if(!read_size) {
        fprintf(stderr,"ERROR: read zero bytes from dispatcher socket, exiting...\n");
        return false;
    }

	std::cout << "INFO: unix socket read " << read_size << " bytes." << std::endl;
	if(read_size == 2) {
		dsockets::Socket::Ptr clientSocket = receivedTcpSocketFactory->createSocket();
		if(!clientSocket) {
			std::cerr << "FAILED: client socket received failed." << std::endl;
			return false;
		}
		std::cout << "INFO: client socket has received." << std::endl;
		if(!socketsList->putSocket(clientSocket)) {
            write(clientSocket->descriptor(),"FAILED BUSY TRY LATER!!!!\n",25);
			std::cerr << "FAILED: put socket: max size reached!" << std::endl;
		}
		clientSocket->events(POLLOUT);
		clientSocket->clientStatus(dsockets::ClientStatus::HELLO);
	} else if( memcmp(message,"CONFIG",6) == 0 ) {
		ssize_t read_size = recv(unixSocket->descriptor(), message, 200, 0);
		printf("INFO: Dispatcher has received configuration read size = %ld\n", read_size);
	} else if( memcmp(message,"EXIT",4) == 0 ) {
		printf("INFO: Dispatcher has received EXIT request = %ld\n", read_size);
		return false;
	}

	return true;
}


bool testSocketForDrop(dsockets::Socket::Ptr clientSocket) {
    if( (clientSocket->revents() & POLLERR) ||
        (clientSocket->revents() & POLLHUP) ||
		(clientSocket->revents() & POLLNVAL)||
        (clientSocket->revents() & POLLRDHUP)) {
    	return true;
    }
	return false;
}

bool testSocketForRead(dsockets::Socket::Ptr clientSocket) {
	if( clientSocket->revents() & POLLIN ) {
		return true;
	}
	return false;
}

bool testSocketForWrite(dsockets::Socket::Ptr clientSocket) {
	if( clientSocket->revents() & POLLOUT ) {
		return true;
	}
	return false;
}


bool processClient(dsockets::Socket::Ptr clientSocket) {
    if( testSocketForDrop(clientSocket) ) {
    	return false;
    }
    if( testSocketForRead(clientSocket) ) {
        char buffer[100];
        ssize_t r = recv(clientSocket->descriptor(), buffer, 100, 0);
		if( r == 0 ) {
			// Typical client drop the connection.
			// Try to write bytes to socket.
			// Check on the next cycle what we have.
			// We will expect the POLLERR and POLLHUP.
			r = write(clientSocket->descriptor(), "EXIT\n", 5 );
			if( r < 0 ) {
				printf("client socket error, read (test write) failed:\n");
				return false;
			}
		} else if(r > 0) {
			printf("client reading socket: %d\n",r);
			if( memcmp(buffer,"EXIT",4) == 0) {
				clientSocket->clientStatus(dsockets::ClientStatus::BYE);
			}
			clientSocket->revents(POLLOUT);
		}
    }
    if( testSocketForWrite(clientSocket) ) {
        ssize_t r = 0;
        if( clientSocket->clientStatus() == dsockets::ClientStatus::HELLO ) {
            r = write(clientSocket->descriptor(), "HELLO!!!!\n", 10 );
            // Switch to next mode ECHO.
            clientSocket->clientStatus(dsockets::ClientStatus::ECHO);
        } else if( clientSocket->clientStatus() == dsockets::ClientStatus::ECHO ) {
            r = write(clientSocket->descriptor(), "ECHOO!!!!\n", 10 );
        } else if( clientSocket->clientStatus() == dsockets::ClientStatus::BYE ) {
            r = write(clientSocket->descriptor(), "BYE...\n", 7 );
            shutdown(clientSocket->descriptor(),SHUT_RDWR);
            return false;
        }
        if( r == -1 ) {
            fprintf(stderr, "write to client socket failed, drop connection: [%d] \"%s\"\n",errno,strerror(errno));
            shutdown(clientSocket->descriptor(),SHUT_RDWR);
            return false;
        } else {
            printf("write to client socket: %d bytes\n",r);
            clientSocket->events(POLLIN);
            // Wait for client send command.
        }
    }
    return true;
}


int main(int argc, char *argv[])
{
    if( argc != 2 ) {
        fprintf(stderr, "Dispatcher started without argument, exiting...\n");
        return 0;
    }
    printf("Dispatcher started\n");
    printf("Try connect to: %s\n", argv[1]);

    dsockets::utility::SocketFactory::Ptr unixSocketFactory = dsockets::utility::createUnixSocketFactory(argv[1]);
    dsockets::Socket::Ptr unixSocket = unixSocketFactory->createSocket();
    if(!unixSocket) {
    	std::cerr << "FAILED: unix  socket create." << std::endl;
    	return 1;
    }

    dsockets::utility::SocketFactory::Ptr receivedTcpSocketFactory = dsockets::utility::createReceivedTcpSocketFactory(unixSocket);

    dsockets::SocketsPoller::Ptr socketsPoller = dsockets::createSocketsPoller();

    dsockets::SocketsList::Ptr socketsList = std::make_shared<dsockets::SocketsList>(MAX_SOCKETS);
	socketsList->putSocket(unixSocket);

    signal(SIGINT,sig_int_handler);

	bool dispatcherFailed = true;
    while(dispatcherFailed) {
    	unixSocket->events(POLLIN);
    	/// TODO: need variant for create sockets list on a stack!
    	///       Bad design: always allocate object at heap.
        dsockets::SocketsList::Ptr reSocketsList = std::make_shared<dsockets::SocketsList>(MAX_SOCKETS);
        dsockets::ErrorType errorType = socketsPoller->pollSockets( socketsList, reSocketsList );

        if( errorType == dsockets::ErrorType::NONE ) {
			for( const auto s : *reSocketsList ) {
				//std::cout << "INFO: process re-events sockets list." << std::endl;
				if(s->socketType() == dsockets::SocketType::UNIX) {
					std::cout << "INFO: unix socket activity." << std::endl;
					if(!dispatcherProcess(unixSocket, receivedTcpSocketFactory, socketsList)) {
						dispatcherFailed = false;
						break;
					}
				} else if(s->socketType() == dsockets::SocketType::TCP) {
					if(!processClient(s)) {
				    	std::cerr << "WARNING: client dropped." << std::endl;
						if( !socketsList->delSocket(s) ) {
							std::cerr << "FAILED: socket hasn't been deleted!" << std::endl;
						}
					}
				}
			}
        }
    }

    printf("Exit\n");
    return 0;
}
