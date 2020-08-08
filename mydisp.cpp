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

#include "utils.h"

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
                fprintf(stderr,"ERROR: invalide socket POLLINVAL event has come.\n");
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


int main(int argc, char *argv[])
{
    if( argc != 2 ) {
        fprintf(stderr, "Dispatcher started without argument, exiting...\n");
        return 0;
    }
    printf("Dispatcher started\n");
    printf("Try connect to: %s\n", argv[1]);

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
                    if( sockets[i].revents & POLLERR ||
                        sockets[i].revents & POLLHUP ||
                        sockets[i].revents & POLLRDHUP) {
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


/* size of control buffer to send/recv one file descriptor */
#define CONTROLLEN  CMSG_LEN(sizeof(int))

static struct cmsghdr   *cmptr = NULL;      /* malloc'ed first time */
#define err_sys(msg)  fprintf(stderr,"ERROR:  %s\n",msg)
#define err_ret(msg)  fprintf(stderr,"RETURN: %s\n",msg)
#define err_dump(msg) fprintf(stderr,"DUMP:   %s\n",msg)
const int MAXLINE=128;

/*
 * Receive a file descriptor from a server process.  Also, any data
 * received is passed to (*userfunc)(STDERR_FILENO, buf, nbytes).
 * We have a 2-byte protocol for receiving the fd from send_fd().
 */
int recv_fd(int fd, ssize_t (*userfunc)(int, const void *, size_t))
{
   int             newfd, nr, status;
   char            *ptr;
   char            buf[MAXLINE];
   struct iovec    iov[1];
   struct msghdr   msg;

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
}

