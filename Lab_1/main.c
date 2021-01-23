#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
char ** parseString(char * str); //parses an command into strings

char** parseString(char* str)
{
	char* token = strtok(str, " ");
    int r = 5;
    int c = 5;
    char** result = (char**)malloc(sizeof(char*)*r);
    for (int i = 0; i < r; i++)
    {
        result[i] = (char*)malloc(sizeof(char)*c);
    }
    int j = 0;
    while (token != NULL)
    {
        result[j] = token;
        j++;
        token = strtok(NULL, " ");
    }
    result[j] = (char*)NULL;
    return result;
}
/* exec Example 2 */
int main(){
    int cpid;
    char **parsedcmd;
    char buffer[20];
    while(1){
        printf("# ");
        scanf("%s", buffer);
        parsedcmd = parseString(buffer);
        cpid = fork();
        if (cpid == 0){
            execvp(parsedcmd[0],parsedcmd);
        }else{
            wait((int *)NULL);
        }
    }
}

// parseString function omitted (read the man page for strtok)
