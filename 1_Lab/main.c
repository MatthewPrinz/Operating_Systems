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
#include <errno.h>

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
#define DEBUG 1
// max num g_jobs <= 20
// & always at the end
pid_t g_currentPgid;
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
} Command_t;

typedef struct Jobs {
    Command_t commands[2];
    char *fullInput;
    int numCommands;
    bool runOnForeBackground;
    bool runInBackground;
    pid_t pgid;
    int jobNumber;
} Job_t;
// Has all the related to commands in one simple struct
Command_t input[2];
int g_numJobs = 0;
int g_jobsNumber = 0;
Job_t g_jobs[20];

int g_jobsIndex = 0;

void parseString(char *str); //parses an command into strings

void sigHandler(int signum);

void executeTwoCommands();

void executeOneCommand();

void redirectOutput(int inputIndex);

void redirectInput(int inputIndex);

void redirectError(int inputIndex);

void updateJobs();

void printJobs();

void freeJobs(int index);

void bg();

void fg();


void parseString(char *str) {
    char *tokenizedCommand[2000];
    // Need to initialize my struct
    input[0].numArgs = 0;
    if (g_numJobs == 0)
        g_jobs[g_jobsIndex].runOnForeBackground = true;
    g_jobs[g_jobsIndex].jobNumber = ++g_jobsNumber;
    g_jobs[g_jobsIndex].numCommands = 1;
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
        } else if (strcmp(token, ">") == 0) {
            input[cmdIndex].hasOutRedirect = true;
            outRedirectIndex = numTokens + 1;
        } else if (strcmp(token, "<") == 0) {
            input[cmdIndex].hasInRedirect = true;
            inRedirectIndex = numTokens + 1;
        } else if (strcmp(token, "2>") == 0) {
            input[cmdIndex].hasErrorRedirect = true;
            errorRedirectIndex = numTokens + 1;
        } else if (strcmp(token, "|") == 0) {
            input[cmdIndex].hasPipe = true;
            cmdIndex++;
            g_jobs[0].numCommands++;
            argvIndex = 0;
            input[cmdIndex].numArgs = 0;
            recognizeCommand = true;
        } else if (strcmp(token, "&") == 0) {
            g_jobs[g_jobsIndex].runInBackground = true;
        } else if (token[0] == '-' || isalnum(token[0])) {
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
    for (int i = 0; i < g_jobs[g_jobsIndex].numCommands; i++) {
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
        for (j = 1; j < input[i].numArgs + 1; j++) {
            input[i].fexec[j] = input[i].argv[j - 1];
        }
        input[i].fexec[j + 1] = (char *) NULL;
    }
    for (int i = 0; i < g_jobs[g_jobsIndex].numCommands; i++) {
        g_jobs[g_jobsIndex].commands[i] = input[i];
    }
    g_numJobs++;
    g_jobs[g_jobsIndex].fullInput = strdup(str);
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
void redirectInput(int inputIndex) {
    int fd = open(input[inputIndex].inRedirectFileName, O_RDONLY);
    dup2(fd, STDIN_FILENO);
}

void redirectError(int inputIndex) {
    int fd = creat(input[inputIndex].errorRedirectFileName, 0644);
    dup2(fd, STDERR_FILENO);
}

void sigHandler(int signum) {
    switch (signum) {
        case SIGTSTP:
            if (DEBUG)
                printf("Received stop signal");
            kill((-1 * g_currentPgid), SIGTSTP);
            for (int i = 0; i < g_numJobs; i++) {
                for (int j = 0; j < g_jobs[i].numCommands; j++) {
                    g_jobs[i].runOnForeBackground = true;
                    // one of the commands will need to set runOnForeBackground true.
                }
            }
            break;
        case SIGINT:
            if (DEBUG)
                printf("Received SIGINT");
            kill((-1 * g_currentPgid), SIGTSTP);
            break;
        case SIGCONT:
            // NOT SURE IF THIS SHOULD BE HERE, PRETTY SURE I WANT MY SIGCONTS TO GO TO ACTUAL
            // PGRPS
            break;
        case 22:
            if (DEBUG)
                printf("received sigttou");
            sleep(10);
            break;
        default:
            if (DEBUG)
                printf("Signal number = %d, not handled", signum);
            break;
    }
}

void fg() {
    for (int i = 0; i < g_numJobs; i++) {
        if (g_jobs[i].runOnForeBackground)
            kill(-1 * (g_jobs[i].pgid), SIGCONT);
    }
}

void bg() {
    for (int i = 0; i < g_numJobs; i++) {
        if (g_jobs[i].runOnForeBackground)
            kill(-1 * (g_jobs[i].pgid), SIGCONT);
    }
}

void freeJobs(int index) {
    for (int i = 0; i < g_jobs[index].numCommands; i++) {
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
        input[i].hasPipe = false;
        for (int j = 0; input[i].fexec[j] != (char *) NULL; j++) {
            input[i].fexec[j] = (char *) NULL;
        }
    }
    free(g_jobs[index].fullInput);
    g_jobs[index].numCommands = 0;
    g_jobs[index].runOnForeBackground = false;
    g_jobs[index].runInBackground = false;
    g_jobs[index].pgid = 1;
    g_jobs[index].jobNumber = 0;
}

void executeTwoCommands() {
    int pipefd[2];
    int status, pid_ch1, pid_ch2, pid;
    pipe(pipefd);
    pid_ch1 = fork();
    if (pid_ch1 > 0) {
        if (DEBUG)
            printf("Child1 pid = %d\n", pid_ch1);
        g_currentPgid = pid_ch1;
        // Parent
        pid_ch2 = fork();
        if (pid_ch2 > 0) {
            if (DEBUG)
                printf("Child2 pid = %d\n", pid_ch2);
            close(pipefd[0]); //close the pipe in the parent
            close(pipefd[1]);
            int count = 0;
            while (count < 2) {
                if (!g_jobs[g_jobsIndex].runInBackground)
                    pid = waitpid(-1 * pid_ch1, &status, WUNTRACED);
                else
                    pid = waitpid(-1 * pid_ch1, &status, WNOHANG);
                if (status == 3) {
                    if (DEBUG)
                        printf("Invalid command");
                    count++;
                }
                if (WIFEXITED(status)) {
                    if (DEBUG)
                        printf("child %d exited, status=%d\n", pid, WEXITSTATUS(status));
                    count++;
                } else if (WIFCONTINUED(status)) {
                    if (DEBUG)
                        printf("Continuing %d\n", pid);
                    // PLACE HOLDER
                }
                /*else if (WIFSTOPPED(status)) {
                    if (DEBUG)
                        printf("%d stopped by signal %d\n", pid, WSTOPSIG(status));
                    // PLACE HOLDER
                }*/
            }
            if (!g_jobs[g_jobsIndex].runInBackground)
            {
                freeJobs(g_jobsIndex);
                g_numJobs--;
            }
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
                exit(3);
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
            exit(3);
        }
    }
}

void executeOneCommand() {
    g_jobs[g_jobsIndex].numCommands = 1;
    int pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
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
        if (execvp(input[0].command, input[0].fexec) == -1) {
            exit(3);
        }
    } else {
        g_jobs[g_jobsIndex].pgid = pid;
        int status;
        if (!g_jobs[g_jobsIndex].runInBackground) {
            waitpid(pid, &status, WUNTRACED | WCONTINUED);
            freeJobs(g_jobsIndex);
            g_numJobs--;
            if (WIFEXITED(status)) {
                if (DEBUG)
                    printf("child %d exited, status=%d\n", pid, WEXITSTATUS(status));
            } else if (WIFCONTINUED(status)) {
                if (DEBUG)
                    printf("Continuing %d\n", pid);
            } else {
//            waitpid(pid, &status, WNOHANG);
            }
        // wait does not take options:
        //    waitpid(-1,&status,0) is same as wait(&status)
        // with no options waitpid wait only for terminated child processes
        // with options we can specify what other changes in the child's status
        // we can respond to. Here we are saying we want to also know if the child
        // has been stopped (WUNTRACED) or continued (WCONTINUED)

            // PLACE HOLDER
        }/*else if (WIFSTOPPED(status)) {
            if (DEBUG)
                printf("%d stopped by signal %d\n", pid, WSTOPSIG(status));
            // PLACE HOLDER
        }*/
    }
}

