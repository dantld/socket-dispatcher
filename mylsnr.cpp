/**
 * The main listener process.
 * Must drop using xined service for some my legacy services.
 */

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

static struct cmsghdr *cmptr = NULL;
#define CONTROLLEN  CMSG_LEN(sizeof(int))


int send_fd(int fd, int fd_to_send);

const char *MY_SOCK_PATH = "dispatch.sock";

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

static bool exitRequested = false;
void sig_INT(int)
{
    exitRequested = true;
}

void setnonblocking(int sock);

/**
 * @brief Check sockets for accept ready.
 * @return Type of socket which ready for accept.
 */
AcceptSocketType check_for_accept_connections(int tcp_fd, int unix_fd, ErrorType& errorType, int &error);

/**
 * @brief Create, bind and start to listen the server tcp socket.
 * @details Use socket for external connections.
 */
int create_listen_server_tcp_socket()
{
    int retVal;
    int socketfd;
    socketfd = socket(AF_INET,SOCK_STREAM,0);
    if(socketfd < 0 ) return -1;
    struct sockaddr_in srv_addr;
    memset( &srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port   = htons(5555);
    srv_addr.sin_addr.s_addr = INADDR_ANY;

    retVal = bind( socketfd, (sockaddr*)&srv_addr, sizeof(srv_addr) );
    if(retVal < 0 ) return -2;
    retVal = listen( socketfd, 100);
    if(retVal < 0 ) return -3;
    return socketfd;
}

/**
 * @brief Create, bind and start to listen the server unix socket.
 * @details Use this socket for accepting internal server modules
 * connections.
 */
int create_listen_server_unix_socket()
{
   int sfd;
   struct sockaddr_un my_addr;

   sfd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (sfd < 0) return -1;

    unlink(MY_SOCK_PATH);
    memset(&my_addr, 0, sizeof(struct sockaddr_un));
    my_addr.sun_family = AF_UNIX;
    strncpy( my_addr.sun_path, MY_SOCK_PATH, sizeof(my_addr.sun_path) - 1);

    size_t len = strlen(my_addr.sun_path) + sizeof(my_addr.sun_family);
    int retVal = bind(sfd, (struct sockaddr *) &my_addr, len);
    if( retVal < 0 ) return -2;
    retVal = listen( sfd, 3);
    if(retVal < 0 ) return -3;

    return sfd;
}

int accept_incoming_connection(int tcp_fd)
{
    int newfd = 0;
    struct sockaddr_in peer_addr;
    socklen_t peer_size = sizeof(peer_addr);
    newfd = accept(tcp_fd,(sockaddr*)&peer_addr,&peer_size);
    if( newfd < 0 ) {
        fprintf(stderr,"accept failed: %s\n", strerror(errno));
        return -1;
    }
    printf("Incoming Tcp Connection Has Accepted\n");
    return newfd;
}

int accept_dispatcher_connection(int unix_fd)
{
    struct sockaddr_un peer_addr;
    socklen_t peer_addr_size;

    peer_addr_size = sizeof(struct sockaddr_un);
//    int disp_fd = accept( unix_fd, (struct sockaddr *) &peer_addr, &peer_addr_size);
    int disp_fd = accept( unix_fd, NULL, NULL);
    if (disp_fd < 0) {
        fprintf(stderr,"accept dispatcher connection failed: %s\n", strerror(errno));
        return -1;
    }
    printf("Dispatcher Connection Has Accepted\n");
    return disp_fd;
}

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
    return 0;
}
const int DISPATCHERS_MAX = 10;
int dispatched_fds[DISPATCHERS_MAX];

