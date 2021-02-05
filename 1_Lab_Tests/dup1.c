#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

/* dup Example 1 */
/* runs as: dup1 <command_with_no_args> <output_file>*/
int main(int argc, char **argv) 
{
    pid_t cpid;
    int ofd;
    char *cmd, *output;

    cmd = argv[1]; output = argv[2];
    cpid = fork(); 
    if (cpid == 0) {  /* Child */
	printf("Running like : %s > %s\n",cmd, output);
	ofd = creat(output, 0644);
	dup2(ofd,1);   // same as: close(1); dup(ofd)
	execlp(cmd, cmd, (char *)NULL);
    }
    wait((int *)NULL);
}