void printJobs() {
    int finishedJobs[20];
    int finishedJobsIdx = 0;
    for (int i = 0; i < 20; i++)
        finishedJobs[i] = -1;
    // Check if any jobs are running or
    for (int jobsIdx = 0; jobsIdx < g_numJobs; jobsIdx++) {
        int status;
        int ret = waitpid(-1 * g_jobs[jobsIdx].pgid, &status, WNOHANG | WUNTRACED);
        if (ret == 0 && !WIFSTOPPED(status)) {
            if (g_jobs[jobsIdx].runOnForeBackground)
                printf("[%d]+ Running %s", g_jobs[jobsIdx].jobNumber, g_jobs[jobsIdx].fullInput);
            else
                printf("[%d]- Running %s", g_jobs[jobsIdx].jobNumber, g_jobs[jobsIdx].fullInput);
        } else if (ret == 0 && WIFSTOPPED(status)) {
            if (WSTOPSIG(status) == SIGTSTP) {
                if (g_jobs[jobsIdx].runOnForeBackground)
                    printf("[%d]+ Stopped %s", g_jobs[jobsIdx].jobNumber, g_jobs[jobsIdx].fullInput);
                else
                    printf("[%d]- Stopped %s", g_jobs[jobsIdx].jobNumber, g_jobs[jobsIdx].fullInput);
            }
            if (WSTOPSIG(status) == SIGINT) {
                if (g_jobs[jobsIdx].runOnForeBackground)
                    printf("[%d]+ Killed %s", g_jobs[jobsIdx].jobNumber, g_jobs[jobsIdx].fullInput);
                else
                    printf("[%d]- Killed %s", g_jobs[jobsIdx].jobNumber, g_jobs[jobsIdx].fullInput);
                finishedJobs[finishedJobsIdx] = jobsIdx;
                finishedJobsIdx++;
            }
        }
    }
    for (int i = 0; i < 20; i++) {
        if (finishedJobs[i] != -1)
            freeJobs(i);
    }

}

