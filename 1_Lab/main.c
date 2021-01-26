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
/**
 * Data structures I need to create:
 * job array?
 * process list?
 * Pretty obvious commands I need to use -
 * fork
 * exec
 * wait
 * signal
 * pipe
 * dup2
 * open
 * creat
 * write
 * read
 * setpgid
 * getpgid
 * tcsetpgrp
 * tcgetpgrp
 * signal
 * kill - both positive & negative, positive for processes, negative for ctrl c & ctrl z
 *
 */

int S_STRINGS_LENGTH = 6;
// max num jobs <= 20
// & always at the end
int numJobs = 0;

char **tokenizedCommand;
// ORDER MATTERS - WE SWITCH BASED OFF OF INDEX POSITION
char *specialStrings[6] = {"|", ">", "<", ">>", "2>", "&"};
void parseString(char *str); //parses an command into strings
void piping();
void redirectOutput(int tokenIndex);
void redirectInput();
void appendToFile();
void redirectError();
void JOBCONTROL(); // name to change

void parseString(char *str) {
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
    for (int sStringsIndex = 0; sStringsIndex < S_STRINGS_LENGTH; sStringsIndex++)
    {
        for (int tokenIndex = 0; tokenIndex < numTokens; tokenIndex++)
        {
            if (strcmp(specialStrings[sStringsIndex], tokenizedCommand[tokenIndex]) == 0)
            {
                switch(sStringsIndex)
                {
                    // "|"
                    case 0:
                        break;
                    // ">"
                    case 1:
                        redirectOutput(tokenIndex);
                        break;
                    // "<"
                    case 2:
                        break;
                    // ">>"
                    case 3:
                        break;
                    // "2>"
                    case 4:
                        break;
                    // "&"
                    case 5:
                        break;
                }
            }
        }
    }
}
/**
 * For dealing with ">"
 * @param tokenIndex
 */
void redirectOutput(int tokenIndex)
{
    // tokenIndex + 1 => file name
    open(tokenizedCommand[tokenIndex + 1], O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

}

int main() {
    int cpid;
    char *inString;
    while (1) {
        inString = readline("# ");
        printf("instring: %s", inString);
        parseString(inString);
        cpid = fork();
        if (cpid == 0) {
            execvp(tokenizedCommand[0], tokenizedCommand);
        } else {
            free(inString);
            wait((int *) NULL);
        }
    }
}
