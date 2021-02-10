#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>

#define DEBUG false
// max num g_jobs <= 20
// & always at the end
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
    bool tstopped;
    bool interrupted;
    bool completed;
} Job_t;
Job_t g_jobs[20] = {0};
int g_jobsNumber = 0;

// Points at the last FULL command
int g_jobsCurrent = 0;

// Points at the empty command directly after g_jobsCurrent
int g_jobsNext = 0;
pid_t g_currentPgid = 0;


void parseString(char *str); //parses an command into strings

void sigHandler(int signum);

void executeTwoCommands();

void executeOneCommand();

void redirectOutput(int inputIndex);

void redirectInput(int inputIndex);

void redirectError(int inputIndex);

void updateJobs();

void leftShiftJobs();

void printJobs();

void freeJobs(const int* array);

void bg();

void fg();

void initJobs();

void parseString(char *str) {
    // ---------------------- Command_t creation & population
    Command_t input[2] = {0};
    char *tokenizedCommand[2000];
    // Need to initialize my struct
    input[0].numArgs = 0;
    g_jobs[g_jobsNext].numCommands = 1;
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
            g_jobs[g_jobsNext].numCommands++;
            argvIndex = 0;
            input[cmdIndex].numArgs = 0;
            recognizeCommand = true;
        } else if (strcmp(token, "&") == 0) {
            g_jobs[g_jobsNext].runInBackground = true;
        } else {
            // We'll be grabbing the redirection file names in the for loop, so these
            // should just be argv given to the command
            // Pretty sure this can just be an else as opposed to an else if with
            /* if (token[0] == '-' || isalnum(token[0])) { */
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
    for (int i = 0; i < g_jobs[g_jobsNext].numCommands; i++) {
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

    // ---------------------- Job_t creation & population
    // only viable for the first run -
    if (g_jobsNext == 0)
        g_jobs[g_jobsNext].runOnForeBackground = true;
    g_jobs[g_jobsNext].jobNumber = ++g_jobsNumber;
    for (int i = 0; i < g_jobs[g_jobsNext].numCommands; i++) {
        g_jobs[g_jobsNext].commands[i] = input[i];
    }
    g_jobs[g_jobsNext].fullInput = strdup(str);
    free(str);

    g_jobsCurrent = g_jobsNext;
    g_jobsNext++;
}

/**
 * For dealing with ">"
 * @param inputIndex the command to assign the redirection to
 */
void redirectOutput(int inputIndex) {
    if (DEBUG)
        printf("%s:%d Redirecting output file at %s, using commands[%d] - command: %s\n",
               __FUNCTION__, __LINE__, g_jobs[g_jobsCurrent].commands[inputIndex].outRedirectFileName,
               inputIndex, g_jobs[g_jobsCurrent].commands[inputIndex].command);
    int fd = creat(g_jobs[g_jobsCurrent].commands[inputIndex].outRedirectFileName, 0644);
    dup2(fd, STDOUT_FILENO);
}

/**
 * For dealing with "<"
 * @param inputIndex the command to assign the redirection to
 */
void redirectInput(int inputIndex) {

    int fd = open(g_jobs[g_jobsCurrent].commands[inputIndex].inRedirectFileName, O_RDONLY);
    if (DEBUG)
        printf("%s:%d Redirecting input to file at %s, using commands[%d] - command: %s fd = %d, errno: %d\n",
               __FUNCTION__, __LINE__, g_jobs[g_jobsCurrent].commands[inputIndex].inRedirectFileName,
               inputIndex, g_jobs[g_jobsCurrent].commands[inputIndex].command, fd, errno);
    dup2(fd, STDIN_FILENO);
}
/**
 * For dealing with "2>"
 * @param inputIndex the command to assign the redirection to
 */
void redirectError(int inputIndex) {
    if (DEBUG)
        printf("%s:%d Redirecting error to file at %s, using commands[%d] - command: %s\n",
               __FUNCTION__, __LINE__, g_jobs[g_jobsCurrent].commands[inputIndex].errorRedirectFileName,
               inputIndex, g_jobs[g_jobsCurrent].commands[inputIndex].command);
    int fd = creat(g_jobs[g_jobsCurrent].commands[inputIndex].errorRedirectFileName, 0644);
    dup2(fd, STDERR_FILENO);
}

void sigHandler(int signum) {
    switch (signum) {
        case SIGTSTP:
            if (DEBUG)
                printf("%s:%d - Received stop signal", __FUNCTION__, __LINE__);
            if (g_currentPgid != 0 && g_jobs[g_jobsCurrent].runInBackground == false) {
                if (DEBUG)
                    printf(" Sending it to g_currentPgid=%d\n", g_currentPgid);
                kill((-1 * g_currentPgid), SIGTSTP);
                for (int i = 0; i < 20; i++) {
                    g_jobs[i].runOnForeBackground = false;
                }
                g_jobs[g_jobsCurrent].runOnForeBackground = true;
                g_jobs[g_jobsCurrent].tstopped = true;
            }
            break;
        case SIGINT:
            // Job is freed and left shifted on calling of jobs - see printJobs()
            if (DEBUG)
                printf("%s:%d - Received sig int", __FUNCTION__, __LINE__);
            if (g_currentPgid != 0 && g_jobs[g_jobsCurrent].runInBackground == false) {
                g_jobs[g_jobsCurrent].interrupted = true;
                kill((-1*g_currentPgid), SIGINT);
                if (DEBUG)
                    printf(" Sending it to g_currentPgid=%d\n", g_currentPgid);
            }
            break;
        default:
            if (DEBUG)
                printf("Signal number = %d, not handled\n", signum);
            break;
    }
}

void fg() {
    int status;
    int jobsIdx;
    for (jobsIdx = 0; jobsIdx < 20; jobsIdx++) {
        if (g_jobs[jobsIdx].runOnForeBackground) {
            g_jobsCurrent = jobsIdx;
            g_currentPgid = g_jobs[jobsIdx].pgid;
            g_jobs[jobsIdx].tstopped = false;
            g_jobs[jobsIdx].runInBackground = false;
            // removing ampersand if included
            if (g_jobs[jobsIdx].fullInput[strlen(g_jobs[jobsIdx].fullInput)-1] == '&')
                g_jobs[jobsIdx].fullInput[strlen(g_jobs[jobsIdx].fullInput)-1] = '\0';
            printf("%s\n", g_jobs[jobsIdx].fullInput);
            kill(-1 * (g_jobs[jobsIdx].pgid), SIGCONT);
            break;
        }
    }
    waitpid(g_jobs[g_jobsCurrent].pgid, &status, WUNTRACED);
    if (WIFEXITED(status)) {
        g_jobs[g_jobsCurrent].completed = true;
        if (DEBUG)
            printf("%s:%d - child %d exited, status=%d\n", __FUNCTION__, __LINE__, g_jobs[g_jobsCurrent].pgid,
                   WEXITSTATUS(status));
    } else if (WIFSTOPPED(status)) {
        if (WSTOPSIG(status) == SIGTSTP)
            g_jobs[g_jobsCurrent].tstopped = true;
        else if (WSTOPSIG(status) == SIGINT)
            g_jobs[g_jobsCurrent].interrupted = true;
        if (DEBUG)
            printf("%s:%d - %d tstopped by signal %d\n", __FUNCTION__, __LINE__, g_jobs[g_jobsCurrent].pgid,
                   WSTOPSIG(status));

    }
}

void bg() {
    int status;
    int jobsIdx;
    for (jobsIdx = 0; jobsIdx < 20; jobsIdx++) {
        if (g_jobs[jobsIdx].runOnForeBackground) {
            g_jobsCurrent = jobsIdx;
            g_jobs[jobsIdx].tstopped = false;
            g_currentPgid = g_jobs[jobsIdx].pgid;
            g_jobs[jobsIdx].runInBackground = true;
            // appending ampersand
            strcat(g_jobs[jobsIdx].fullInput, " &");
            printf("[%d]+ %s\n", g_jobs[jobsIdx].jobNumber, g_jobs[jobsIdx].fullInput);
            kill(-1 * (g_jobs[jobsIdx].pgid), SIGCONT);
            break;
        }
    }
    waitpid(g_jobs[g_jobsCurrent].pgid, &status, WNOHANG);
}


void executeTwoCommands() {
    int pipefd[2];
    int status, pid_ch1, pid_ch2, pid;
    pipe(pipefd);
    pid_ch1 = fork();
    if (pid_ch1 > 0) {
        if (DEBUG)
            printf("%s:%d - Child1 pid = %d\n", __FUNCTION__, __LINE__, pid_ch1);
        g_currentPgid = pid_ch1;
        g_jobs[g_jobsCurrent].pgid = pid_ch1;
        // Parent
        pid_ch2 = fork();
        if (pid_ch2 > 0) {
            if (DEBUG)
                printf("%s:%d - Child2 pid = %d\n",  __FUNCTION__, __LINE__,pid_ch2);
            close(pipefd[0]); //close the pipe in the parent
            close(pipefd[1]);
            int count = 0;
            while (count < 2) {
                if (!g_jobs[g_jobsCurrent].runInBackground)
                    pid = waitpid(-1 * pid_ch1, &status, WUNTRACED);
                else
                    pid = waitpid(-1 * pid_ch1, &status, WNOHANG);
                // status == 3 because I'm exiting with 3
                if (status == 3) {
                    if (DEBUG)
                        printf("%s:%d - Invalid command", __FUNCTION__, __LINE__);
                    count++;
                }
                if (WIFEXITED(status)) {
                    g_jobs[g_jobsCurrent].completed = true;
                    if (DEBUG)
                        printf("%s:%d - child %d exited, status=%d\n", __FUNCTION__, __LINE__,pid, WEXITSTATUS(status));
                    count++;
                }
                else if (WIFSTOPPED(status)) {
                    if (WSTOPSIG(status) == SIGTSTP)
                        g_jobs[g_jobsCurrent].tstopped = true;
                    else if (WSTOPSIG(status) == SIGINT)
                        g_jobs[g_jobsCurrent].interrupted = true;
                    if (DEBUG)
                        printf("%d tstopped by signal %d\n", pid, WSTOPSIG(status));
                }
            }
        } else {
            //Child 2
            setpgid(0, pid_ch1); //child2 joins the group whose group id is same as child1's pid
            close(pipefd[1]); // close the write end
            dup2(pipefd[0], STDIN_FILENO);
            if (g_jobs[g_jobsCurrent].commands[1].hasInRedirect) {
                redirectInput(1);
            }
            if (g_jobs[g_jobsCurrent].commands[1].hasOutRedirect) {
                redirectOutput(1);
            }
            if (g_jobs[g_jobsCurrent].commands[1].hasErrorRedirect) {
                redirectError(1);
            }
            if (execvp(g_jobs[g_jobsCurrent].commands[1].command, g_jobs[g_jobsCurrent].commands[1].fexec) == -1) {
                exit(3);
            }
        }
    } else {
        // Child 1
        setpgid(0, 0);
        //   group id is same as his pid: pid_ch1
        close(pipefd[0]); // close the read end
        dup2(pipefd[1], STDOUT_FILENO);
        if (g_jobs[g_jobsCurrent].commands[0].hasInRedirect) {
            redirectInput(0);
        }
        if (g_jobs[g_jobsCurrent].commands[0].hasOutRedirect) {
            redirectOutput(0);
        }
        if (g_jobs[g_jobsCurrent].commands[0].hasErrorRedirect) {
            redirectError(0);
        }
        if (execvp(g_jobs[g_jobsCurrent].commands[0].command, g_jobs[g_jobsCurrent].commands[0].fexec) == -1) {
            exit(3);
        }
    }
}

void executeOneCommand() {
    int pgid = fork();
    if (pgid == 0) {
        setpgid(0, 0);
        if (g_jobs[g_jobsCurrent].commands[0].hasInRedirect) {
            redirectInput(0);
        }
        if (g_jobs[g_jobsCurrent].commands[0].hasOutRedirect) {
            redirectOutput(0);
        }
        if (g_jobs[g_jobsCurrent].commands[0].hasErrorRedirect) {
            redirectError(0);
        }
        // exit if there's no command found
        if (execvp(g_jobs[g_jobsCurrent].commands[0].command, g_jobs[g_jobsCurrent].commands[0].fexec) == -1) {
            exit(3);
        }
    } else {
        g_currentPgid = pgid;
        g_jobs[g_jobsCurrent].pgid = pgid;
        int status;
        if (!g_jobs[g_jobsCurrent].runInBackground) {
            waitpid(pgid, &status, WUNTRACED);
            if (WIFEXITED(status)) {
                g_jobs[g_jobsCurrent].completed = true;
                if (DEBUG)
                    printf("%s:%d - child %d exited, status=%d\n", __FUNCTION__, __LINE__, pgid,
                           WEXITSTATUS(status));

            } else if (WIFSTOPPED(status)) {
                    if (WSTOPSIG(status) == SIGTSTP)
                        g_jobs[g_jobsCurrent].tstopped = true;
                    else if (WSTOPSIG(status) == SIGINT)
                        g_jobs[g_jobsCurrent].interrupted = true;
                    if (DEBUG)
                        printf("%s:%d - %d tstopped by signal %d\n", __FUNCTION__, __LINE__, pgid, WSTOPSIG(status));

            }
        } else {
            waitpid(pgid, &status, WNOHANG);
        }
    }
}

/**
 * Sets pgid of all jobs to 1 to ensure signal handlers, fg, bg (anyone who uses kill())
 *  From man 2 kill: If pid equals 0, then sig is sent to every process in the process group
 *  of the calling process.
 */
void initJobs() {
    for (int i = 0; i < 20; i++) {
        g_jobs[i].pgid = 1;
    }
}

void freeJobs(const int* arr) {
    for (int arrInd = 0; arrInd < 20; arrInd++) {
        if (arr[arrInd] != -1) {
            for (int i = 0; i < g_jobs[arr[arrInd]].numCommands; i++) {
                free(g_jobs[arr[arrInd]].commands[i].command);
                if (g_jobs[arr[arrInd]].commands[i].numArgs > 0) {
                    for (int j = 0; j < g_jobs[arr[arrInd]].commands[i].numArgs; j++) {
                        free(g_jobs[arr[arrInd]].commands[i].argv[j]);
                    }
                }
                if (g_jobs[arr[arrInd]].commands[i].hasInRedirect) {
                    free(g_jobs[arr[arrInd]].commands[i].inRedirectFileName);
                    g_jobs[arr[arrInd]].commands[i].hasInRedirect = false;
                }
                if (g_jobs[arr[arrInd]].commands[i].hasOutRedirect) {
                    free(g_jobs[arr[arrInd]].commands[i].outRedirectFileName);
                    g_jobs[arr[arrInd]].commands[i].hasOutRedirect = false;
                }
                if (g_jobs[arr[arrInd]].commands[i].hasErrorRedirect) {
                    free(g_jobs[arr[arrInd]].commands[i].errorRedirectFileName);
                    g_jobs[arr[arrInd]].commands[i].hasErrorRedirect = false;
                }
                g_jobs[arr[arrInd]].commands[i].hasPipe = false;
                for (int j = 0; g_jobs[arr[arrInd]].commands[i].fexec[j] != (char *) NULL; j++) {
                    g_jobs[arr[arrInd]].commands[i].fexec[j] = (char *) NULL;
                }
            }
            free(g_jobs[arr[arr[arrInd]]].fullInput);
            g_jobs[arr[arrInd]].numCommands = 0;
            g_jobs[arr[arrInd]].runOnForeBackground = false;
            g_jobs[arr[arrInd]].runInBackground = false;
            g_jobs[arr[arrInd]].pgid = 1;
            g_jobs[arr[arrInd]].jobNumber = 0;
            g_jobs[arr[arrInd]].interrupted = false;
            g_jobs[arr[arrInd]].tstopped = false;
            g_jobs[arr[arrInd]].completed = false;
        }
    }
    leftShiftJobs();
}

/**
 * ONLY NECESSARY AFTER A JOB HAS BEEN FREED
 * Sets all g_jobs variables and ensures that no empty job is in the jobs array
 * Specifically, sets:
 * g_jobsNumber - NOT SET UNLESS THERE ARE NO MORE JOBS
 * g_jobsCurrent
 * g_jobsNext
 */
void leftShiftJobs() {
    if (DEBUG)
        printf("%s:%d Left shifted jobs!\n", __FUNCTION__, __LINE__);
    // left shifts all array contents
    for (int i = 0; i < 20; i++) {
        // empty pos when pgid == 1
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
    if (g_jobs[0].pgid == 1)
    {
        if (DEBUG)
            printf("%s:%d g_jobsNext, g_jobsCurrent, g_jobsNumber, g_numJobs all == 0\n",
                   __FUNCTION__ , __LINE__);
        g_jobsNext = 0;
        g_jobsCurrent = 0;
        g_jobsNumber = 0;
    }
    else {
        int jobsIdx;
        for (jobsIdx = 1; jobsIdx < 20; jobsIdx++) {
            if (g_jobs[jobsIdx].pgid == 1)
                break;
        }
        // this is ugly, but i'll just get the maxJobNumber jobs number, the add 1 will happen in the parse.
        int maxJobNumber = -1;
        for (int i = 0; i < 20; i++)
        {
            if (g_jobs[i].jobNumber > maxJobNumber)
                maxJobNumber = g_jobs[i].jobNumber;
        }
        g_jobsNumber = maxJobNumber;
        g_jobsNext = jobsIdx;
        g_jobsCurrent = jobsIdx - 1;
        if (DEBUG)
            printf("%s:%d g_jobsNext=%d, g_jobsCurrent=%d, g_jobsNumber=%d\n",
                   __FUNCTION__ , __LINE__, g_jobsNext, g_jobsCurrent, g_jobsNumber);
    }
    if (DEBUG)
    {
        printf("%s:%d - Jobs array:\n", __FUNCTION__, __LINE__);
        for (int i = 0; i < 5; i++)
        {
            printf("jobs[%d] = %s\n", i, g_jobs[i].fullInput);
        }
    }
}

/**
 * Ran on jobs command
 */
void printJobs() {
    int interruptedJobs[20];
    int interruptedJobsIdx = 0;
    for (int i = 0; i < 20; i++)
        interruptedJobs[i] = -1;
    for (int jobsIdx = 0; jobsIdx < 20; jobsIdx++) {
        if (g_jobs[jobsIdx].pgid != 1) {
            if (g_jobs[jobsIdx].tstopped == true) {
                if (g_jobs[jobsIdx].runOnForeBackground)
                    printf("[%d]+ Stopped %s\n", g_jobs[jobsIdx].jobNumber, g_jobs[jobsIdx].fullInput);
                else
                    printf("[%d]- Stopped %s\n", g_jobs[jobsIdx].jobNumber, g_jobs[jobsIdx].fullInput);
            } else if (g_jobs[jobsIdx].interrupted == true) {
                interruptedJobs[interruptedJobsIdx] = jobsIdx;
                interruptedJobsIdx++;
            } else if (g_jobs[jobsIdx].completed != true) {
                // fix this
                if (g_jobs[jobsIdx].runOnForeBackground)
                    printf("[%d]+ Running %s\n", g_jobs[jobsIdx].jobNumber, g_jobs[jobsIdx].fullInput);
                else
                    printf("[%d]- Running %s\n", g_jobs[jobsIdx].jobNumber, g_jobs[jobsIdx].fullInput);
            }
        }
    }
    freeJobs(interruptedJobs);
}

/*
 * Should specifically update jobs which are occurring in the background.
 *
 */
void updateJobs() {
    int finishedJobs[20];
    for (int i = 0; i < 20; i++)
        finishedJobs[i] = -1;
    int finishedJobsIdx = 0;
    // Check if any job that's running in the background finished
    for (int jobsIdx = 0; jobsIdx < 20; jobsIdx++) {
        if (g_jobs[jobsIdx].completed)
        {
            finishedJobs[finishedJobsIdx] = jobsIdx;
            finishedJobsIdx++;
        }
        if (g_jobs[jobsIdx].runInBackground) {
            int status;
            int ret = waitpid(-1 * g_jobs[jobsIdx].pgid, &status, WNOHANG | WUNTRACED);
            if (DEBUG)
                printf("%s:%d Called waitpid on pg: -1*%d, return value: %d\n", __FUNCTION__, __LINE__,
                       g_jobs[jobsIdx].pgid, ret);
            if (ret == -1)
                if (DEBUG)
                    printf("%s:%d - waitpid error, errno: %d\n", __FUNCTION__, __LINE__, errno);
            // ret will be the pid (as it's finished) and WIFSTOPPED will be true
            // have to use both due to WNOHANG
            if (ret != 0 && WIFEXITED(status) && ret != -1) {
                // most recent command
                if (g_jobs[jobsIdx].runOnForeBackground)
                    printf("[%d]+ Done %s\n", g_jobs[jobsIdx].jobNumber, g_jobs[jobsIdx].fullInput);
                else
                    printf("[%d]- Done %s\n", g_jobs[jobsIdx].jobNumber, g_jobs[jobsIdx].fullInput);
                g_jobs[jobsIdx].completed = true;
                finishedJobs[finishedJobsIdx] = jobsIdx;
                finishedJobsIdx++;
            }
        }
    }
    // free jobs that ran in the background
    freeJobs(finishedJobs);
    
    // assign runOnForeBackground here
    // note that g_jobsCurrent should be correctly set by this point (either by freeJobs or was already correct)
    for (int i = 0; i < 20; i++) {
        g_jobs[i].runOnForeBackground = false;
    }
    g_jobs[g_jobsCurrent].runOnForeBackground = true;


}

int main() {
    initJobs();
    char *inString;
    while (1) {
        signal(SIGINT, sigHandler);
        signal(SIGTSTP, sigHandler);
        inString = readline("# ");
        if (inString == NULL)
            break;
        if (strlen(inString) > 0) {
            if (DEBUG)
                printf("Main loop begin: g_jobsCurrent = %d, g_jobsNext = %d, g_jobsNumber,  = %d\n",
                       g_jobsCurrent, g_jobsNext, g_jobsNumber);
            if (strcmp(inString, "bg") == 0)
                bg();
            else if (strcmp(inString, "fg") == 0)
                fg();
            else if (strcmp(inString, "jobs") == 0)
            {
                updateJobs();
                printJobs();
            }
            else {
                parseString(inString);
                if (DEBUG)
                    printf("Main loop after parse: g_jobsCurrent = %d, g_jobsNext = %d, g_jobsNumber = %d\n",
                           g_jobsCurrent, g_jobsNext, g_jobsNumber);
                if (g_jobs[g_jobsCurrent].commands[0].hasPipe) {
                    executeTwoCommands();
                } else {
                    executeOneCommand();
                }
            }
            // needs to be towards the end - if we ran # ls, with
            // a completed background job, then we want [1]+ Done & the listing (order of both doesn't
            // matter
            updateJobs();
        }
    }
    for (int i = 0; i < 20; i++)
    {
        if (g_jobs[i].pgid != 1)
        {
            kill(g_jobs[i].pgid, SIGKILL);
        }
    }
}