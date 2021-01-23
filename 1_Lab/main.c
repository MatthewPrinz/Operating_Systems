#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>

int S_STRINGS_LENGTH = 6;
char **tokenizedCommand;
// ORDER MATTERS - WE SWITCH BASED OFF OF INDEX POSITION
char *specialStrings[6] = {"|", ">", "<", ">>", "2>", "&"};
void piping();
void redirectOutput();
void redirectInput();
void appendToFile();
void redirectError();
void JOBCONTROL(); // name to change
char **parseString(char *str); //parses an command into strings

char **parseString(char *str) {
    char *token = strtok(str, " ");
    int tokens = 5;
    int charInToken = 5;
    tokenizedCommand = (char **) malloc(sizeof(char *) * tokens);
    for (int i = 0; i < tokens; i++) {
        tokenizedCommand[i] = (char *) malloc(sizeof(char) * charInToken);
    }
    int numTokens = 0;
    while (token != NULL) {
        tokenizedCommand[numTokens] = token;
        numTokens++;
        token = strtok(NULL, " ");
    }
    tokenizedCommand[numTokens] = (char *) NULL;
    for (int i = 0; i < S_STRINGS_LENGTH; i++)
    {
        for (int j = 0; j < numTokens; j++)
        {
            if (strcmp(specialStrings[i], tokenizedCommand[j]) == 0)
            {
                switch(i)
                {
                    // ">"
                    case 1:
                        redirectOutput();
                        break;
                    case 2:


                }
            }
        }
    }
    return tokenizedCommand;
}

/* exec Example 2 */
int main() {
    int cpid;
    char **parsedcmd;
    char *inString;
    while (inString = readline("# ")) {
        parsedcmd = parseString(inString);
        cpid = fork();
        if (cpid == 0) {
            execvp(parsedcmd[0], parsedcmd);
        } else {
            free(inString);
            wait((int *) NULL);
        }
    }
}



// parseString function omitted (read the man page for strtok)
