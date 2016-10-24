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
#include <unistd.h>
#include <fcntl.h>
#include "parse.h"
#include <signal.h>
#include <errno.h>


#define NUM_BUILTINS 10
#define err(x) fprintf(stderr, "%s\n", x);

char shell_built_ins[NUM_BUILTINS][20] = {"cd", "pwd", "echo", "setenv", "logout", "end",  "where", "nice", "kill", "unsetenv"};

int check_if_builtin(char *cmd) {
    int i;
    for (i =0; i < NUM_BUILTINS; i++) {
         if (strcmp(cmd, shell_built_ins[i]) == 0) {
             return 1; // True
         }
    }
    return 0; //False
}

char * cmd_path(char * cname) {
    
    struct stat sti;
    char env_path[1000];
    // This is an out parameter needs to be on heap
	char * cmd_str = malloc(2000);
	char * c = getenv("PATH");
	char * token;

	if (cname == NULL) return NULL;

	if (cname[0] == '/') {
		if (stat(cname, &sti) == 0) {
			strncpy(cmd_str, cname, 2000);
			return cmd_str;
		} else {
			free(cmd_str);
			return NULL;
		}
    }
	strncpy(env_path, c, 1000);
    
    token = strtok(env_path, ":");
    while (token) {
    	strncpy(cmd_str, token,  2000);
    	strncat(cmd_str, "/",   2000 - strlen(cmd_str));
    	strncat(cmd_str, cname, 2000 - strlen(cmd_str));
    	if (stat(cmd_str, &sti) == 0) {
    		return cmd_str;
    	}
    	token = strtok(NULL, ":");
    }
    free(cmd_str);
    return NULL;
}
void setup_pipes_io(Cmd c, int *lp, int *rp) {
    
    int fd = -1;
    if (c) {
        printf("%s%s ", c->exec == Tamp ? "BG " : "", c->args[0]);
    
        if (c->in != Tnil) {
        	
        	switch (c->in) {
            
                case Tin: {
                     printf("<(%s) ", c->infile);
                     fd = open(c->infile, O_RDONLY, 0660);
                     if (fd == -1) {
                         fprintf(stderr, "%s: Permission denied\n", c->infile);
                         exit(1);
                     } else {
                     	dup2(fd, 0);
                        // Not so sure about this
                        close(fd);
                     }
                }
                break;
            
                case Tpipe: 
                case TpipeErr: {

                	 if (lp == NULL) {err("%s: Something went wrong in pipes");}
                	 printf("cmd %s\n", c->args[0]);
                	 fflush(stdout);
                	 printf("Desc1 is %d Desc2 is %d\n",lp[0], lp[1]);
                	 dup2(lp[0], 0);
                	 close(lp[1]);
                }
                break;
                default: err("should not come here");
            }
        }

        if (c->out != Tnil) {
            switch (c->out) {
               case Tout: {
               	  fd = open(c->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0660);
	              if (fd == -1) { 
	              	  fprintf(stderr, "%s: Permission denied\n", c->outfile);
	              	  exit(1);
	              }
	              dup2(fd, 1);
	              // Not so sure
	              close(fd);
	              printf(">(%s) ", c->outfile);
	           }       
	           break;
             
               case Tapp: {
               	    fd = open(c->outfile, O_WRONLY | O_APPEND | O_CREAT, 0660);
	                if (fd == -1) { 
	                	fprintf(stderr, "%s: Permission denied\n", c->outfile);
	                    exit(1);
	                }
	                dup2(fd, 1);
	                // Not so sure
	                close(fd);
	                printf(">>(%s) ", c->outfile);
	           }
	           break;
      
               case ToutErr: {
               	    fd = open(c->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0660);
	                if (fd == -1)  { 
	                	fprintf(stderr, "%s: Permission denied\n", c->outfile);
	                	exit(1);
	                }
	                dup2(fd, 1);
	                dup2(fd, 2);
	                // Not so sure
	                close(fd);
	                printf(">&(%s) ", c->outfile);
       	       }     
	           break;
               
               case TappErr: {
               	    fd = open(c->outfile, O_WRONLY | O_APPEND | O_CREAT, 0660);
	                if (fd == -1) {
	                	fprintf(stderr, "%s: Permission denied\n", c->outfile);
	                	exit(1);
	                }
	                dup2(fd, 1);
	                dup2(fd, 2);
	                // Not so sure
	                close(fd);
	                printf(">>&(%s) ", c->outfile);
	           }
	           break;
               case Tpipe: {
            
               	    if (rp == NULL) {
               	    	err("Something went wrong in output pipes");
               	    }
               	    printf("| ");
               	    printf("Desc1 is %d Desc2 is %d\n",rp[0], rp[1]);
               	    dup2(rp[1], 1);
               	    close(rp[0]);
	           }
	           break;
               case TpipeErr: {
               	    if (rp == NULL) {
               	    	err("Something went wrong in output pipes");
               	    }
               	    dup2(rp[1], 2);
               	    dup2(rp[1], 1);
               	    close(rp[0]);
	                printf("|& ");
               }
	           break;
               default:
	                err("Shouldn't get here");
	                exit(-1);
            }   
        }
    }
}

