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
} command_t;

typedef struct Jobs
{
    command_t commands[2];
    char* fullInput;
    int numCommands;
    pid_t pid1;
    pid_t pid2;
    bool runOnForeBackground;
    bool runInBackground;
    pid_t pgid;
    bool jobDone;
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

void freeJob(int index);

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
    jobs[jobsIndex].numCommands = numCommands;
    for (int i = 0; i < numCommands; i++)
    {
        jobs[jobsIndex].commands[i] = input[i];
    }
    numJobs++;
    jobs[jobsIndex].fullInput = strdup(str);
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
            printf("Received stop signal");
            kill((-1 * currentPgid), SIGTSTP);
//            if (runningTwoCommands)
//                kill((-1 * currentPgid), SIGTSTP);
//            else
//                kill(currentProcess, SIGTSTP);
            for (int i = 0; i < numJobs; i++) {
                for (int j = 0; j < jobs[i].numCommands; j++) {
                    jobs[i].runOnForeBackground = true;
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
            numJobs--;
            printf("Received SIGCHLD");
            break;
        case SIGCONT:
            break;
        default:
            printf("Signal number = %d, not handled", signum);
            break;
    }
}

void freeJob(int index)
{
    jobs[index].runInBackground = false;
    jobs[index].runOnForeBackground = false;
    jobs[index].numCommands = false;
    free(jobs[index].fullInput);
}

void fg()
{
    for (int i = 0; i < numJobs; i++)
    {
        if (jobs[i].runOnForeBackground)
            kill(-1*(jobs[i].pgid), SIGCONT);
    }
}

void bg()
{
    for (int i = 0; i < numJobs; i++) {
        if (jobs[i].runOnForeBackground)
            kill(-1 * (jobs[i].pgid), SIGCONT);
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


void executeTwoCommands() {
    int pipefd[2];
    int status, pid_ch1, pid_ch2, pid;
    pipe(pipefd);
    pid_ch1 = fork();
    if (pid_ch1 > 0) {
        jobs[jobsIndex].pid1 = pid_ch1;
        printf("Child1 pid = %d\n", pid_ch1);
        currentPgid = pid_ch1;
        // Parent
        pid_ch2 = fork();
        if (pid_ch2 > 0) {
            jobs[jobsIndex].pid2 = pid_ch2;
            printf("Child2 pid = %d\n", pid_ch2);
            close(pipefd[0]); //close the pipe in the parent
            close(pipefd[1]);
            int count = 0;
            while (count < 2) {
                int stat = tcsetpgrp(0, pid_ch1);
                printf("tcsetpgrp returned: %d\n", stat);
                pid = waitpid(-1, &status, WUNTRACED | WCONTINUED);
                if (status == -1)
                {
                    // clean up job
                }
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
            freeCommands();
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
            if (execvp(input[1].command, input[1].fexec) == -1) {
                exit(-1);
            }

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
        if (execvp(input[0].command, input[0].fexec) == -1) {
            exit(-1);
        }
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
        int status = 100;
        if (!jobs[jobsIndex].runInBackground) {
            currentProcess = waitpid(-1, &status, WUNTRACED | WCONTINUED);
            // frees all my strdup'd memory & reinitalizes the input array so it's
            // ready to take in more input
            freeCommands();
        }
        // wait does not take options:
        //    waitpid(-1,&status,0) is same as wait(&status)
        // with no options waitpid wait only for terminated child processes
        // with options we can specify what other changes in the child's status
        // we can respond to. Here we are saying we want to also know if the child
        // has been stopped (WUNTRACED) or continued (WCONTINUED)
        if (status == -1)
        {
            // clean up jobtable
        }
        if (WIFEXITED(status)) {
            printf("child %d exited, status=%d\n", currentProcess, WEXITSTATUS(status));
        } else if (WIFSTOPPED(status)) {
            printf("%d stopped by signal %d\n", currentProcess, WSTOPSIG(status));
            // PLACE HOLDER
        } else if (WIFCONTINUED(status)) {
            printf("Continuing %d\n", currentProcess);
            // PLACE HOLDER
        }
    }
}

void printJobs()
{
    int numJobsFinished = 0;
    for (int i = 0; i < numJobs; i++)
    {
        if (jobs[i].jobDone) {
            printf("[%d]- Done %s", i, jobs[i].fullInput);
            numJobsFinished++;
            freeJob(i);
        }
    }
    for (int i = 0; i < numJobs; i++)
    {

    }
    numJobs -= numJobsFinished;
}

int main() {
    char *inString;
    while (1) {
        signal(SIGINT, sigHandler);
        signal(SIGTSTP, sigHandler);
        signal(SIGCHLD, sigHandler);
        signal(SIGCONT, sigHandler);
        signal(SIGTTOU, sigHandler);
        inString = readline("# ");
        if (inString == NULL)
            break;
        parseString(inString);
        if (strcmp(input[0].command, "bg") == 0)
            bg();
        else if (strcmp(input[0].command, "fg") == 0)
            fg();
        else if (strcmp(input[0].command, "jobs") == 0)
            printJobs();
        else if (input[0].hasPipe)
        {
            executeTwoCommands();
        }
        else {
            executeOneCommand();
        }
    }
}