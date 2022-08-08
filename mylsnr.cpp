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

#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>

#include <iostream>
#include "SocketFactory.h"
#include "SocketsPoller.h"
#include "SocketUtils.h"
#include "InitSsl.h"
#include "ApplicationConfig.h"
#include "utils.h"

ApplicationConfig::Ptr appCfg;
const char *MY_SOCK_PATH = "dispatch.sock";
static bool exitRequested = false;

void sig_INT(int)
{
    exitRequested = true;
}

/// @brief wait for child pid with no hung.
pid_t wait_disp_child();

pid_t disp_pid = -1;
int create_dispatcher()
{
    int r = fork();
    if( r < 0 ) {
        logger->error("fork failed");
        return -1;
    }
    if( r == 0 ) {
        execl("mydisp","mydisp",MY_SOCK_PATH,nullptr);
        assert(0);
        exit(13);
    }
    logger->info("Dispatcher has been created with PID = [{}]", r );
    disp_pid = (pid_t)r;
    return 0;
}

bool processClientConnection(
	dsockets::Socket::Ptr listenerTcpSocket,
	dsockets::SocketsList::Ptr socketsList
	)
{
    dsockets::Socket::Ptr clientConnection = listenerTcpSocket->acceptConnection();
    if( !clientConnection ) {
        logger->error("Accept client connection failed, exiting...");
        return false;
    }
    logger->info("Send accepted TCP socket to dispatcher process");
    /// TODO: need algorithm for selecting dispatcher socket here.
    for(auto s : *socketsList) {
    	if( s->socketType() == dsockets::SocketType::UNIX && !s->listenSocket()) {
    		logger->info("Send client socket to dispatcher process");
    		if(!dsockets::utils::sendSocket(s, clientConnection)) {
    			logger->error("FAILED: send client socket to dispatcher failed!");
    		}
    	}
    }
	return true;
}

bool processDispatcherConnection(
	dsockets::Socket::Ptr dispatcherSocket,
	dsockets::SocketsList::Ptr socketsList
	)
{
    dsockets::Socket::Ptr dispatcherConnection = dispatcherSocket->acceptConnection();
    if( !dispatcherConnection ) {
        logger->error("Accept dispatcher connection failed, exiting...");
        return false;
    }
    socketsList->putSocket(dispatcherConnection);
    ssize_t bytes = write( dispatcherConnection->descriptor(), "CONFIG", 6 );
    if(bytes < 0) {
        logger->error("Send config label to dispatcher failed, exiting...");
        return false;
    }

    char configBuffer[4096];
    int r = snprintf(configBuffer,sizeof(configBuffer),
    		"PPID=%d\n"
    		"CAFILE=%s\n"
    		"CERTFILE=%s\n"
    		"KEYFILE=%s\n"
    		"",
    		(int)getpid(),
			appCfg->getCaFile().c_str(),
			appCfg->getCertFile().c_str(),
			appCfg->getKeyFile().c_str()
			);
    assert(static_cast<size_t>(r) < sizeof(configBuffer));
    bytes = write( dispatcherConnection->descriptor(), configBuffer, r );
    if(bytes < 0) {
        logger->error("Send config to dispatcher failed, exiting...");
        return false;
    }
    return true;
}

void processSockets(
		dsockets::SocketsList::Ptr socketsList,
		dsockets::SocketsList::Ptr outputSocketsList)
{
	for(auto socket : *outputSocketsList) {
		if(socket->isDropped()) {
			socketsList->delSocket(socket);
			// TODO: need special action for dispatcher UNIX client socket check for process???.
			continue;
		}
		if(socket->socketType() == dsockets::SocketType::TCP && socket->listenSocket()) {
			processClientConnection(socket, socketsList);
		} else if(socket->socketType() == dsockets::SocketType::UNIX && socket->listenSocket()) {
			processDispatcherConnection(socket, socketsList);
		}
	}
}

void sayGoodbyeToAllDispatchers(
	dsockets::SocketsList::Ptr socketsList
	)
{
    for(auto s : *socketsList) {
    	if( s->socketType() == dsockets::SocketType::UNIX && !s->listenSocket()) {
    	    write(s->descriptor(), "EXIT", 4);
    	}
    }
}

