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
    logger->info("SIGINT\n");
}

void sig_pipe_handler(int)
{
    logger->info("SIGPIPE\n");
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
        logger->critical("ERROR: parent has disconnected from our UNIX socket, exiting...");
		return false;
	}
    logger->info("dispatcher UNIX socket activity!");
    char message[200];
    ssize_t read_size = recv(unixSocket->descriptor(), message, 200, MSG_PEEK);
    if(!read_size) {
        logger->critical("ERROR: read zero bytes from dispatcher socket, exiting...");
        return false;
    }

	logger->info("INFO: unix socket read {} bytes", read_size);
	if(read_size == 2) {
		dsockets::Socket::Ptr clientSocket = receivedTcpSocketFactory->createSocket();
		if(!clientSocket) {
			std::cerr << "FAILED: client socket received failed." << std::endl;
			return false;
		}
		dsockets::Socket::Ptr clientSslSocket = std::make_shared<dsockets::SslSocket>(std::move(clientSocket));
		logger->info("client socket has received.");
		if(!socketsList->putSocket(clientSslSocket)) {
            dsockets::utils::write(clientSslSocket,"FAILED BUSY TRY LATER!!!!\n");
			logger->error("put socket: max size reached!");
		}
		clientSslSocket->pollForWrite();
		clientSslSocket->clientStatus(dsockets::ClientStatus::HELLO);
	} else if( memcmp(message,"CONFIG",6) == 0 ) {
		char configBuffer[4096];
		ssize_t read_size = recv(unixSocket->descriptor(), configBuffer, sizeof(configBuffer), 0);
		if(read_size < 0) {
			logger->critical("Read config failed");
			return false;
		}
		logger->info("INFO: Dispatcher has received configuration read size = {}", read_size);
		// logger->debug("\n%.*s\n", static_cast<int>(read_size), configBuffer );
		return processConfig( configBuffer, static_cast<size_t>(read_size) );
	} else if( memcmp(message,"EXIT",4) == 0 ) {
		logger->info("INFO: Dispatcher has received EXIT request = {}", read_size);
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
        logger->debug("dsockets::utils::read returns: {}", bytes);
		if( bytes == 0 ) {
			// Typical the client drop the connection.
			// Try to write bytes to socket.
			// Check on the next cycle what we have.
			// We will expect the POLLERR and POLLHUP.
			bytes = dsockets::utils::write(clientSocket, "EXIT\n" );
			if( bytes < 0 ) {
				logger->error("client socket error, read (test write) failed.");
				return false;
			}
		} else if(bytes > 0) {
			logger->info("client reading socket: {}",bytes);
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
            logger->error("write to client socket failed, drop connection: [{}] \"{}\"\n",errno,strerror(errno));
            //shutdown(clientSocket->descriptor(),SHUT_RDWR);
            return false;
        } else {
            logger->info("write to client socket: {} bytes",bytes);
            // Wait for client send command.
        }
    }
    return true;
}


int main(int argc, char *argv[])
try {
	create_logger("dispatcher");
	appCfg = ApplicationConfig::create(ApplicationType::DISPATCHER);

    if( argc != 2 ) {
        logger->critical( "Dispatcher started without argument, exiting...");
        return 0;
    }
    logger->info("Dispatcher started");
    logger->info("Try connect to: {}", argv[1]);

    dsockets::utility::SocketFactory::Ptr unixSocketFactory = dsockets::utility::createUnixSocketFactory(argv[1]);
    dsockets::Socket::Ptr unixSocket = unixSocketFactory->createSocket();
    if(!unixSocket) {
    	logger->critical("FAILED: unix  socket create.");
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
			for( const auto& s : *reSocketsList ) {
				//std::cout << "INFO: process re-events sockets list." << std::endl;
				if(s->socketType() == dsockets::SocketType::UNIX) {
					logger->info("INFO: unix socket activity.");
					if(!dispatcherProcess(unixSocket, receivedTcpSocketFactory, socketsList)) {
						dispatcherFailed = true;
						break;
					}
				} else if(s->socketType() == dsockets::SocketType::TCP ||
						  s->socketType() == dsockets::SocketType::SSL) {
					if(!processClient(s)) {
				    	logger->error("WARNING: client dropped.");
						if( !socketsList->delSocket(s) ) {
							logger->error("FAILED: socket hasn't been deleted!");
						}
					}
				}
			}
        } else if( errorType == dsockets::ErrorType::TIMEOUT ) {
        	/// TODO: process free time for something.
        } else if( errorType == dsockets::ErrorType::ERROR ) {
			dispatcherFailed = true;
			logger->error("FAILED: dispatcher has failed!");
        }
    }

    logger->info("Exit");
    return 0;
} catch(const std::exception &e) {
	logger->critical("Exception: {}", e.what());
	return 13;
}
