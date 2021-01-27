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

void hello();
int main()
{
while(1)
{
signal(SIGINT, hello);
}}

void hello()
{
printf("hellasdkljfso\n");
}

