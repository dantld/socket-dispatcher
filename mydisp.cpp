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


enum class ErrorType : uint16_t {
    ERROR,
    TIMEOUT,
    NONE,
};

enum class ClientStatus : uint16_t {
    HELLO,
    BYE,
    ECHO,
    NONE,
};

/// @brief Poll all our sockets for events.
ErrorType poll_sockets(int unix_fd, short &revents, int &error, int &count);

int recv_fd(int fd, ssize_t (*userfunc)(int, const void *, size_t));

const int MAX_SOCKETS = 10;
const int FREE_SOCKET = -1;
const int NO_EVENTS   = 0;
struct Sockets { int socket = FREE_SOCKET; short events = NO_EVENTS; short revents = NO_EVENTS; ClientStatus status = ClientStatus::NONE; };
static Sockets sockets[MAX_SOCKETS];

bool put_socket( int socket )
{
    for( int i = 0; i < MAX_SOCKETS; i++ ) {
        if( sockets[i].socket == FREE_SOCKET ) {
            sockets[i].socket = socket;
            // First time check the possibility of writing to socket.
            sockets[i].events = POLLOUT;
            // Send the HELLO message.
            sockets[i].status = ClientStatus::HELLO;
            return true;
        }
    }
    return false;
}
bool del_socket( int socket )
{
    for( int i = 0; i < MAX_SOCKETS; i++ ) {
        if( sockets[i].socket == socket ) {
            sockets[i].socket  = FREE_SOCKET;
            sockets[i].events  = NO_EVENTS;
            sockets[i].revents = NO_EVENTS;
            sockets[i].status  = ClientStatus::NONE;
            return true;
        }
    }
    return false;
}
void set_events( int socket, short events ) {
    for( int i = 0; i < MAX_SOCKETS; i++ ) {
        if( sockets[i].socket == socket ) {
            sockets[i].events  = events;
            sockets[i].revents = NO_EVENTS;
        }
    }
}
void put_revents( int socket, short events ) {
    for( int i = 0; i < MAX_SOCKETS; i++ ) {
        sockets[i].revents = 0;
        if( sockets[i].socket == socket ) {
            if( events & POLLNVAL ) {
                fprintf(stderr,"ERROR: invalid socket POLLINVAL event has come.\n");
                sockets[i].socket = FREE_SOCKET;
            }
            sockets[i].events  = NO_EVENTS;
            sockets[i].revents = events;
        }
    }
}

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
						if( !socketsList->delSocket(s) ) {
							std::cerr << "FAILED: socket hasn't been deleted!" << std::endl;
						}
					}
				}
			}
        }
    }

    exit(0);

    for(int i = 0; i < MAX_SOCKETS; i++ ) sockets[i].socket = FREE_SOCKET;

    int sfd;
    struct sockaddr_un my_addr;

    sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd < 0) return -1;

    memset(&my_addr, 0, sizeof(struct sockaddr_un));
    my_addr.sun_family = AF_UNIX;
    strncpy( my_addr.sun_path, argv[1], sizeof(my_addr.sun_path) - 1);

    size_t len = strlen(my_addr.sun_path) + sizeof(my_addr.sun_family);
    int retVal = connect(sfd, (struct sockaddr *) &my_addr, len);
    if( retVal < 0 ) {
        fprintf(stderr,"Connect failed: [%s]\n", strerror(errno));
        return -2;
    }

    setnonblocking(sfd);

    signal(SIGINT,sig_int_handler);

    while(1) {
        ErrorType errorType;
        int error;
        int count;
        short sfd_revents = 0;
        errorType = poll_sockets( sfd, sfd_revents, error, count );

        if( errorType == ErrorType::ERROR ) {
            printf("ERROR\n");
//            break;
        } else if( errorType == ErrorType::TIMEOUT ) {
//            printf("TIMEOUT\n");
        } else if( errorType == ErrorType::NONE ) {
            if( sfd_revents != 0) {
                printf("dispatcher UNIX socket activity!\n");
                char message[200];
                ssize_t read_size = recv(sfd, message, 200, MSG_PEEK);
                if(!read_size) {
                    fprintf(stderr,"ERROR: read zero bytes from dispatcher socket, exiting...\n");
                    break;
                }
                printf("ready for read data from unix socket [%d]\n", read_size);
                if(read_size == 2) {
                    int s = recv_fd(sfd, NULL);
                    if(s != -1) {
                        if(!put_socket(s)) {
                            write(s,"FAILED BUSY TRY LATER!!!!\n",25);
                            close(s);
                        } else {
                            printf("Accept received socket\n");
                        }
                    } else {
                        fprintf(stderr,"ERROR: receive socket failed\n");
                    }
                } else if( memcmp(message,"CONFIG",6) == 0 ) {
                    ssize_t read_size = recv(sfd, message, 200, 0);
                    printf("INFO: Dispatcher config received read size = %ld\n", read_size);
                } else if( memcmp(message,"EXIT",4) == 0 ) {
                    printf("INFO: Dispatcher config received EXIT request = %ld\n", read_size);
                    break;
                }
            } else if(count > 0) {
                // Process clients TCP sockets here.
                for( int i = 0; i < MAX_SOCKETS; i++ ) {
                    if( sockets[i].socket  == FREE_SOCKET ) continue;
                    if( sockets[i].revents == 0 ) continue;
                    if( (sockets[i].revents & POLLERR) ||
                        (sockets[i].revents & POLLHUP) ||
                        (sockets[i].revents & POLLRDHUP)) {
                        bool pollErr   = sockets[i].revents & POLLERR;
                        bool pollHup   = sockets[i].revents & POLLHUP;
                        bool pollRdHup = sockets[i].revents & POLLRDHUP;
                        printf("client socket error: [ERR:%d HUP:%d RDHUP:%d] close socket.\n", pollErr, pollHup, pollRdHup );
                        close(sockets[i].socket);
                        del_socket(sockets[i].socket);
                        continue;
                    }
                    if( sockets[i].revents & POLLIN ) {
                        char buffer[100];
                        ssize_t r = recv(sockets[i].socket, buffer, 100, 0);
                        if( r == 0 ) {
                            // Typical client drop the connection.
                            // Try to write bytes to socket.
                            // Check on the next cycle what we have.
                            // We wil expect the POLLERR and POLLHUP.
                            r = write(sockets[i].socket, "EXIT\n", 5 );
                            if( r < 0 ) {
                                printf("client socket error, read (test write) failed:\n");
                                close(sockets[i].socket);
                                del_socket(sockets[i].socket);
                                continue;
                            }
                            continue;
                        }
                        printf("client reading socket: %d\n",r);
                        if( memcmp(buffer,"EXIT",4) == 0) {
                            sockets[i].status = ClientStatus::BYE;
                        }
                        set_events(sockets[i].socket, POLLOUT);
                    }
                    if( sockets[i].revents & POLLOUT ) {
                        ssize_t r = 0;
                        if( sockets[i].status == ClientStatus::HELLO ) {
                            r = write(sockets[i].socket, "HELLO!!!!\n", 10 );
                            // Switch to next mode ECHO.
                            sockets[i].status = ClientStatus::ECHO;
                        } else if( sockets[i].status == ClientStatus::ECHO ) {
                            r = write(sockets[i].socket, "ECHOO!!!!\n", 10 );
                        } else if( sockets[i].status == ClientStatus::BYE ) {
                            r = write(sockets[i].socket, "BYE...\n", 7 );
                            shutdown(sockets[i].socket,SHUT_RDWR);
                            close(sockets[i].socket);
                            del_socket(sockets[i].socket);
                        }
                        if( r == -1 ) {
                            fprintf(stderr, "write to client socket failed, drop connection: [%d] \"%s\"\n",errno,strerror(errno));
                            shutdown(sockets[i].socket,SHUT_RDWR);
                            close(sockets[i].socket);
                            del_socket(sockets[i].socket);
                        } else {
                            printf("write to client socket: %d bytes\n",r);
                            set_events(sockets[i].socket, POLLIN);
                        }
                        // Wait for client send command.
                    }
                }
            }
        }
    }

    close(sfd);
    printf("Exit\n");
    return 0;
}

