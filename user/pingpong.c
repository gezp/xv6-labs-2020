#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[])
{
    int pid;
    int fd[2];
    char buf[4];
    pipe(fd);
    pid = fork();
    if (pid > 0) {
        //parent
        write(fd[1], "o", 1);
        wait(0);
        read(fd[0], buf, 1);
        fprintf(1, "%d: received pong\n", getpid());
    } else if(pid == 0) {
        //child
        read(fd[0], buf, 1);
        fprintf(1, "%d: received ping\n", getpid());
        write(fd[1], buf, 1);
        exit(0);
    } else{
        fprintf(1, "Error: failed to fork!\n");
        exit(1);
    }
    exit(0);
}