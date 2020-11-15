/**
 * The main listener process.
 * Must drop using xined service for some my legacy services.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
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
#include "SocketFactory.h"
#include "SocketsPoller.h"

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
        fprintf(stderr,"fork failed\n");
        return -1;
    }
    if( r == 0 ) {
        execl("mydisp","mydisp",MY_SOCK_PATH);
        assert(0);
        exit(13);
    }
    printf("Dispatcher created with PID = [%d]\n", r );
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
        fprintf(stderr,"Accept client connection failed, exiting...\n");
        return false;
    }
    printf("Send accepted TCP socket to dispatcher process\n");
    /// TODO: need algorithm for selecting dispatcher socket here.
    for(auto s : *socketsList) {
    	if( s->socketType() == dsockets::SocketType::UNIX && !s->listenSocket()) {
    		std::cout << "Send client socket to dispatcher process" << std::endl;
    		if(!dsockets::utils::sendSocket(s, clientConnection)) {
    			std::cerr << "FAILED: send client socket to dispatcher failed!" << std::endl;
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
        fprintf(stderr,"Accept dispatcher connection failed, exiting...\n");
        return false;
    }
    socketsList->putSocket(dispatcherConnection);
    char buffer[128];
    int r = snprintf(buffer,128,"CONFIG:PPID=%d\n",(int)getpid());
    assert(r<128);
    write( dispatcherConnection->descriptor(), buffer, r );
    return true;
}

void processSockets(
		dsockets::SocketsList::Ptr socketsList,
		dsockets::SocketsList::Ptr outputSocketsList)
{
	for(auto socket : *outputSocketsList) {
		if((socket->revents() & POLLHUP) || (socket->revents() & POLLERR) || (socket->revents() & POLLNVAL)) {
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
{
    dsockets::utility::SocketFactory::Ptr tcpListenerSocketFactory = dsockets::utility::createTcpListenerSocketFactory(5555);
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
    		if(s->listenSocket()) {
    			s->events(POLLIN);
    			s->revents(0);
    		}
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
    		std::cerr << "ERROR: socket poller failed, exiting..." << std::endl;
    		break;
    	}
    }

    sayGoodbyeToAllDispatchers(socketsList);

    sleep(2);
    pid_t dispatcherPid = wait_disp_child();

    printf("Done...\n");

    return 0;
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
            fprintf( stderr, "ERROR: dispatcher child pid=[%d] has exit with status=[%d]\n", (int)pid, WEXITSTATUS(wstatus) );
        } else if(WEXITSTATUS(wstatus)) {
            fprintf( stderr, "ERROR: dispatcher child pid=[%d] has killed by signal=[%d]\n", (int)pid, WTERMSIG(wstatus) );
        }
        return pid;
    }
    return pid_t(0);
}
