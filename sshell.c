#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h> // Needed for pid_t
#include <sys/wait.h> // Needed for waitpid and WIFEXITED

#define CMDLINE_MAX 512
#define ARG_MAX 16
// #define TKN_MAX 32 // not sure if needed but is in project specification

void piping(char *cmdline)
{
        int pipefd[2];
        pid_t pid1, pid2;
        char *cmd1[ARG_MAX + 1];
        char *cmd2[ARG_MAX + 1];
        int i = 0;
        
        // Create local copy of command to parse
        char cmd_copy[CMDLINE_MAX];
        strncpy(cmd_copy, cmdline, CMDLINE_MAX);
        
        // Parse first command (before pipe)
        char *arg = strtok(cmd_copy, " \t");
        while (arg != NULL && strcmp(arg, "|") != 0) {
                if (i >= ARG_MAX) {
                        fprintf(stderr, "Error: too many process arguments\n");
                        return;
                }
                cmd1[i++] = arg;
                arg = strtok(NULL, " \t");
        }
        cmd1[i] = NULL;

        // Parse second command (after pipe)
        i = 0;
        arg = strtok(NULL, " \t");  // Get first token after pipe
        while (arg != NULL) {
                if (i >= ARG_MAX) {
                        fprintf(stderr, "Error: too many process arguments\n");
                        return;
                }
                cmd2[i++] = arg;
                arg = strtok(NULL, " \t");
        }
        cmd2[i] = NULL;

        // Create pipe
        if (pipe(pipefd) == -1) {
                perror("pipe");
                return;
        }

        // Fork first child
        if ((pid1 = fork()) == -1) {
                perror("fork");
                close(pipefd[0]);
                close(pipefd[1]);
                return;
        }

        if (pid1 == 0) {
                // First child - writes to pipe
                close(pipefd[0]);  // Close read end
                dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to pipe
                close(pipefd[1]);  // Close write end after dup2
                execvp(cmd1[0], cmd1);
                perror("execvp");
                exit(1);
        }

        // Fork second child
        if ((pid2 = fork()) == -1) {
                perror("fork");
                close(pipefd[0]);
                close(pipefd[1]);
                return;
        }

        if (pid2 == 0) {
                // Second child - reads from pipe
                close(pipefd[1]);  // Close write end
                dup2(pipefd[0], STDIN_FILENO);  // Redirect stdin to pipe
                close(pipefd[0]);  // Close read end after dup2
                execvp(cmd2[0], cmd2);
                perror("execvp");
                exit(1);
        }

        // Parent process
        close(pipefd[0]);
        close(pipefd[1]);
        
        // Wait for both children
        int status1, status2;
        waitpid(pid1, &status1, 0);
        waitpid(pid2, &status2, 0);
}

int main(void)
{
        char cmd[CMDLINE_MAX];
        char *eof;

        while (1) {
                char *nl;

                /* Print prompt */
                printf("sshell$ ");
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

                /* Builtin command */
                if (!strcmp(cmd, "exit")) { // this does not allow for whitespace like the sshell_ref does, but will let slide
                        fprintf(stderr, "Bye...\n");
                        fprintf(stderr, "+ completed 'exit' [0]\n"); // not sure if hardcoding is best practice here
                        break;
                }

                // my work below

                // skip empty line in the case the user presses enter with no text or only whitespace
                int is_blank = 1;
                for (int i = 0; cmd[i] != '\0'; i++) {
                        if(cmd[i] != ' ' && cmd[i] != '\t') {
                                is_blank = 0;
                                break;
                        }
                }
                if (is_blank)
                        continue;

                // copying entire input to output in + complete section
                char cmd_copy[CMDLINE_MAX];
                strncpy(cmd_copy, cmd, CMDLINE_MAX);

                // parsing args in parent to prevent child showing + completed
                char *arg_vect[ARG_MAX + 1];
                char *arg = strtok(cmd, " \t");

                int s = 0;

                int i;
                for (i = 0; i < ARG_MAX; i++) {
                        if (!arg)
                                break;

                        // Not sure if I should include this, it's in the project specification but is not handled by sshell_ref
                        // if (strlen(arg) > TKN_MAX) {
                        //     fprintf(stderr, "Error: token exceeds max length of %d characters\n", TKN_MAX);
                        //     continue;
                        // }
                        if (strcmp(arg, "|") == 0) {
                                s = 1;
                        } else if (strcmp(arg, ">") == 0) {
                                s = 2;
                        }

                        arg_vect[i] = arg;
                        arg = strtok(NULL, " \t");
                }

                if (arg != NULL && i == ARG_MAX) {
                        fprintf(stderr, "Error: too many process arguments\n");
                        continue;
                }

                arg_vect[i] = NULL; // null terminate

                pid_t pid = fork();
                if (pid == 0) {
                        // child
                        switch (s)
                        {
                        case 1:
                                /* pipeline */
                                piping(cmd_copy);
                                continue;;
                        
                        case 2:
                                /* output redirection */
                                
                        default:
                        execvp(arg_vect[0], arg_vect); // run first arg as cmd
                        fprintf(stderr, "Error: command not found\n");
                        exit(1); // exit back to parent, useless to continue on child, exit status same as sshell_ref
                        }
                } else if (pid > 0) {
                        // parent
                        int status;
                        waitpid(pid, &status, 0);
                        fprintf(stderr, "+ completed '%s' [%d]\n", cmd_copy, WEXITSTATUS(status));
                        // warning!! this does not handle a segmentation fault such as if(!WIFEXITED(status);
                        // sshell_ref doesn't handle it though, so kept as it
                        // keeping this as a future reference
                } else {
                        perror("fork");
                        continue; // allow shell to continue upon fork error
                }
        }

        return EXIT_SUCCESS;
}