int main(int argc, char *argv[])
try {
	create_logger("listner");
	std::string cAfile   = "../CA-cert.pem";
	std::string certFile = "../server-cert.pem";
	std::string keyFile  = "../server-key.pem";

	appCfg = ApplicationConfig::create(argc, argv, ApplicationType::LISTENER);

	if(!appCfg->isOk()) {
		logger->warn("Configuration failed. Load defaults settings.");
		appCfg->setCaFile(cAfile);
		appCfg->setCertFile(certFile);
		appCfg->setKeyFile(keyFile);
	}

	if(dsockets::ssl::createContext(appCfg->getCaFile(), appCfg->getCertFile(), appCfg->getKeyFile(), "1q2w3e4r")) {
		logger->critical("SSL create context failed.");
		return 1;
	}

    {
        dsockets::utility::SocketFactory::Ptr tcpListenerSocketFactory = dsockets::utility::createTcpSslListenerSocketFactory(5555);
        dsockets::utility::SocketFactory::Ptr unixListenerSocketFactory = dsockets::utility::createUnixListenerSocketFactory(MY_SOCK_PATH);

        dsockets::Socket::Ptr tcpListenerSocket = tcpListenerSocketFactory->createSocket();
        dsockets::Socket::Ptr unixListenerSocket = unixListenerSocketFactory->createSocket();

        // Run dispatcher here.
        // TODO: need configuration for count of dispatchers processes.
        create_dispatcher();

        dsockets::SocketsList::Ptr socketsList = std::make_shared<dsockets::SocketsList>(100);
        socketsList->putSocket(tcpListenerSocket);
        socketsList->putSocket(unixListenerSocket);
        dsockets::SocketsList::Ptr outputSocketsList = std::make_shared<dsockets::SocketsList>(100);

        dsockets::SocketsPoller::Ptr socketsPoller = dsockets::createSocketsPoller();

        signal( SIGINT, sig_INT );

        while(!exitRequested) {
            outputSocketsList->clear();
            for(auto s : *socketsList) {
                s->pollForRead();
            }
            dsockets::ErrorType errorType = socketsPoller->pollSockets(socketsList,outputSocketsList);
            if(errorType == dsockets::ErrorType::NONE) {
                processSockets(socketsList, outputSocketsList);
            } else if(errorType == dsockets::ErrorType::TIMEOUT) {
                pid_t pid = wait_disp_child();
                // restart dispatcher child if need.
                if( pid > pid_t(0) ) {
                    create_dispatcher();
                }
            } else if(errorType == dsockets::ErrorType::INTERRUPTED) {
                /// TODO: Do special action here.
            } else {
                logger->critical("ERROR: socket poller failed, exiting...");
                break;
            }
        }

        sayGoodbyeToAllDispatchers(socketsList);

        sleep(2);
        wait_disp_child();
    }

    dsockets::ssl::dropContext();
    logger->info("Done...");

    return 0;
} catch(const std::exception& e ) {
	logger->critical("exception: {}", e.what());
	return 1;
}


pid_t wait_disp_child()
{
    int wstatus = 0;
    pid_t pid = waitpid(pid_t(-1), &wstatus, WNOHANG );
    if(pid == pid_t(-1)) {
        perror("waitpid failed");
        return pid_t(-1);
    } else if( pid > pid_t(0) ) {
        if(WIFEXITED(wstatus)) {
            if(WEXITSTATUS(wstatus) == 0) {
                logger->info("dispatcher child pid=[{}] has exited", (int)pid);
            } else {
                logger->warn("dispatcher child pid=[{}] has exited with status=[{}]", (int)pid, WEXITSTATUS(wstatus) );
            }
        } else if(WEXITSTATUS(wstatus)) {
            logger->error("dispatcher child pid=[{}] was killed by signal=[{}]", (int)pid, WTERMSIG(wstatus) );
        }
        return pid;
    }
    return pid_t(0);
}
