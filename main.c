/******************************************************************************
 *
 *  File Name........: main.c
 *
 *  Description......: Simple driver program for ush's parser
 *
 *  Author...........: Vincent W. Freeh
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "parse.h"


#define NUM_BUILTINS 9
#define err(x) fprintf(stderr, "%s\n", x);

char shell_built_ins[NUM_BUILTINS][20] = {"cd", "pwd", "echo", "setenv", "logout",  "where", "nice", "kill", "unsetenv"};

int check_if_builtin(char *cmd) {
    int i;
    for (i =0; i < NUM_BUILTINS; i++) {
         if (strcmp(cmd, shell_built_ins[i]) == 0) {
             return 1; // True
         }
    }
    return 0; //False
}

void setup_pipes_io(Cmd c) {
    
    int fd = -1;
    if (c) {
        printf("%s%s ", c->exec == Tamp ? "BG " : "", c->args[0]);
    
        if (c->in == Tin ) {
            printf("<(%s) ", c->infile);
            fd = open(c->infile, O_RDONLY, 0660);
            if (fd == -1) 
                error("failed to open input file <%s> to %s", 
                	  c->infile, c->args[0]);
            dup2(fd, 0);
            // Not so sure about this
            close(fd);
        }

        if (c->out != Tnil) {
            switch (c->out) {
               case Tout: {
               	  fd = open(c->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0660);
	              if (fd == -1) error("failed to open output file <%s> to %s",
	                	               c->outfile, c->args[0]);
	              dup2(fd, 1);
	              // Not so sure
	              close(fd);
	              printf(">(%s) ", c->outfile);
	           }       
	           break;
             
               case Tapp: {
               	    fd = open(c->outfile, O_WRONLY | O_APPEND | O_CREAT, 0660);
	                if (fd == -1) error("failed to open output file <%s> to %s",
	                	               c->outfile, c->args[0]);
	                dup2(fd, 1);
	                // Not so sure
	                close(fd);
	                printf(">>(%s) ", c->outfile);
	           }
	           break;
      
               case ToutErr: {
               	    fd = open(c->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0660);
	                if (fd == -1) error("failed to open output file <%s> to %s",
	                	               c->outfile, c->args[0]);
	                dup2(fd, 1);
	                dup2(fd, 2);
	                // Not so sure
	                close(fd);
	                printf(">&(%s) ", c->outfile);
       	       }     
	           break;
               
               case TappErr: {
               	    fd = open(c->outfile, O_WRONLY | O_APPEND | O_CREAT, 0660);
	                if (fd == -1) error("failed to open output file <%s> to %s",
	                	               c->outfile, c->args[0]);
	                dup2(fd, 1);
	                dup2(fd, 2);
	                // Not so sure
	                close(fd);
	                printf(">>&(%s) ", c->outfile);
	           }
	           break;
               case Tpipe: {
	                printf("| ");
	           }
	           break;
               case TpipeErr: {
	                printf("|& ");
               }
	           break;
               default:
	                fprintf(stderr, "Shouldn't get here\n");
	                exit(-1);
            }   
        }
    }
}

static void prCmd(Cmd c)
{
    /*
    int i;
    
    if (c->nargs > 1 ) {
        printf("[");
        for ( i = 1; c->args[i] != NULL; i++ )
	         printf("%d:%s,", i, c->args[i]);
             printf("\b]");
    }
    putchar('\n');
    */

    // this driver understands one command
    if (strcmp(c->args[0], "end") == 0) {
        exit(0);
    }

    if (c->next == NULL && check_if_builtin(c->args[0])) {
       // Last command in pipe is a builtin. Execute directly.
    } else {
        pid_t pid;
        pid = fork();
        if (pid < 0) {
            err("fork: failed");
        } else if (pid == 0) {
            /*child */
            setup_pipes_io(c);
            execv(c->args[0], c->args);
            exit(0);
        } else {
            /*parent */
            wait(NULL);
        }
    } 
}


static void prPipe(Pipe p)
{
  int i = 0;
  Cmd c;

  if ( p == NULL )
    return;

  printf("Begin pipe%s\n", p->type == Pout ? "" : " Error");
  for ( c = p->head; c != NULL; c = c->next ) {
    printf("  Cmd #%d: ", ++i);
    prCmd(c);
  }
  printf("End pipe\n");
  prPipe(p->next);
}

int main(int argc, char *argv[])
{
  Pipe p;
  char host[50];

  while ( 1 ) {
    if (gethostname(host,50) != 0) {
        strncpy(host,"armadillo",50);
    }
    printf("%s%% ", host);
    p = parse();
    prPipe(p);
    freePipe(p);
  }
}

/*........................ end of main.c ....................................*/
