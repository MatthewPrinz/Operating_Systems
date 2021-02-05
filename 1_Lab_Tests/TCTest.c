#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
int execStatus;


void sigHandler(int signo)
{
    printf("received %d\n", signo);
}
int main() 
{
    // char* flags[3] = {"ls", "-l", (char*)NULL};
    signal(SIGTTOU, sigHandler);
    int retVal = setpgid(0, 0);
    if (retVal == 0)
        printf("setpgid success!\n");
    char* flags[3] = {"sleep", "10", (char*)NULL};
    pid_t parentPid = getpid();
    printf("parents pgid is: %d, parents pid is: %d\n", getpgid(parentPid), parentPid);
    int currentProcess = fork();
    // pid_t pid = tcgetpgrp(1);
    // printf("parent pid is: %d, tcgetpgrp is: %d, currentProcess is: %d\n", getpid(), pid, currentProcess);

    if (currentProcess == 0)
    {
        setpgid(getpid(), parentPid);
        // printf("parent pid is: %d, tcgetpgrp is: %d, currentProcess is: %d\n", getpid(), pid, currentProcess);
        printf("hello, my ppid is : %d", getppid());
        fflush(NULL);        
        execvp("sleep", flags);
    }
    else
    {
        // kill(-1*parentPid, SIGINT);
        // printf("Sending SIGINT to parentPid*_1 = %d\n", -1*parentPid);
        fflush(NULL);
        int status;
        wait(&status);
        // status = WEXITSTATUS(status);
    }
}
