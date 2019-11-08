#define _XOPEN_SOURCE 600

#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <libgen.h>
#include <termios.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define BUF_SIZE 1024

int
main(int argc, char *argv[]) {
    int master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (master_fd == -1)
        exit(-1);

    if (grantpt(master_fd) == -1)
        exit(-1);

    if (unlockpt(master_fd) == -1)
        exit(-1);

    char* pty_name = ptsname(master_fd);
    if (pty_name == NULL)
        exit(-1);

    pid_t child_pid = fork();
    if (child_pid == -1)
        exit(-1);

    // child
    if (child_pid == 0) {
        if (setsid() == -1)
            exit(-1);

        close(master_fd);

        int slave_fd = open(pty_name, O_RDWR);
        if (slave_fd == -1)
            exit(-1);

        if (dup2(slave_fd, STDIN_FILENO) != STDIN_FILENO)
            exit(-1);
        if (dup2(slave_fd, STDOUT_FILENO) != STDOUT_FILENO)
            exit(-1);
        if (dup2(slave_fd, STDERR_FILENO) != STDERR_FILENO)
            exit(-1);

        execlp("/bin/bash", "/bin/bash", NULL);
    }

    // parent
    int output_fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC);
    if (output_fd == -1)
        exit(-1);

    fd_set in_fds;
    char buf[BUF_SIZE];
    for (;;) {
        FD_ZERO(&in_fds);
        FD_SET(STDIN_FILENO, &in_fds);
        FD_SET(master_fd, &in_fds);

        if (select(master_fd+1, &in_fds, NULL, NULL, NULL) == -1)
            exit(-1);

        // stdin -> pty
        if (FD_ISSET(STDIN_FILENO, &in_fds)) {
            int n = read(STDIN_FILENO, buf, BUF_SIZE);
            if (n <= 0) {
                exit(0);
            }

            if (write(master_fd, buf, n) != n)
                exit(-1);
        }

        // pty -> pipe
        if (FD_ISSET(master_fd, &in_fds)) {
            int n = read(master_fd, buf, BUF_SIZE);
            if (n <= 0)
                exit(0);

            if (write(output_fd, buf, n) != n)
                exit(-1);
        }
    }
}
