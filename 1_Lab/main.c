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

// max num jobs <= 20
// & always at the end
int numJobs = 0;

typedef struct Commands {
    char *command;
    char *fexec[2000];
    char *argv[2000];
    int numArgs;
    bool hasInRedirect;
    char *inRedirectFileName;
    bool hasOutRedirect;
    char *outRedirectFileName;
    bool hasErrorRedirect;
    char *errorRedirectFileName;
    bool hasPipe;
    pid_t pid;
} command_t;

// Has all the related to commands in one simple struct
command_t input[2];

int commandIndex;
int numCommands = 1;
// will be the pid
pid_t currentProcess;

void freeCommands();

void parseString(char *str); //parses an command into strings
void stopHandler(int signum);

void intHandler(int signum);

void piping();

void redirectOutput();

void redirectInput();

void redirectError();

void JOBCONTROL(); // name to change

void parseString(char *str) {
    char *tokenizedCommand[2000];
    // Need to initialize numArgs for my struct
    input[0].numArgs = 0;
    int cmdIndex = 0;
    int numTokens = 0;
    char *cl_copy, *token, *save_ptr;
    cl_copy = strdup(str);

    // Used to note that the next token to be analyzed will be an actual command that can be
    // ran by the shell.
    bool recognizeCommand = true;
    // Used to show the location of the file name in tokenizedCommand
    int inRedirectIndex, outRedirectIndex, errorRedirectIndex;
    int argvIndex = 0;

    while ((token = strtok_r(cl_copy, " ", &save_ptr))) {
        tokenizedCommand[numTokens] = token;
        if (recognizeCommand) {
            input[cmdIndex].command = strdup(token);
            recognizeCommand = false;
        }
        else if (strcmp(token, ">") == 0) {
            input[cmdIndex].hasOutRedirect = true;
            outRedirectIndex = numTokens + 1;
        }
        else if (strcmp(token, "<") == 0) {
            input[cmdIndex].hasInRedirect = true;
            inRedirectIndex = numTokens + 1;
        }
        else if (strcmp(token, "2>") == 0) {
            input[cmdIndex].hasErrorRedirect = true;
            errorRedirectIndex = numTokens + 1;
        }
        else if (strcmp(token, "|") == 0) {
            input[cmdIndex].hasPipe = true;
            cmdIndex++;
            numCommands++;
            argvIndex = 0;
            input[cmdIndex].numArgs = 0;
            recognizeCommand = true;
        }
        else if (token[0] == '-' || isalnum(token[0])) {
            // We'll be grabbing the redirection file names in the for loop, so these
            // should just be argv given to the command
            if ((numTokens != inRedirectIndex) &&
                (numTokens != outRedirectIndex) &&
                (numTokens != errorRedirectIndex)) {
                input[cmdIndex].argv[argvIndex] = strdup(token);
                input[cmdIndex].numArgs++;
                argvIndex++;
            }
        }
        numTokens++;
        cl_copy = NULL;
    }
    for (int i = 0; i < numCommands; i++) {
        if (input[i].hasOutRedirect) {
            input[i].outRedirectFileName = strdup(tokenizedCommand[outRedirectIndex]);
        }
        if (input[i].hasInRedirect) {
            input[i].inRedirectFileName = strdup(tokenizedCommand[inRedirectIndex]);
        }
        if (input[i].hasErrorRedirect) {
            input[i].errorRedirectFileName = strdup(tokenizedCommand[errorRedirectIndex]);
        }
        input[i].fexec[0] = input[i].command;
        int j;
        for (j = 1; j < input[i].numArgs + 1; j++)
        {
            input[i].fexec[j] = input[i].argv[j - 1];
        }
        input[i].fexec[j+1] = (char*) NULL;
    }
    tokenizedCommand[numTokens] = (char *) NULL;
    free(str);
}
/**
 * For dealing with ">"
 */
void redirectOutput() {
    int fd = creat(input[commandIndex].outRedirectFileName, 0644);
    dup2(fd, STDOUT_FILENO);
}
/**
 * For dealing with "<"
 */
void redirectInput()
{
    int fd = open(input[commandIndex].inRedirectFileName, O_RDONLY);
    dup2(fd, STDIN_FILENO);
}

void redirectError()
{
    int fd = creat(input[commandIndex].errorRedirectFileName, 0644);
    dup2(fd, STDERR_FILENO);
}

void intHandler(int signum) {
    kill(SIGINT, currentProcess);
    printf("Received int signal");
}

void stopHandler(int signum) {
    kill(SIGTSTP, currentProcess);
    printf("Received stop signal");
}

void freeCommands() {
    for (int i = 0; i < numCommands; i++) {
        free(input[i].command);
        if (input[i].numArgs > 0) {
            for (int j = 0; j < input[i].numArgs; j++) {
                free(input[i].argv[j]);
            }
        }
        if (input[i].hasInRedirect) {
            free(input[i].inRedirectFileName);
            input[i].hasInRedirect = false;
        }
        if (input[i].hasOutRedirect) {
            free(input[i].outRedirectFileName);
            input[i].hasOutRedirect = false;
        }
        if (input[i].hasErrorRedirect) {
            free(input[i].errorRedirectFileName);
            input[i].hasErrorRedirect = false;
        }
        for (int j = 0; input[i].fexec[j] != (char *) NULL; j++) {
            input[i].fexec[j] = (char *) NULL;
        }
    }
}

void piping()
{

}

int main() {
    char *inString;
    while (1) {
        signal(SIGINT, intHandler);
        signal(SIGSTOP, stopHandler);
        inString = readline("# ");
        if (inString == NULL)
            break;
        parseString(inString);
        if (input[0].hasPipe)
        {
            piping();
        }
        else {
            currentProcess = fork();
            if (currentProcess == 0) {
                if (input[commandIndex].hasInRedirect) {
                    redirectInput();
                }
                if (input[commandIndex].hasOutRedirect) {
                    redirectOutput();
                }
                if (input[commandIndex].hasErrorRedirect) {
                    redirectError();
                }
                execvp(input[commandIndex].command, input[commandIndex].fexec);
            } else {
                int status;
                wait(&status);
                // frees all my strdup'd memory & reinitalizes the input array so it's
                // ready to take in more input
                freeCommands();
            }
        }
    }
}