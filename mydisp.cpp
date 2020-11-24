#include <netinet/tcp.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>

#include <iostream>

#include "utils.h"

#include "SslSocket.h"
#include "SocketUtils.h"
#include "SocketFactory.h"
#include "SocketsPoller.h"
#include "InitSsl.h"
#include "ApplicationConfig.h"

ApplicationConfig::Ptr appCfg;

const size_t MAX_SOCKETS = 10;


void sig_int_handler(int)
{
    printf("SIGINT\n");
}

void retrieveConfigOption(const char* configLine)
{
#define CONFIGPID "CONFIGPPID="
#define CAFILE    "CAFILE="
#define CERTFILE  "CERTFILE="
#define KEYFILE   "KEYFILE="
	if(       memcmp(configLine, CONFIGPID, sizeof(CONFIGPID)-1) == 0 ) {
		/// TODO: need to store parent PID.
	} else if(memcmp(configLine, CAFILE,    sizeof(CAFILE)-1)    == 0 ) {
		appCfg->setCaFile(configLine+sizeof(CAFILE)-1);
	} else if(memcmp(configLine, CERTFILE,  sizeof(CERTFILE)-1)  == 0)  {
		appCfg->setCertFile(configLine+sizeof(CERTFILE)-1);
	} else if(memcmp(configLine, KEYFILE,  sizeof(KEYFILE)-1)    == 0)  {
		appCfg->setKeyFile(configLine+sizeof(KEYFILE)-1);
	}
#undef CONFIGPID
#undef CAFILE
#undef CERTFILE
#undef KEYFILE
}

bool processConfig(char *bufferConfig, size_t bufferSize)
{
	char lineBuffer[256];
	const char *startPointer = bufferConfig;
	const char *endPointer = bufferConfig;
	while( (endPointer - bufferConfig) < bufferSize ) {
		if(*endPointer != '\n') {
			endPointer++;
			continue;
		}
		memcpy(lineBuffer,startPointer,endPointer-startPointer);
		lineBuffer[endPointer-startPointer] = 0;
		retrieveConfigOption(lineBuffer);

		startPointer = ++endPointer;
	}

	if(dsockets::ssl::createContext(appCfg->getCaFile(), appCfg->getCertFile(), appCfg->getKeyFile(), "1q2w3e4r")) {
		std::cerr << "SSL create context failed." << std::endl;
		return false;
	}

	return true;
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
		dsockets::Socket::Ptr clientSslSocket = std::make_shared<dsockets::SslSocket>(clientSocket);
		std::cout << "INFO: client socket has received." << std::endl;
		if(!socketsList->putSocket(clientSslSocket)) {
            dsockets::utils::write(clientSocket,"FAILED BUSY TRY LATER!!!!\n");
			std::cerr << "FAILED: put socket: max size reached!" << std::endl;
		}
		clientSocket->events(POLLOUT);
		clientSocket->clientStatus(dsockets::ClientStatus::HELLO);
	} else if( memcmp(message,"CONFIG",6) == 0 ) {
		char configBuffer[4096];
		ssize_t read_size = recv(unixSocket->descriptor(), configBuffer, sizeof(configBuffer), 0);
		if(read_size < 0) {
			fprintf(stderr, "Read config failed\n");
			return false;
		}
		printf("INFO: Dispatcher has received configuration read size = %ld\n", read_size);
		printf("\n%.*s\n", read_size, configBuffer );
		return processConfig( configBuffer, static_cast<size_t>(read_size) );
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
        ssize_t r = dsockets::utils::read(clientSocket, buffer, 100, 0);
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
            r = dsockets::utils::write(clientSocket, "HELLO!!!!\n" );
            // Switch to next mode ECHO.
            clientSocket->clientStatus(dsockets::ClientStatus::ECHO);
        } else if( clientSocket->clientStatus() == dsockets::ClientStatus::ECHO ) {
            r = dsockets::utils::write(clientSocket, "ECHOO!!!!\n");
        } else if( clientSocket->clientStatus() == dsockets::ClientStatus::BYE ) {
            r = dsockets::utils::write(clientSocket, "BYE...\n" );
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
try {
	appCfg = ApplicationConfig::create(ApplicationType::DISPATCHER);

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

	bool dispatcherFailed = false;
    while(!dispatcherFailed) {
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
						dispatcherFailed = true;
						break;
					}
				} else if(s->socketType() == dsockets::SocketType::TCP ||
						  s->socketType() == dsockets::SocketType::SSL) {
					if(!processClient(s)) {
				    	std::cerr << "WARNING: client dropped." << std::endl;
						if( !socketsList->delSocket(s) ) {
							std::cerr << "FAILED: socket hasn't been deleted!" << std::endl;
						}
					}
				}
			}
        } else if( errorType == dsockets::ErrorType::TIMEOUT ) {
        	/// TODO: process free time for something.
        } else if( errorType == dsockets::ErrorType::ERROR ) {
			dispatcherFailed = true;
			std::cerr << "FAILED: dispatcher has failed!" << std::endl;
        }
    }

    printf("Exit\n");
    return 0;
} catch(const std::exception &e) {
	std::cerr << "Exception: " << e.what() << std::endl;
	return 13;
}
