#include <stdlib.h>
#include <stdio.h>

char *cmd1[] = { "/bin/ls", "-al", "/", 0 };
char *cmd2[] = { "/usr/bin/tr", "a-z", "A-Z", 0 };

void runsource(int pfd[]);
void rundest(int pfd[]);

int
main(int argc, char **argv)
{
	int pid, status;
	int fd[2];

	pipe(fd);

	runsource(fd);
	rundest(fd);
	close(fd[0]); close(fd[1]); 	/* this is important! close both file descriptors on the pipe */

	while ((pid = wait(&status)) != -1)	/* pick up all the dead children */
		fprintf(stderr, "process %d exits with %d\n", pid, WEXITSTATUS(status));
	exit(0);
}


void
runsource(int pfd[])	/* run the first part of the pipeline, cmd1 */
{
	int pid;	/* we don't use the process ID here, but you may wnat to print it for debugging */

	switch (pid = fork()) {

	case 0: /* child */
		dup2(pfd[1], 1);	/* this end of the pipe becomes the standard output */
		close(pfd[0]); 		/* this process don't need the other end */
		execvp(cmd1[0], cmd1);	/* run the command */
		perror(cmd1[0]);	/* it failed! */

	default: /* parent does nothing */
		break;

	case -1:
		perror("fork");
		exit(1);
	}
}

void
rundest(int pfd[])	/* run the second part of the pipeline, cmd2 */
{
	int pid;

	switch (pid = fork()) {

	case 0: /* child */
		dup2(pfd[0], 0);	/* this end of the pipe becomes the standard input */
		close(pfd[1]);		/* this process doesn't need the other end */
		execvp(cmd2[0], cmd2);	/* run the command */
		perror(cmd2[0]);	/* it failed! */

	default: /* parent does nothing */
		break;

	case -1:
		perror("fork");
		exit(1);
	}
}

