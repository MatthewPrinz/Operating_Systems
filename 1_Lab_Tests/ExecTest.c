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

int main() 
{
    char* flags[3] = {"garbage", "-l", (char*)NULL};
    //char* flags[3] = {"ls", "-l", (char*)NULL};
    int currentProcess = fork();
    if (currentProcess ==0)
    {
       execStatus = execvp("garbage", flags);
       printf("execStatus: %d", execStatus);
    }
    else
    {
        int status;
        wait(&status);
        status = WEXITSTATUS(status);
        printf("execStatus is: %d", execStatus);
    }
}
