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

/**
 * @brief Application configuration global instance.
 * @details Due dispatcher application couldn't read all data at the start
 * and some data can be dynamically sent by parent process the global instance
 * is a best place for application configuration instance.
 */
ApplicationConfig::Ptr appCfg;

const size_t MAX_SOCKETS = 10;


void sig_int_handler(int)
{
    printf("SIGINT\n");
}

void sig_pipe_handler(int)
{
    printf("SIGPIPE\n");
}

/**
 * @brief Process configuration received from parent process.
 * @param bufferConfig buffer with configuration.
 * @param bufferSize buffer size.
 * @return true if configuration has successfully parsed and read out.
 */
bool processConfig(char *bufferConfig, size_t bufferSize)
{
	char lineBuffer[256];
	const char *startPointer = bufferConfig;
	const char *endPointer = bufferConfig;
	while( static_cast<size_t>(endPointer - bufferConfig) < bufferSize ) {
		if(*endPointer != '\n') {
			endPointer++;
			continue;
		}
		memcpy(lineBuffer,startPointer,endPointer-startPointer);
		lineBuffer[endPointer-startPointer] = 0;
		appCfg->retrieveConfigOption(lineBuffer);

		startPointer = ++endPointer;
	}

	if(dsockets::ssl::createContext(appCfg->getCaFile(), appCfg->getCertFile(), appCfg->getKeyFile(), "1q2w3e4r")) {
		std::cerr << "SSL create context failed." << std::endl;
		return false;
	}

	return true;
}

/**
 * @brief Try to receive client TCP socket from the UNIX socket. Read configuration.
 * @details When the listener process send to us the connected
 * TCP socket we received it from the UNIX socket and put it to
 * sockets list. Received factory implements the all the stuff which
 * can help to us create socket from data received from UNIX socket.
 * Also parent process sent to us configuration through the UNIX socket and
 * we need read out received configuration data. Configuration data saved in
 * a global application configuration instance @appCfg.
 * @param [in] unixSocket source socket which receive the socket from listener.
 * @param [in] receivedTcpSocketFactory factory which retrieves file descriptor from UNIX socket.
 * @param [in] socketsList destination list where received socket will be putted.
 */
bool dispatcherProcess(
		dsockets::Socket::Ptr unixSocket,
		dsockets::utility::SocketFactory::Ptr receivedTcpSocketFactory,
		dsockets::SocketsList::Ptr socketsList
		) {
	if( unixSocket->isDropped() ) {
        fprintf(stderr,"ERROR: parent has disconnected from our UNIX socket, exiting...\n");
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
		dsockets::Socket::Ptr clientSslSocket = std::make_shared<dsockets::SslSocket>(std::move(clientSocket));
		std::cout << "INFO: client socket has received." << std::endl;
		if(!socketsList->putSocket(clientSslSocket)) {
            dsockets::utils::write(clientSslSocket,"FAILED BUSY TRY LATER!!!!\n");
			std::cerr << "FAILED: put socket: max size reached!" << std::endl;
		}
		clientSslSocket->pollForWrite();
		clientSslSocket->clientStatus(dsockets::ClientStatus::HELLO);
	} else if( memcmp(message,"CONFIG",6) == 0 ) {
		char configBuffer[4096];
		ssize_t read_size = recv(unixSocket->descriptor(), configBuffer, sizeof(configBuffer), 0);
		if(read_size < 0) {
			fprintf(stderr, "Read config failed\n");
			return false;
		}
		printf("INFO: Dispatcher has received configuration read size = %ld\n", read_size);
		printf("\n%.*s\n", static_cast<int>(read_size), configBuffer );
		return processConfig( configBuffer, static_cast<size_t>(read_size) );
	} else if( memcmp(message,"EXIT",4) == 0 ) {
		printf("INFO: Dispatcher has received EXIT request = %ld\n", read_size);
		return false;
	}

	return true;
}


bool processClient(dsockets::Socket::Ptr clientSocket) {
    if( clientSocket->isDropped() ) {
    	return false;
    }
    if( clientSocket->isReadAvailable() ) {
        char buffer[100];
        ssize_t bytes = dsockets::utils::read(clientSocket, buffer, 100, 0);
        std::cerr << "dsockets::utils::read returns: " << bytes << std::endl;
		if( bytes == 0 ) {
			// Typical the client drop the connection.
			// Try to write bytes to socket.
			// Check on the next cycle what we have.
			// We will expect the POLLERR and POLLHUP.
			bytes = dsockets::utils::write(clientSocket, "EXIT\n" );
			if( bytes < 0 ) {
				printf("client socket error, read (test write) failed:\n");
				return false;
			}
		} else if(bytes > 0) {
			printf("client reading socket: %ld\n",bytes);
			if( memcmp(buffer,"EXIT",4) == 0) {
				clientSocket->clientStatus(dsockets::ClientStatus::BYE);
			}
			clientSocket->revents(POLLOUT);
		} else {
			return false;
		}
    }
    if( clientSocket->isWriteAvailable() ) {
        ssize_t bytes = 0;
        if( clientSocket->clientStatus() == dsockets::ClientStatus::HELLO ) {
        	bytes = dsockets::utils::write(clientSocket, "HELLO!!!!\n" );
            // Switch to next mode ECHO.
            clientSocket->clientStatus(dsockets::ClientStatus::ECHO);
        } else if( clientSocket->clientStatus() == dsockets::ClientStatus::ECHO ) {
        	bytes = dsockets::utils::write(clientSocket, "ECHOO!!!!\n");
        } else if( clientSocket->clientStatus() == dsockets::ClientStatus::BYE ) {
        	bytes = dsockets::utils::write(clientSocket, "BYE...\n" );
            //shutdown(clientSocket->descriptor(),SHUT_RDWR);
            return false;
        }
        if( bytes == -1 ) {
            fprintf(stderr, "write to client socket failed, drop connection: [%d] \"%s\"\n",errno,strerror(errno));
            //shutdown(clientSocket->descriptor(),SHUT_RDWR);
            return false;
        } else {
            printf("write to client socket: %ld bytes\n",bytes);
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
    signal(SIGPIPE,sig_pipe_handler);

	bool dispatcherFailed = false;
    dsockets::SocketsList::Ptr reSocketsList = std::make_shared<dsockets::SocketsList>(MAX_SOCKETS);
    while(!dispatcherFailed) {
    	unixSocket->pollForRead();

    	reSocketsList->clear();
    	for( auto s : *socketsList ) {
    		if(s->socketType() != dsockets::SocketType::UNIX ) {
    			s->pollForRead();
    		}
    	}
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
