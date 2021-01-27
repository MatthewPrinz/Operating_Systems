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


// will be the pid
pid_t currentProcess;

char *tokenizedCommand[2000];
// ORDER MATTERS - WE SWITCH BASED OFF OF INDEX POSITION
char *specialStrings[6] = {"|", ">", "<", ">>", "2>", "&"};
void parseString(char *str); //parses an command into strings
void stopHandler(int signum);
void intHandler(int signum);
void piping();
void redirectOutput(int tokenIndex);
void redirectInput();
void appendToFile();
void redirectError();
void JOBCONTROL(); // name to change

void parseString(char *str)
{
    char *cl_copy, *token, *save_ptr;
    cl_copy = strdup(str);
	int numTokens = 0;
	while ((token = strtok_r(cl_copy, " ", &save_ptr))) {
        tokenizedCommand[numTokens] = token;
    	numTokens++;
    	/* First argument to strtok_r must be NULL after the first iteration. */
    	cl_copy = NULL;
	}
	tokenizedCommand[numTokens] = (char*)NULL;
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

/*void parseString(char *str) {
    char *token = strtok_r(str, " ");
    int tokens = 2000;
    int charInToken = 30;
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

}
 */
/**
 * For dealing with ">"
 * @param tokenIndex
 */
void redirectOutput(int tokenIndex)
{
    // tokenIndex + 1 => file name
//    open(tokenizedCommand[tokenIndex + 1], O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
    int fd = creat(tokenizedCommand[tokenIndex + 1], 0644);
    dup2(fd, 1);
}

void intHandler(int signum)
{
    kill(SIGKILL, currentProcess);
    printf("Received int signal");
}

void stopHandler(int signum)
{
    kill(SIGSTOP, currentProcess);
    printf("Received stop signal");
}

int main() {
    char *inString;

    while (1) {
        signal(SIGINT, intHandler);
        signal(SIGSTOP, stopHandler);
        inString = readline("# ");
        parseString(inString);
        currentProcess = fork();
        if (currentProcess == 0) {
            execvp(tokenizedCommand[0], tokenizedCommand);
        } else {
            wait((int *) NULL);
            free(inString);
        }
    }
}
