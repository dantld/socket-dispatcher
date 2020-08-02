#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>

bool setnonblocking(int sock)
{
    int opts;

    opts = fcntl(sock,F_GETFL);
    if (opts < 0) {
        perror("fcntl(F_GETFL)");
        return false;
    }
    opts = (opts | O_NONBLOCK);
    if (fcntl(sock,F_SETFL,opts) < 0) {
        perror("fcntl(F_SETFL)");
        return false;
    }
    return true;
}
