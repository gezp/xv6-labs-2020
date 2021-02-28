#include "kernel/types.h"
#include "user/user.h"


//be carefully to close pipe file descriptor.
int main(int argc, char* argv[])
{
    int fd1[2], fd2[2];
    int pid, cpid, ret, prime, buf;
    pipe(fd1);
    pid = fork();
    if (pid > 0) {
        close(fd1[0]);
        //first parent process,generate data
        for (int i = 2; i <= 35; ++i) {
            write(fd1[1], &i, sizeof(int));
        }
        close(fd1[1]);
        wait(0);
    } else if (pid == 0) {
        close(fd1[1]);
        //loop child process
        while (1) {
            ret = read(fd1[0], &prime, sizeof(int));
            if (ret == 0) {
                close(fd1[0]);
                exit(1);
            }
            fprintf(1, "prime %d\n", prime);
            pipe(fd2);
            cpid = fork();
            if (cpid > 0) {
                //parent: filter (recv from left,send to right)
                close(fd2[0]);
                while (1) {
                    ret = read(fd1[0], &buf, sizeof(int));
                    if (ret == 0) {
                        close(fd1[0]);
                        close(fd2[1]);
                        wait(0);
                        exit(1);
                    }
                    if (buf % prime != 0) {
                        write(fd2[1], &buf, sizeof(int));
                    }
                }
            } else if (cpid == 0) {
                //child: update fd, do loop
                close(fd2[1]);
                close(fd1[0]);
                fd1[0]=fd2[0];
            } else {
                fprintf(1, "Error: failed to fork!\n");
                exit(1);
            }
        }
        exit(0);
    } else {
        fprintf(1, "Error: failed to fork!\n");
        exit(1);
    }
    exit(0);
}