void updateJobs() {
    int finishedJobs[20];
    for (int i = 0; i < 20; i++)
        finishedJobs[i] = -1;
    int finishedJobsIdx = 0;
    // Check if any job has finished
    for (int jobsIdx = 0; jobsIdx < g_numJobs; jobsIdx++) {
        int status;
        int ret = waitpid(-1 * g_jobs[jobsIdx].pgid, &status, WNOHANG | WUNTRACED);
        if (ret == -1)
            if (DEBUG)
                printf("waitpid error, errno: %d\n", errno);
        // ret will be the pid (as it's finished) and WIFSTOPPED will be true
        // have to use both due to WNOHANG
        if (ret != 0 && WIFEXITED(status) && ret != -1) {
            // most recent command
            if (g_jobs[jobsIdx].runOnForeBackground)
                printf("[%d]+ Done %s", g_jobs[jobsIdx].jobNumber, g_jobs[jobsIdx].fullInput);
            else
                printf("[%d]- Done %s", g_jobs[jobsIdx].jobNumber, g_jobs[jobsIdx].fullInput);
            finishedJobs[finishedJobsIdx] = jobsIdx;
            finishedJobsIdx++;
        }
    }
    for (int i = 0; i < 20; i++) {
        if (finishedJobs[i] != -1)
            freeJobs(i);
    }
    // left shifts all array contents
    for (int i = 0; i < 20; i++) {
        // empty pos
        if (g_jobs[i].pgid == 1) {
            int nearestFullElement;
            for (nearestFullElement = i; nearestFullElement < 19; nearestFullElement++) {
                if (g_jobs[nearestFullElement].pgid != 1)
                    break;
            }
            g_jobs[i] = g_jobs[nearestFullElement];
            g_jobs[nearestFullElement].pgid = 1;
        }
    }
    // finding the last element
    int i;
    for (i = 0; i < 20; i++) {
        if (g_jobs[i].pgid != 1)
            break;
    }
    // resetting jobs number
    if (i == 0) {
        g_jobsNumber = 0;
        g_numJobs = 0;
    }
    else
        g_numJobs = i+1;
    g_jobs[i].runOnForeBackground = true;
    g_jobsIndex = i;
}

int main() {
    char *inString;
    while (1) {
        signal(SIGINT, sigHandler);
        signal(SIGTSTP, sigHandler);
        signal(SIGCONT, sigHandler);
        updateJobs();
        inString = readline("# ");
        if (inString == NULL)
            break;
        if (strlen(inString) > 0) {
            parseString(inString);
            if (strcmp(input[0].command, "bg") == 0)
                bg();
            else if (strcmp(input[0].command, "fg") == 0)
                fg();
            else if (strcmp(input[0].command, "jobs") == 0)
                printJobs();
            else if (input[0].hasPipe) {
                executeTwoCommands();
            } else {
                executeOneCommand();
            }
        }
    }
}