void run_builtin(Cmd c) {
	
	if ((strcmp(c->args[0], "end") == 0) || 
    	       (strcmp(c->args[0], "logout") == 0)) {
        exit(0);
    }

    if (strcmp(c->args[0], "pwd") == 0 ) {
    	char pwd[2000];
    	char *c = getcwd(pwd, 2000);
    	if (c) {printf("%s\n", pwd);}
    	else {err("unable to get pwd");}
    	return;
    }

    if (strcmp(c->args[0], "cd") == 0) {
    	if (c->args[1] == NULL) {
    		char * home_dir = getenv("HOME");
    		if (home_dir == NULL)           {err("cd: failed");}
    		else if (chdir(home_dir) == -1) {err("cd: failed");}
    	} else {
    		if (chdir(c->args[1]) == -1)    {err("cd: failed");}
    	}
    	return;
    }

    if (strcmp(c->args[0], "echo") == 0) {
    	if (c->nargs > 1 ) {
            int i;
            for (i = 1; c->args[i]!= NULL; i++)
            	printf("%s\n",c->args[i]);
        }
        return;
    }

}

static void prCmd(Cmd c, int * left, int * right)
{
    pid_t pid;
    if (check_if_builtin(c->args[0])) {
       if (c->next == NULL) {
       	 // Last command in pipe is a builtin. Execute directly (csh). 
           run_builtin(c);
       } else {
       	 // builtin has to be run in pipe
           pid = fork();
           if (pid < 0) {
           	   err("Fork Failed");
           } else if (pid == 0) {
           	   setup_pipes_io(c, left, right);
           	   run_builtin(c);
           	   exit(0);
           }
       }
    } else {
        //char * path = cmd_path(c->args[0]);
        pid = fork();
        if (pid < 0) {
            err("fork: failed");
        } else if (pid == 0) {
            /*child */
            setup_pipes_io(c, left, right);
            execvp(c->args[0], c->args);
            if (errno == ENOENT) {
                fprintf(stderr, "%s: command not found\n", c->args[0]);
            } else {    
                fprintf(stderr, "%s: permission denied\n", c->args[0]);
            }
            exit(1);
        }
        // if (path) {free(path);};
    } 
}


static void prPipe(Pipe p)
{
  int i;
  Cmd c;
  Cmd temp;
  int * fd;

  if ( p == NULL )
    return;
  
  int pipe_count = -1;
  
  for (temp = p->head; temp != NULL; temp = temp->next) {
  	pipe_count = pipe_count + 1;
  }
  
  fd = calloc(pipe_count*2, sizeof(int));
  i = 0;
  for (i = 0; i <= (pipe_count - 1)*2; i= i + 2) {
  	   pipe(fd + i);
  }
  printf("Begin pipe%s\n", p->type == Pout ? "" : " Error");
  i = 0;
  for ( c = p->head; c != NULL; c = c->next ) {
    int * left =  NULL;
    int * right = NULL;
    if (i != 0) {
        left = fd + 2*(i -1);
        printf("LEFT %d,%d\n", 2*(i-1), *left);
    } 
    if (i != pipe_count) {
        right = fd + 2*i;
        printf(" RIGHT %d,%d\n", 2*i, *right);
    }
    prCmd(c, left, right);
    
    if (i > 0) {
    	close(fd[2*i - 2]);
    	close(fd[2*i - 1]);
    }
    
    printf("Cmd #%d:%s \n", ++i, c->args[0]);
  }
  /* Not Working
  for (i = 0; i <pipe_count*2; i++) {
  	   printf("%d\n", fd[i]);
  	   close(fd[i]);
  }*/
  int pid,status;
  
  while ((pid = wait(&status)) != -1)	/* pick up all the dead children */
		fprintf(stderr, "process %d exits with %d\n", pid, WEXITSTATUS(status));
  
  free(fd);
  prPipe(p->next);
}

/* 
 *Below two functions are inspired from 
 *http://www.thegeekstuff.com/2012/03/catch-signals-sample-c-code
 */
void sig_handler(int signalno)
{
    if (signalno == SIGTERM)
        printf("received SIGTERM\n");
}

void setup_signals()
{  
  if (signal(SIGTERM, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGTERM\n");
  
  if (signal(SIGQUIT, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGQUIT\n");
  
  if (signal(SIGINT, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGINT\n");

  if (signal(SIGUSR1, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGUSR1\n");
  
  if (signal(SIGUSR2, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGUSR1\n");
  
  if (signal(SIGABRT, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGABRT\n");
}


int main(int argc, char *argv[])
{
  Pipe p;
  char host[50];
  setup_signals();
  
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