ErrorType poll_sockets(int unix_fd, short &revents, int &error, int &count) {
    int retVal = 0;
    error = 0;
    count = 0;
    revents = 0;
    struct pollfd p[MAX_SOCKETS+1];
    nfds_t nfds = 1;
    p[0].fd = unix_fd;
    p[0].events = POLLIN;
    p[0].revents = 0;
    for( int index = 0, pidx = 1; index < MAX_SOCKETS; index++ ) {
        if(sockets[index].socket == FREE_SOCKET) continue;
        p[pidx].fd = sockets[index].socket;
        p[pidx].events = sockets[index].events;// POLLIN;
        // depend on status we decide: do we need wait for write to socket availability.
        //if( sockets[index].status == 0 ) p[pidx].events |= POLLOUT;
        p[pidx].revents = 0;
        nfds++;
        pidx++;
    }
//    printf("Check nfds=%d\n",nfds);
    retVal = poll(p, nfds, 1000);
//    int ppoll(struct pollfd *fds, nfds_t nfds, const struct timespec *tmo_p, const sigset_t *sigmask);
    if(retVal == -1) {
        error = errno;
        fprintf(stderr,"disp child poll failed: [%d] \"%s\"\n", errno, strerror(errno));
        return ErrorType::ERROR;
    } else if(retVal == 0) {
//        printf("disp timeout occurred\n");
        return ErrorType::TIMEOUT;
    }
//    printf("REVENTS %d\n",retVal);
    if( p[0].revents != 0 ) {
        count++;
        revents = p[0].revents;
    }
    for( int i = 1; i < nfds; i++ ) {
        if(p[i].revents == 0) continue;
        printf("check clients socket [%d:0x%0X] \n", p[i].fd, p[i].revents);
        put_revents( p[i].fd, p[i].revents );
        count++;
    }
    return ErrorType::NONE;
}


