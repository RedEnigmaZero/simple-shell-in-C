#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>     // For O_WRONLY, O_CREAT, O_TRUNC
#include <sys/types.h> // For pid_t
#include <sys/wait.h>  // For waitpid and WIFEXITED

#define CMDLINE_MAX 512
#define ARG_MAX 16
// #define TKN_MAX 32 // not sure if needed but is in project specification
#define PATH_MAX 4096

void piping(char *cmdline, int pipe_count, int *exit_status)
{
        int pipes[pipe_count][2];
        pid_t pids[pipe_count + 1];
        char *commands[pipe_count + 1][ARG_MAX + 1];
        int i, j;
        
        // Create local copy of command to parse
        char cmd_copy[CMDLINE_MAX];
        strncpy(cmd_copy, cmdline, CMDLINE_MAX);
        
        // Parse all commands
        char *arg = strtok(cmd_copy, " \t");
        i = 0;  // command index
        j = 0;  // argument index
        
        while (arg != NULL) {
                if (strcmp(arg, "|") == 0) {
                        commands[i][j] = NULL;  // Null terminate current command
                        i++;  // Move to next command
                        j = 0;  // Reset argument index
                } else {
                        if (j >= ARG_MAX) {
                                fprintf(stderr, "Error: too many process arguments\n");
                                return;
                        }
                        commands[i][j++] = arg;
                }
                arg = strtok(NULL, " \t");
        }
        commands[i][j] = NULL;  // Null terminate last command
        
        // Create all pipes
        for (i = 0; i < pipe_count; i++) {
                if (pipe(pipes[i]) == -1) {
                        perror("pipe");
                        return;
                }
        }
        
        // Create all processes
        for (i = 0; i <= pipe_count; i++) {
                pids[i] = fork();
                if (pids[i] == -1) {
                        perror("fork");
                        return;
                }
                
                if (pids[i] == 0) {
                        // Child process
                        if (i > 0) {
                                if (dup2(pipes[i-1][0], STDIN_FILENO) == -1) {
                                        perror("dup2");
                                        exit(1);
                                }
                        }
                        if (i < pipe_count) {
                                if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                                        perror("dup2");
                                        exit(1);
                                }
                        }
                        
                        // Close all pipe fds
                        for (j = 0; j < pipe_count; j++) {
                                close(pipes[j][0]);
                                close(pipes[j][1]);
                        }
                        
                        execvp(commands[i][0], commands[i]);
                        fprintf(stderr, "Error: command not found\n");
                        exit(1);
                }
        }
        
        // Parent process - close all pipe fds
        for (i = 0; i < pipe_count; i++) {
                close(pipes[i][0]);
                close(pipes[i][1]);
        }
        
        // Wait for all children and store their exit statuses
        for (i = 0; i <= pipe_count; i++) {
                int status;
                waitpid(pids[i], &status, 0);
                if (WIFEXITED(status)) {
                        exit_status[i] = WEXITSTATUS(status);
                } else {
                        exit_status[i] = 1;
                }
        }
}