int main(int argc, char *argv[])
{
    for(int i = 0; i < DISPATCHERS_MAX; i++ ) { dispatched_fds[i] = -1; }

    int tcp_fd = create_listen_server_tcp_socket();
    if(tcp_fd < 0 ) {
        fprintf(stderr, "Create TCP socket failed: %s", strerror(errno));
        return -tcp_fd;
    }

    int unix_fd = create_listen_server_unix_socket();
    if(unix_fd < 0) {
        fprintf(stderr, "Create UNIX socket failed: %s", strerror(errno));
        close(tcp_fd);
        return -unix_fd;
    }

    create_dispatcher();

    signal( SIGINT, sig_INT );
    while(!exitRequested) {
    AcceptSocketType acceptSocketType;
    while(!exitRequested) {
        ErrorType errorType;
        int error;
        acceptSocketType = check_for_accept_connections(tcp_fd, unix_fd, errorType, error);
        if(acceptSocketType == AcceptSocketType::NONE) {
            if(errorType == ErrorType::TIMEOUT) continue;
            if(errorType == ErrorType::ERROR  ) break;
        } else if(acceptSocketType == AcceptSocketType::TCP) {
            int new_fd = accept_incoming_connection(tcp_fd);
            if( new_fd < 0 ) {
                fprintf(stderr,"Accept incoming connection failed, exiting...\n");
                exitRequested = true;
                break;
            }
            /// @todo process new socket here.
            printf("Send accpted TCP socket to dispatcher process\n");
            send_fd(dispatched_fds[0], new_fd);
            close(new_fd);
        } else if(acceptSocketType == AcceptSocketType::UNIX) {
            int new_fd = accept_dispatcher_connection(unix_fd);
            if( new_fd < 0 ) {
                fprintf(stderr,"Accept dispatcher connection failed, exiting...\n");
                exitRequested = true;
                break;
            }
            dispatched_fds[0] = new_fd;
            /// @todo process new socket here.
            char buffer[128];
            int r = snprintf(buffer,128,"CONFIG:PPID=%d\n",(int)getpid());
            assert(r<128);
            write( new_fd, buffer, r );
            //close(new_fd);
        }
    }
    }
    for(int i = 0; i < DISPATCHERS_MAX; i++ ) { if(dispatched_fds[i] > 0) close(dispatched_fds[i]); }
    close(tcp_fd);
    close(unix_fd);
    unlink(MY_SOCK_PATH);

    return 0;
}

/*
 * Pass a file descriptor to another process.
 * If fd<0, then -fd is sent back instead as the error status.
 */
int send_fd(int fd, int fd_to_send)
{
    struct iovec    iov[1];
    struct msghdr   msg;
    char            buf[2]; /* send_fd()/recv_fd() 2-byte protocol */

    iov[0].iov_base = buf;
    iov[0].iov_len  = 2;
    msg.msg_iov     = iov;
    msg.msg_iovlen  = 1;
    msg.msg_name    = NULL;
    msg.msg_namelen = 0;
    if (fd_to_send < 0) {
        msg.msg_control    = NULL;
        msg.msg_controllen = 0;
        buf[1] = -fd_to_send;   /* nonzero status means error */
        if (buf[1] == 0)
            buf[1] = 1; /* -256, etc. would screw up protocol */
    } else {
        if (cmptr == NULL && (cmptr = (cmsghdr*)malloc(CONTROLLEN)) == NULL)
            return(-1);
        cmptr->cmsg_level  = SOL_SOCKET;
        cmptr->cmsg_type   = SCM_RIGHTS;
        cmptr->cmsg_len    = CONTROLLEN;
        msg.msg_control    = cmptr;
        msg.msg_controllen = CONTROLLEN;
        *(int *)CMSG_DATA(cmptr) = fd_to_send;     /* the fd to pass */
        buf[1] = 0;          /* zero status means OK */
    }
    buf[0] = 0;              /* null byte flag to recv_fd() */
    if (sendmsg(fd, &msg, 0) != 2)
        return(-1);
    return(0);
}

AcceptSocketType check_for_accept_connections(int tcp_fd, int unix_fd, ErrorType& errorType, int &error)
{
    errorType = ErrorType::NONE;
    error = 0;
    fd_set set;
    struct timeval timeout;
    int rv;

    timeout.tv_sec  = 2;
    timeout.tv_usec = 0;
    FD_ZERO(&set);
    FD_SET(tcp_fd,  &set);
    FD_SET(unix_fd, &set);
    int max_fd = tcp_fd > unix_fd ? tcp_fd : unix_fd;
    rv = select(max_fd + 1, &set, NULL, NULL, &timeout);
    if(rv == -1) {
        error = errno;
        errorType = ErrorType::ERROR;
        perror("select");
        return AcceptSocketType::NONE;
    } else if(rv == 0) {
        errorType = ErrorType::TIMEOUT;
        //printf("timeout occurred\n");
        return AcceptSocketType::NONE;
    } else if( FD_ISSET(tcp_fd,&set) ) {
        return AcceptSocketType::TCP;
    } else if( FD_ISSET(unix_fd,&set) ) {
        return AcceptSocketType::UNIX;
    }
    assert(0);
    return AcceptSocketType::NONE;
}

