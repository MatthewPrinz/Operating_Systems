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

void handler(int signum)
{
printf("signum is: %d", signum);
}


int main() 
{
while(1)
{
signal(SIGTSTP, handler);
signal(SIGINT, handler);
}
}
