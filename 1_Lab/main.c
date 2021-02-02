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
pid_t currentPgid;
bool runningTwoCommands = false;
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
    pid_t pgid;
    pid_t pid;
} command_t;

typedef struct Jobs
{
    command_t commands[2];
    int numCommands;
    bool runOnForeBackground;
    bool runInBackground;
} job_t;

// Has all the related to commands in one simple struct
command_t input[2];
int numJobs = 0;
job_t jobs[20];


int jobsIndex = 0;
int numCommands = 1;
pid_t currentProcess;

void freeCommands();
void parseString(char *str); //parses an command into strings

void sigHandler(int signum);

void executeTwoCommands();

void executeOneCommand();

void redirectOutput(int inputIndex);

void redirectInput(int inputIndex);

void redirectError(int inputIndex);

void printJobs();

void bg();

void fg();

void parseString(char *str) {
    char *tokenizedCommand[2000];
    // Need to initialize my struct
    input[0].numArgs = 0;
    bool makeJob = false;
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
        else if (strcmp(token, "&") == 0)
        {
            numJobs++;
            jobs[jobsIndex].runInBackground = true;
            makeJob = true;
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
    if (makeJob)
    {
        jobs[jobsIndex].numCommands = numCommands;
        for (int i = 0; i < numCommands; i++)
        {
            jobs[jobsIndex].commands[i] = input[i];
        }
    }
    free(str);
}
/**
 * For dealing with ">"
 */
void redirectOutput(int inputIndex) {
    int fd = creat(input[inputIndex].outRedirectFileName, 0644);
    dup2(fd, STDOUT_FILENO);
}
/**
 * For dealing with "<"
 */
void redirectInput(int inputIndex)
{
    int fd = open(input[inputIndex].inRedirectFileName, O_RDONLY);
    dup2(fd, STDIN_FILENO);
}

void redirectError(int inputIndex)
{
    int fd = creat(input[inputIndex].errorRedirectFileName, 0644);
    dup2(fd, STDERR_FILENO);
}

void sigHandler(int signum)
{
    switch (signum) {
        case SIGTSTP:
            if (runningTwoCommands)
                kill((-1 * currentPgid), SIGTSTP);
            else
                kill(currentProcess, SIGTSTP);
            for (int i = 0; i < numJobs; i++) {
                for (int j = 0; j < jobs[i].numCommands; j++) {
                    jobs[i].runOnForeBackground = true;
                    printf("Received stop signal");
                    // one of the commands will need to set runOnForeBackground true.
                }
            }
            break;
        case SIGINT:
            if (runningTwoCommands)
                kill((-1 * currentPgid), SIGINT);
            else
                kill(currentProcess, SIGINT);
            break;
        case SIGCHLD:
            // going to involve updating jobs
            break;
    }
}

void fg()
{

}

void bg()
{
    for (int i = 0; i < numJobs; i++)
    {
    }
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

//void executeTwoCommands() {
//    runningTwoCommands = true;
//    int pipefd[2];
//    int pid;
//    int count = 0;
//    int status;
//    pipe(pipefd);
//    close(pipefd[0]);
//    close(pipefd[1]);
//    currentProcess = fork();
//    // child 1
//    if (currentProcess == 0) {
//        setpgid(0, 0);
//        close(pipefd[0]);
//        dup2(pipefd[1], STDOUT_FILENO);
//        if (input[commandIndex].hasInRedirect) {
//            redirectInput();
//        }
//        if (input[commandIndex].hasOutRedirect) {
//            redirectOutput();
//        }
//        if (input[commandIndex].hasErrorRedirect) {
//            redirectError();
//        }
//        execvp(input[commandIndex].command, input[commandIndex].fexec);
//    }
//    else {
//        while (count < 2) {
//            pid = waitpid(-1, &status, WUNTRACED | WCONTINUED);
//            // wait does not take options:
//            //    waitpid(-1,&status,0) is same as wait(&status)
//            // with no options waitpid wait only for terminated child processes
//            // with options we can specify what other changes in the child's status
//            // we can respond to. Here we are saying we want to also know if the child
//            // has been stopped (WUNTRACED) or continued (WCONTINUED)
//            if (WIFEXITED(status)) {
//                printf("child %d exited, status=%d\n", pid, WEXITSTATUS(status));
//                count++;
//            } else if (WIFSIGNALED(status)) {
//                printf("child %d killed by signal %d\n", pid, WTERMSIG(status));
//                count++;
//            } else if (WIFSTOPPED(status)) {
//
//                printf("%d stopped by signal %d\n", pid, WSTOPSIG(status));
//                printf("Sending CONT to %d\n", pid);
//                sleep(4); //sleep for 4 seconds before sending CONT
//                kill(pid, SIGCONT);
//            } else if (WIFCONTINUED(status)) {
//                printf("Continuing %d\n", pid);
//            }
//        }
//    }
//    commandIndex++;
//    currentProcess = fork();
//    // child 2
//    if (currentProcess == 0) {
//        close(pipefd[1]);
//        dup2(pipefd[0], STDIN_FILENO);
//        if (input[commandIndex].hasInRedirect) {
//            redirectInput();
//        }
//        if (input[commandIndex].hasOutRedirect) {
//            redirectOutput();
//        }
//        if (input[commandIndex].hasErrorRedirect) {
//            redirectError();
//        }
//        execvp(input[commandIndex].command, input[commandIndex].fexec);
//    }
//}

void executeTwoCommands() {
    int pipefd[2];
    int status, pid_ch1, pid_ch2, pid;
    pid_ch1 = fork();
    if (pid_ch1 > 0) {
        printf("Child1 pid = %d\n", pid_ch1);
        // Parent
        pid_ch2 = fork();
        if (pid_ch2 > 0) {
            printf("Child2 pid = %d\n", pid_ch2);
            close(pipefd[0]); //close the pipe in the parent
            close(pipefd[1]);
            int count = 0;
            while (count < 2) {
                pid = waitpid(-1, &status, WUNTRACED | WCONTINUED);
                if (WIFEXITED(status)) {
                    printf("child %d exited, status=%d\n", pid, WEXITSTATUS(status));
                    count++;
                } else if (WIFSTOPPED(status)) {
                    printf("%d stopped by signal %d\n", pid, WSTOPSIG(status));
                    // PLACE HOLDER
                } else if (WIFCONTINUED(status)) {
                    printf("Continuing %d\n", pid);
                    // PLACE HOLDER
                }
            }
            exit(1);
        } else {
            //Child 2
            setpgid(0, pid_ch1); //child2 joins the group whose group id is same as child1's pid
            close(pipefd[1]); // close the write end
            dup2(pipefd[0], STDIN_FILENO);
            if (input[1].hasInRedirect) {
                redirectInput(1);
            }
            if (input[1].hasOutRedirect) {
                redirectOutput(1);
            }
            if (input[1].hasErrorRedirect) {
                redirectError(1);
            }
            execvp(input[1].command, input[1].fexec);
        }
    } else {
        // Child 1
        setpgid(0, 0);
        //   group id is same as his pid: pid_ch1
        close(pipefd[0]); // close the read end
        dup2(pipefd[1], STDOUT_FILENO);
        if (input[0].hasInRedirect) {
            redirectInput(0);
        }
        if (input[0].hasOutRedirect) {
            redirectOutput(0);
        }
        if (input[0].hasErrorRedirect) {
            redirectError(0);
        }
        execvp(input[0].command, input[0].fexec);
    }
}

void executeOneCommand() {
    runningTwoCommands = false;
    currentProcess = fork();
    if (currentProcess == 0) {
        if (input[0].hasInRedirect) {
            redirectInput(0);
        }
        if (input[0].hasOutRedirect) {
            redirectOutput(0);
        }
        if (input[0].hasErrorRedirect) {
            redirectError(0);
        }
        // exit if there's no command found
        if (execvp(input[0].command, input[0].fexec) == -1)
        {
            exit(-1);
        }
    } else {
        int status;
        int pid = waitpid(-1, &status, WUNTRACED | WCONTINUED);
        // wait does not take options:
        //    waitpid(-1,&status,0) is same as wait(&status)
        // with no options waitpid wait only for terminated child processes
        // with options we can specify what other changes in the child's status
        // we can respond to. Here we are saying we want to also know if the child
        // has been stopped (WUNTRACED) or continued (WCONTINUED)
        if (WIFEXITED(status)) {
            printf("child %d exited, status=%d\n", pid, WEXITSTATUS(status));
        } else if (WIFSTOPPED(status)) {
            printf("%d stopped by signal %d\n", pid, WSTOPSIG(status));
            // PLACE HOLDER
        } else if (WIFCONTINUED(status)) {
            printf("Continuing %d\n", pid);
            // PLACE HOLDER
        }
//        if (input[commandIndex].runInBackground) {
//            waitpid(currentProcess, &status, WNOHANG);
//        } else {
//            wait(&status);
//            printf("status is: %d\n", status);
//        }
        // frees all my strdup'd memory & reinitalizes the input array so it's
        // ready to take in more input
        freeCommands();
    }
}

int main() {
    char *inString;
    while (1) {
        signal(SIGINT, sigHandler);
        signal(SIGTSTP, sigHandler);
        signal(SIGCHLD, sigHandler);
        inString = readline("# ");
        if (inString == NULL)
            break;
        parseString(inString);
        if (strcmp(input[0].command, "bg") == 0)
            bg();
        else if (strcmp(input[0].command, "fg") == 0)
            fg();
        else if (input[0].hasPipe)
        {
            executeTwoCommands();
        }
        else {
            executeOneCommand();
        }
    }
}