void redirection(char *cmdline)
{
        char cmd_copy[CMDLINE_MAX];
        strncpy(cmd_copy, cmdline, CMDLINE_MAX);

        int s = 0;

        // Find the redirection symbol
        char *redir_pos = NULL;
        if (strchr(cmd_copy, '>') != NULL) {
                redir_pos = strchr(cmd_copy, '>');
        } else if (strchr(cmd_copy, '<') != NULL) {
                redir_pos = strchr(cmd_copy, '<');
                s = 1;

        } 
        
        if (!redir_pos) {
                fprintf(stderr, "Error: redirection symbol not found\n");
                exit(1);
        }
        
        // Split the command at the redirection
        *redir_pos = '\0';
        char *output_file = redir_pos + 1;
        
        // Skip leading whitespace in output filename
        while (*output_file == ' ' || *output_file == '\t')
                output_file++;
                
        if (*output_file == '\0') {
                fprintf(stderr, "Error: no output file\n");
                exit(1);
        }

        // Parse command and arguments
        char *arg_vect[ARG_MAX + 1];
        int i = 0;
        char *arg = strtok(cmd_copy, " \t");
        
        while (arg != NULL) {
                if (i >= ARG_MAX) {
                        fprintf(stderr, "Error: too many process arguments\n");
                        exit(1);
                }
                arg_vect[i++] = arg;
                arg = strtok(NULL, " \t");
        }
        arg_vect[i] = NULL;

        int fd;
        if (s == 0) {  // Output redirection (>)
                fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        } else {  // Input redirection (<)
                fd = open(output_file, O_RDONLY);
        }

        if (fd == -1) {
                perror("open");
                exit(1);
        }
        
        switch (s)
        {
        case 0:
                // Redirect stdout to file
                if (dup2(fd, STDOUT_FILENO) == -1) {
                        perror("dup2");
                        exit(1);
                }
                break;
        
        case 1:
                // Redirect stdin from file
                if (dup2(fd, STDIN_FILENO) == -1) {
                        perror("dup2");
                        exit(1);
                }
                break;
        }
        close(fd);
        
        // Execute command
        execvp(arg_vect[0], arg_vect);
        fprintf(stderr, "Error: command not found\n");
        exit(1);
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
                if (!strcmp(cmd, "exit")) { // this does not allow for whitespace like the sshell_ref does, but will let slide (THE NEW SSHELL_REF ACCOUNTS FOR THIS!!!)
                        fprintf(stderr, "Bye...\n");
                        fprintf(stderr, "+ completed 'exit' [0]\n");
                        break;
                }

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
                int pipe_count = 0;

                int i = 0;
                int too_many_args = 0;
                
                while (arg != NULL) {
                    if (i >= ARG_MAX) {
                        too_many_args = 1;
                        break; // Stop parsing shen we've hit the limit
                    }
                
                    if (strcmp(arg, "|") == 0) { 
                        pipe_count++;
                    } 
                
                    arg_vect[i++] = arg;
                    arg = strtok(NULL, " \t");
                }

                if (strchr(cmd_copy, '|') != NULL) {
                        s = 1; 
                }
                if (strchr(cmd_copy, '>') != NULL || strchr(cmd_copy, '<') != NULL) {
                        s = 2; 
                }

                arg_vect[i] = NULL;
                
                if (too_many_args) {
                    fprintf(stderr, "Error: too many process arguments\n");
                    continue;
                }

                arg_vect[i] = NULL; // null terminate

                /* PWD */
                if (!strcmp(arg_vect[0], "pwd")) {
                        char cwd[PATH_MAX];
                        if (getcwd(cwd, sizeof(cwd)) != NULL) {
                            printf("%s\n", cwd);
                            fprintf(stderr, "+ completed '%s' [0]\n", cmd_copy);
                        } else {
                            perror("pwd");
                            fprintf(stderr, "+ completed '%s' [1]\n", cmd_copy);
                        }
                        continue;
                }

                /* CHANGE DIRECTORY */
                if (!strcmp(arg_vect[0], "cd")) {
                        char *target = arg_vect[1];
                        
                        // Try to change directory
                        if (chdir(target) == 0) {
                                fprintf(stderr, "+ completed '%s' [0]\n", cmd_copy);
                        } else {
                                fprintf(stderr, "Error: cannot cd into directory\n");
                                fprintf(stderr, "+ completed '%s' [1]\n", cmd_copy);
                        }
                        
                        continue;
                }

                pid_t pid = fork();
                if (pid == 0) {
                        // child
                        switch (s)
                        {
                        case 1:
                                /* pipeline */
                                {
                                        int exit_status[4] = {0}; // Max 4 commands (3 pipes)
                                        piping(cmd_copy, pipe_count, exit_status);
                                        
                                        // Only parent should print completion message
                                        fprintf(stderr, "+ completed '%s'", cmd_copy);
                                        for (int i = 0; i <= pipe_count; i++) {
                                                fprintf(stderr, " [%d]", exit_status[i]);
                                        }
                                        fprintf(stderr, "\n");
                                        exit(0);  // Exit child process
                                }
                        
                        case 2:
                                /* output redirection */
                                redirection(cmd_copy);
                                break;  // Add this to prevent fallthrough

                        default:
                                execvp(arg_vect[0], arg_vect);
                                fprintf(stderr, "Error: command not found\n");
                                exit(1);
                        }
                } else if (pid > 0) {
                        // parent
                        int status;
                        waitpid(pid, &status, 0);
                        if (s != 1) {  // Only print completion for non-piped commands
                                fprintf(stderr, "+ completed '%s' [%d]\n", cmd_copy, WEXITSTATUS(status));
                        }
                } else {
                        perror("fork");
                        continue; // allow shell to continue upon fork error
                }
        }

        return EXIT_SUCCESS;
}
