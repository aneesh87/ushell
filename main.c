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
#include <sys/time.h>
#include <sys/resource.h>


#define NUM_BUILTINS 13
#define MAXLEN 1024
#define DEFAULT_PRIO 4
#define err(x) fprintf(stderr, "%s\n", x);

extern char **environ;

char shell_built_ins[NUM_BUILTINS][20] = {"cd", "pwd", "echo", "setenv", "logout", "end", "jobs",  
                                          "where", "nice", "kill", "unsetenv", "fg", "bg"};

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
    char env_path[MAXLEN];
	char cmd_str[MAXLEN];
	char * c = getenv("PATH");
	char * token;

	if (cname == NULL) return NULL;

    // This is an out parameter needs to be on heap
    char * buffer = calloc(MAXLEN, sizeof(char));
    int cmd_found = 0;
	strncpy(env_path, c, MAXLEN);
    
    token = strtok(env_path, ":");
    while (token) {
    	strncpy(cmd_str, token,  MAXLEN);
    	strncat(cmd_str, "/",    MAXLEN - strlen(cmd_str));
    	strncat(cmd_str, cname,  MAXLEN - strlen(cmd_str));
    	if (stat(cmd_str, &sti) == 0) {
    		if (strlen(buffer) == 0) {
    		    snprintf(buffer, MAXLEN, "%s\n", cmd_str);
    		} else {
    			strncat(buffer, cmd_str, MAXLEN - strlen(buffer));
    			strncat(buffer, "\n", MAXLEN - strlen(buffer));
    		}
    		cmd_found = 1;
    	}
    	token = strtok(NULL, ":");
    }
    if (cmd_found) {
    	return buffer;
    } else {
        free(buffer);
        return NULL;
    }
}
void setup_pipes_io(Cmd c, int *lp, int *rp) {
    
    int fd = -1;
    if (c) {
        //printf("%s%s ", c->exec == Tamp ? "BG " : "", c->args[0]);
        if (c->in != Tnil) {
        	
        	switch (c->in) {
            
                case Tin: {
                     //printf("<(%s) ", c->infile);
                     fd = open(c->infile, O_RDONLY);
                     if (fd == -1) {
                         fprintf(stderr, "%s: Permission denied\n", c->infile);
                         exit(1);
                     } else {
                     	dup2(fd, 0);
                        close(fd);
                     }
                }
                break;
            
                case Tpipe: 
                case TpipeErr: {

                	 if (lp == NULL) {
                	 	 err("Something went wrong in inp pipes");
                	 	 exit(1);
                	 }
                	 //printf("cmd %s\n", c->args[0]);
                	 //fflush(stdout);
                	 //printf("Desc1 is %d Desc2 is %d\n",lp[0], lp[1]);
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
	              close(fd);
	              //printf(">(%s) ", c->outfile);
	           }       
	           break;
             
               case Tapp: {
               	    fd = open(c->outfile, O_WRONLY | O_APPEND | O_CREAT, 0660);
	                if (fd == -1) { 
	                	fprintf(stderr, "%s: Permission denied\n", c->outfile);
	                    exit(1);
	                }
	                dup2(fd, 1);
	                close(fd);
	                //printf(">>(%s) ", c->outfile);
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
	                close(fd);
	                //printf(">&(%s) ", c->outfile);
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
	                close(fd);
	                //printf(">>&(%s) ", c->outfile);
	           }
	           break;
               case Tpipe: {
            
               	    if (rp == NULL) {
               	    	err("Something went wrong in output pipes");
               	    	exit(1);
               	    }
               	    //printf("| ");
               	    //printf("Desc1 is %d Desc2 is %d\n",rp[0], rp[1]);
               	    dup2(rp[1], 1);
               	    close(rp[0]);
	           }
	           break;
               case TpipeErr: {
               	    if (rp == NULL) {
               	    	err("Something went wrong in output pipes");
               	    	exit(1);
               	    }
               	    dup2(rp[1], 2);
               	    dup2(rp[1], 1);
               	    close(rp[0]);
	                //printf("|& ");
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
            printf("%s", c->args[1]);
            for (i = 2; c->args[i]!= NULL; i++)
            	printf(" %s",c->args[i]);
            printf("\n");
        }
        return;
    }

    if(strcmp(c->args[0], "where") == 0) {
    	if (c->args[1] == NULL) {
    		err("where: too few arguments");
    		return;
    	}
    	if (check_if_builtin(c->args[1])) {
    		printf("%s: shell built-in command.\n", c->args[1]);
    		return;
    	}

    	char * path = cmd_path(c->args[1]);
    	if (path) {
    		printf("%s", path);
    		free(path);
        } else {
        	fprintf(stderr, "%s: command not found\n", c->args[1]);
        }
    	return;
    }

    if(strcmp(c->args[0],"setenv") == 0) {
    	if (c->args[1] == NULL) {
    		int i;
            char *c = *environ;
            for (i = 1; c; i++) {
                 printf("%s\n", c);
                 c = *(environ+i);
            }
    	} else {
    		 if (setenv(c->args[1], c->args[2], 1) != 0) {
                 err("setenv: error");
    		 }
    	}
    	return;
    }

    if(strcmp(c->args[0], "unsetenv") == 0) {
       if (c->args[1] != NULL) {
           if (unsetenv(c->args[1]) != 0) { err("unsetenv: error ");}
       } else {
           err("unsetenv: too few arguments");
       }
       return;
    }

    if(strcmp(c->args[0], "nice") == 0) {
       if (c->nargs == 1) {
       	   int rc = setpriority(PRIO_PROCESS, getpid(), DEFAULT_PRIO);
       	   if (rc < 0) { err("nice: setpriority failed");}
       	   return;
       }
       int prio = DEFAULT_PRIO;
       int cmd_without_num = 0;
       if (c->args[1][0] == '-' || c->args[1][0] == '+') { // +(-)prio given
       	   char * num = c->args[1] + 1;
       	   prio = atoi(num);
       	   if (c->args[1][0] == '-') prio = prio * -1;
       } else if (c->args[1][0] >= '0' && c->args[1][0] <= '9') { // prio given
       	   prio = atoi(c->args[1]);
       } else { //command specified without number
       	   cmd_without_num = 1;
       }
       if (c->args[2] == NULL && cmd_without_num == 0) {
       	   int rc = setpriority(PRIO_PROCESS, getpid(), prio);
       	   if (rc < 0) { err("nice: setpriority failed");}
       	   return;
       }
       char * cmd_to_execute = c->args[2];
       if (cmd_without_num == 1) {
       	   cmd_to_execute = c->args[1];
       }

       int pid = fork();
       if (pid < 0) {
       	   err("nice: fork failed");
       	   return;
       } else if (pid == 0) {
       	    setpriority(PRIO_PROCESS, 0, prio);
            if (cmd_without_num == 1) {
       	        execvp(cmd_to_execute, c->args + 1);
            } else {
            	execvp(cmd_to_execute, c->args + 2);
            }
            if (errno == ENOENT) {
                fprintf(stderr, "%s: command not found\n", cmd_to_execute);
            } else {    
                fprintf(stderr, "%s: permission denied\n", cmd_to_execute);
            }
            exit(1);
       } else {
       	    waitpid(pid, NULL, 0);
       	    return;
       }
    }
}

static void prCmd(Cmd c, int * left, int * right)
{
    pid_t pid;

    if (check_if_builtin(c->args[0])) {
       if (c->next == NULL) {
       	   // reward:effort is very low for this complex trickery.
       	   // Just calling run_builtin may do minus few cases.
       	   int fd;
       	   int saved_stdout;
       	   int saved_stderr;
       	   int saved_stdin;

       	   if (c->in == Tin) {
       	   	    int fdin = open(c->infile, O_RDONLY);
                if (fdin == -1) {
                	if (errno == ENOENT) {
                		fprintf(stderr, "%s: File not found\n", c->infile);
                	} else {
                        fprintf(stderr, "%s: Permission denied\n", c->infile);
                    }
                    return;
                } else {
                     saved_stdin = dup(0);
                     dup2(fdin, 0);
                     close(fdin);
                }
       	   } else if (c->in == Tpipe || c->in == TpipeErr) {

                if (left == NULL) {
                	err("Something went wrong in pipes"); 
                	return;
                }
                saved_stdin = dup(0);
                dup2(left[0], 0);
                close(left[1]);
                close(left[0]);
       	   }
       	 // Last command in pipe is a builtin. Execute directly (csh).
       	 // Take care only of redirection. Last command has no right pipe.

           if (c->out == Tout || c->out == ToutErr) {
           
           	   fd = open(c->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0660);
	           if (fd == -1)  { 
	               fprintf(stderr, "%s: Permission denied\n", c->outfile);
	               return;
	           }
	       
	       } else if (c->out == Tapp || c->out == TappErr) {

	       	    fd = open(c->outfile, O_WRONLY | O_APPEND | O_CREAT, 0660);
	            if (fd == -1) {
	                fprintf(stderr, "%s: Permission denied\n", c->outfile);
	                return;
	            }
	       }

	       if (c->out == Tout || c->out == Tapp) {
	       	   saved_stdout = dup(1);
	       	   dup2(fd, 1);
	       	   close(fd);

	       } else if (c->out == ToutErr || c->out == TappErr) {
	       	   saved_stdout = dup(1);
	       	   saved_stderr = dup(2);
	       	   dup2(fd,1);
	       	   dup2(fd,2);
	       	   close(fd);
	       }

           run_builtin(c);
           
           if (c->out == Tout || c->out == Tapp) {
	       	   dup2(saved_stdout, 1);
	       	   close(saved_stdout);

	       } else if (c->out == ToutErr || c->out == TappErr) {
	       	   dup2(saved_stdout,1);
	       	   dup2(saved_stderr,2);
	       	   close(saved_stdout);
	       	   close(saved_stderr);
	       }

	       if(c->in == Tin || c->in == Tpipe || c->in == TpipeErr) {
	       	  dup2(saved_stdin, 0);
	       	  close(saved_stdin);
	       }

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
  //printf("Begin pipe%s\n", p->type == Pout ? "" : " Error");
  i = 0;
  for ( c = p->head; c != NULL; c = c->next, i++) {
    int * left =  NULL;
    int * right = NULL;
    if (i != 0) {
        left = fd + 2*(i -1);
        //printf("LEFT %d,%d\n", 2*(i-1), *left);
    } 
    if (i != pipe_count) {
        right = fd + 2*i;
        //printf(" RIGHT %d,%d\n", 2*i, *right);
    }
    prCmd(c, left, right);
    
    if (i > 0) {
    	close(fd[2*i - 2]);
    	close(fd[2*i - 1]);
    }
    //printf("Cmd #%d:%s \n", i+1, c->args[0]);
  }

  int pid,status;
  
  while ((pid = wait(&status)) != -1);	/* wait for all children */
		//fprintf(stderr, "process %d exits with %d\n", pid, WEXITSTATUS(status));
  
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
  
}

void process_ushrc() {

  char * home = getenv("HOME");
  char rcpath[512];
  int fd;
  int saved_stdin;
  
  strncpy(rcpath, home, 512);
  strncat(rcpath, "/.ushrc", 512);
  
  fd = open(rcpath, O_RDONLY);
  if (fd == -1) {
  	  if (errno == ENOENT) {
  	  	  err(".ushrc: not found");
  	  } else {
  	  	  err(".ushrc: Permission denied.")
  	  }
  } else {
  	  saved_stdin = dup(0);
  	  dup2(fd, 0);
  	  close(fd);
  	  Pipe p1 = parse();
  	  prPipe(p1);
  	  freePipe(p1);
  	  dup2(saved_stdin, 0);
  	  close(saved_stdin);
  }
}
int main(int argc, char *argv[])
{
  Pipe p;
  char host[50];
  
  setup_signals();
  process_ushrc();

  while ( 1 ) {
    if (gethostname(host,50) != 0) {
        strncpy(host,"armadillo",50);
    }
    printf("%s%% ", host);
    //weird behavior without below line
    fflush(stdout);
    p = parse();
    prPipe(p);
    freePipe(p);
  }
}

/*........................ end of main.c ....................................*/