#define err_sys(msg)  fprintf(stderr,"ERROR:  %s\n",msg)
#define err_ret(msg)  fprintf(stderr,"RETURN: %s\n",msg)
#define err_dump(msg) fprintf(stderr,"DUMP:   %s\n",msg)

/*
 * Receive a file descriptor from a server process.  Also, any data
 * received is passed to (*userfunc)(STDERR_FILENO, buf, nbytes).
 * We have a 2-byte protocol for receiving the fd from send_fd().
 */
int recv_fd(int fd, ssize_t (*userfunc)(int, const void *, size_t))
{
   int             newfd, nr, status;
   char            *ptr;
   const           int MAXLINE=128;
   char            buf[MAXLINE];
   struct iovec    iov[1];
   struct msghdr   msg;
   /* size of control buffer to send/recv one file descriptor */
   const int CONTROLLEN  CMSG_LEN(sizeof(int));
   static struct cmsghdr   *cmptr = NULL;      /* malloc'ed first time */

   status = -1;
   for ( ; ; ) {
       iov[0].iov_base = buf;
       iov[0].iov_len  = sizeof(buf);
       msg.msg_iov     = iov;
       msg.msg_iovlen  = 1;
       msg.msg_name    = NULL;
       msg.msg_namelen = 0;
       if (cmptr == NULL && (cmptr = (cmsghdr*)malloc(CONTROLLEN)) == NULL)
           return(-1);
       msg.msg_control    = cmptr;
       msg.msg_controllen = CONTROLLEN;
       if ((nr = recvmsg(fd, &msg, 0)) < 0) {
           err_sys("recvmsg error");
       } else if (nr == 0) {
           err_ret("connection closed by server");
           return(-1);
       }
       /*
        * See if this is the final data with null & status.  Null
        * is next to last byte of buffer; status byte is last byte.
        * Zero status means there is a file descriptor to receive.
        */
       for (ptr = buf; ptr < &buf[nr]; ) {
           if (*ptr++ == 0) {
               if (ptr != &buf[nr-1])
                   err_dump("message format error");
               status = *ptr & 0xFF;  /* prevent sign extension */
               if (status == 0) {
                   if (msg.msg_controllen != CONTROLLEN)
                       err_dump("status = 0 but no fd");
                   newfd = *(int *)CMSG_DATA(cmptr);
               } else {
                   newfd = -status;
               }
               nr -= 2;
           }
        }
        //if (nr > 0 && (*userfunc)(STDERR_FILENO, buf, nr) != nr)
        //    return(-1);
        if (status >= 0)    /* final data has arrived */
            return(newfd);  /* descriptor, or -status */
   }
   return -1;
}

