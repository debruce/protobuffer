// #include "Filer.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
// #include <filesystem>
#include <iostream>

using namespace std;

int main(int argc, char *argv[])
{
    int pair[2];

    socketpair(AF_UNIX, SOCK_DGRAM, 0, pair);

    int pid = fork();
    if (pid > 0) { // parent
        int mem_fd = memfd_create("dummy", 0);
        write(mem_fd, "hello world\n", 12);

        struct msghdr msg = { 0 };
        char buf[CMSG_SPACE(sizeof(mem_fd))];
        memset(buf, '\0', sizeof(buf));
        char abc[4];
        strncpy(abc, "ABC", 3);
        struct iovec io = { .iov_base = abc, .iov_len = 3 };

        msg.msg_iov = &io;
        msg.msg_iovlen = 1;
        msg.msg_control = buf;
        msg.msg_controllen = sizeof(buf);

        struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(mem_fd));

        *((int *) CMSG_DATA(cmsg)) = mem_fd;

        msg.msg_controllen = CMSG_SPACE(sizeof(mem_fd));

        if (sendmsg(pair[0], &msg, 0) < 0) {
            cerr << "failed to send message" << endl;
        }
    }
    else { // child
        struct msghdr msg = {0};

        char m_buffer[256];
        struct iovec io = { .iov_base = m_buffer, .iov_len = sizeof(m_buffer) };
        msg.msg_iov = &io;
        msg.msg_iovlen = 1;

        char c_buffer[256];
        msg.msg_control = c_buffer;
        msg.msg_controllen = sizeof(c_buffer);

        if (recvmsg(pair[1], &msg, 0) < 0) {
            cerr << "failed to receive message" << endl;
        }

        struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msg);

        unsigned char * data = CMSG_DATA(cmsg);

        // err_remark("About to extract fd\n");
        int fd = *((int*) data);
        // err_remark("Extracted fd %d\n", fd);
        int position = lseek(fd, 0, SEEK_CUR);

        cout << "position = " << position << endl;
        lseek(fd, 0, SEEK_SET);
        char buf[32];
        int len = read(fd, buf, position);
        if (len > 0) {
            buf[len] = 0;
            cout << "read " << buf << endl;
        }
        close(fd);

        return fd;
    }
}