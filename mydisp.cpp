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

#include "utils.h"

enum class AcceptSocketType : uint16_t {
    UNIX,
    TCP,
    NONE
};

enum class ErrorType : uint16_t {
    ERROR,
    TIMEOUT,
    NONE,
};

AcceptSocketType check_for_read(int tcp_fd, int unix_fd, ErrorType& errorType, int &error);
int recv_fd(int fd, ssize_t (*userfunc)(int, const void *, size_t));


int main(int argc, char *argv[])
{
    if( argc != 2 ) {
        fprintf(stderr, "Dispatcher started without argument, exiting...\n");
        return 0;
    }
    printf("Dispatcher started\n");
    printf("Try connect to: %s\n", argv[1]);

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

    while(1) {
        ErrorType errorType;
        int error;
        AcceptSocketType ast = check_for_read(-1, sfd, errorType, error);
        if( ast == AcceptSocketType::NONE ) {
        if( errorType == ErrorType::ERROR ) {
            printf("ERROR\n");
            break;
        }
        if( errorType == ErrorType::TIMEOUT ) {
            //printf("TIMEOUT\n");
        }
        } else if( ast == AcceptSocketType::UNIX) {
            char message[200];
            ssize_t read_size = recv(sfd, message, 200, MSG_PEEK);
            if(!read_size) {
                fprintf(stderr,"ERROR: read zero bytes from dispatcher socket, exiting...\n");
                break;
            }
            printf("ready for read data from unix socket [%d]\n", read_size);
            if(read_size == 2) {
                int s = recv_fd(sfd, NULL);
                write(s,"HELLO!!!!\n",10);
                close(s);
            } else if( memcmp(message,"CONFIG",6) == 0 ) {
                ssize_t read_size = recv(sfd, message, 200, 0);
                printf("INFO: Dispatcher config received\n");
            }
        }
    }

    close(sfd);
    printf("Exit\n");
    return 0;
}

AcceptSocketType check_for_read(int tcp_fd, int unix_fd, ErrorType& errorType, int &error)
{
    errorType = ErrorType::NONE;
    error = 0;
    fd_set set, errset;
    struct timeval timeout;
    int rv;

    timeout.tv_sec  = 5;
    timeout.tv_usec = 0;
    FD_ZERO(&set);
    //if( tcp_fd > 0 ) FD_SET(tcp_fd,  &set);
    FD_SET(unix_fd, &set);
    FD_SET(unix_fd, &errset);
    //int max_fd = tcp_fd > unix_fd ? tcp_fd : unix_fd;
    rv = select(unix_fd + 1, &set, NULL, &errset, &timeout);
    if(rv == -1) {
        error = errno;
        errorType = ErrorType::ERROR;
        perror("disp select");
        return AcceptSocketType::NONE;
    } else if(rv == 0) {
        errorType = ErrorType::TIMEOUT;
        printf("disp timeout occurred\n");
        return AcceptSocketType::NONE;
    } else if( tcp_fd > 0 && FD_ISSET(tcp_fd,&set) ) {
        return AcceptSocketType::TCP;
    } else if( FD_ISSET(unix_fd,&set) ) {
        return AcceptSocketType::UNIX;
    } else if( FD_ISSET(unix_fd,&errset) ) {
        errorType = ErrorType::ERROR;
        fprintf(stderr, "disp unix socket has error state\n");
    }
    assert(0);
    return AcceptSocketType::NONE;
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

