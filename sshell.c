#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define CMDLINE_MAX 512

char **parse_command(char *cmd)
{
        char **args = malloc(CMDLINE_MAX * sizeof(char *));
        char *arg;
        int i = 0;

        arg = strtok(cmd, " ");
        while (arg != NULL) {
                if (i >= 16) {
                        return NULL;
                }
                
                args[i++] = arg;
                arg = strtok(NULL, " ");
        }
        args[i] = NULL;

        return args;
}

int main(void)
{
        char cmd[CMDLINE_MAX];
        char *eof;
        char commondin[CMDLINE_MAX];

        while (1) {
                char *nl;
                //int retval;

                /* Print prompt */
                printf("sshell@ucd$ ");
                fflush(stdout);

                /* Get command line */
                eof = fgets(cmd, CMDLINE_MAX, stdin);
                if (!eof)
                        /* Make EOF equate to exit */
                        strncpy(cmd, "exit\n", CMDLINE_MAX);

                /* Print command line if stdin is not provided by terminal */
                if (!isatty(STDIN_FILENO)) {
                        printf("%s", cmd);
                        fflush(stdout);
                }

                /* Remove trailing newline from command line */
                nl = strchr(cmd, '\n');
                if (nl)
                        *nl = '\0';
                
                strncpy(commondin, cmd, CMDLINE_MAX);

                /* Builtin command */
                if (!strcmp(cmd, "exit")) {
                        fprintf(stderr, "Bye...\n");
                        fprintf(stderr, "+ completed 'exit' [0]\n");
                        break;
                }
                
                pid_t pid;
                char **args = parse_command(cmd);

                char *t = strtok(cmd, " \t");
                if (t == NULL)
                {
                        continue;
                }


                pid = fork();
                if (pid == 0)
                {
                        /* Child */
                        execvp(args[0], args);
                        fprintf(stderr, "Error: command not found\n");
                        exit(1);
                } else if (pid > 0)
                {
                        /* Parent */
                        int status;
                        waitpid(pid, &status, 0);
                        if (args != NULL) {
                                printf("+ completed '%s' [%d]\n", commondin, WEXITSTATUS(status));
                        } else 
                        {
                                printf("Error: too many process arguments\n");
                        }
                        
                        
                }
                else
                {
                        perror("fork");
                        exit(1);
                }

                free(args);
                
                

                /* Regular command 
                retval = system(cmd);
                fprintf(stdout, "Return status value for '%s': %d\n",
                        cmd, retval);
                        */
        }

        return EXIT_SUCCESS;
}

