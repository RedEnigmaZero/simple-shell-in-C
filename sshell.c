#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>     // For O_WRONLY, O_CREAT, O_TRUNC
#include <sys/types.h> // For pid_t
#include <wait.h>  // For waitpid and WIFEXITED
#include <signal.h> // For signal and SIGCHLD

#define CMDLINE_MAX 512
#define ARG_MAX 16
// #define TKN_MAX 32 // not sure if needed but is in project specification
#define PATH_MAX 4096

pid_t bg_pid = -1;
char bg_cmd[CMDLINE_MAX];

void piping(char *cmdline, int pipe_count, int *exit_status)
{
        int pipes[pipe_count][2];
        pid_t pids[pipe_count + 1];
        char *commands[pipe_count + 1][ARG_MAX + 1];
        int i;

        // Create local copy of command to parse
        char cmd_copy[CMDLINE_MAX];
        strncpy(cmd_copy, cmdline, CMDLINE_MAX);

        // Parse commands using pipe symbol as delimiter
        char *cmd_part = cmd_copy;
        char *pipe_pos;
        i = 0;

        while (i <= pipe_count)
        {
                // Find next pipe symbol
                pipe_pos = strchr(cmd_part, '|');
                if (pipe_pos)
                {
                        *pipe_pos = '\0'; // Split at pipe symbol
                }

                // Parse arguments for current command
                char *arg;
                int j = 0;
                arg = strtok(cmd_part, " \t");

                while (arg != NULL)
                {
                        if (j >= ARG_MAX)
                        {
                                fprintf(stderr, "Error: too many process arguments\n");
                                return;
                        }
                        commands[i][j++] = arg;
                        arg = strtok(NULL, " \t");
                }
                commands[i][j] = NULL; // Null terminate command arguments

                // Move to next command after pipe
                if (pipe_pos)
                {
                        cmd_part = pipe_pos + 1;
                        // Skip leading whitespace
                        while (*cmd_part == ' ' || *cmd_part == '\t')
                                cmd_part++;
                }
                i++;
        }

        // Create all pipes
        for (i = 0; i < pipe_count; i++)
        {
                if (pipe(pipes[i]) == -1)
                {
                        perror("pipe");
                        return;
                }
        }

        // Create all processes
        for (i = 0; i <= pipe_count; i++)
        {
                pids[i] = fork();
                if (pids[i] == -1)
                {
                        perror("fork");
                        return;
                }

                if (pids[i] == 0)
                {
                        // Child process
                        if (i > 0)
                        {
                                if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1)
                                {
                                        perror("dup2");
                                        exit(1);
                                }
                        }
                        if (i < pipe_count)
                        {
                                if (dup2(pipes[i][1], STDOUT_FILENO) == -1)
                                {
                                        perror("dup2");
                                        exit(1);
                                }
                        }

                        // Close all pipe fds
                        for (int j = 0; j < pipe_count; j++)
                        {
                                close(pipes[j][0]);
                                close(pipes[j][1]);
                        }

                        if (commands[i][0] == NULL && commands[i+1][0] != NULL)
                        {
                            fprintf(stderr, "Error: missing command\n");
                            exit(1);
                        }
                      
                        

                        execvp(commands[i][0], commands[i]);
                        fprintf(stderr, "Error: command not found\n");
                        exit(1);
                }
        }

        // Parent process - close all pipe fds
        for (i = 0; i < pipe_count; i++)
        {
                close(pipes[i][0]);
                close(pipes[i][1]);
        }

        // Wait for all children and store their exit statuses
        for (i = 0; i <= pipe_count; i++)
        {
                int status;
                waitpid(pids[i], &status, 0);
                if (WIFEXITED(status))
                {
                        exit_status[i] = WEXITSTATUS(status);
                }
                else
                {
                        exit_status[i] = 1;
                }
        }

        // After collecting all exit statuses, check for errors and print completion
        int has_error = 0;
        for (i = 0; i <= pipe_count; i++) {
                if (exit_status[i] != 0) {
                        has_error = 1;
                        break;
                }
        }

        if (!has_error) {
                fprintf(stderr, "+ completed '%s'", cmdline);
                for (i = 0; i <= pipe_count; i++) {
                        if (i == 0)
                                fprintf(stderr, " [%d]", exit_status[i]);
                        else
                                fprintf(stderr, "[%d]", exit_status[i]);
                }
                fprintf(stderr, "\n");
        }
}

int redirection(char *cmdline)  // Change return type to int
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
                return 1;  // Error status
        }

        // Split the command at the redirection
        *redir_pos = '\0';
        char *output_file = redir_pos + 1;

        // Skip leading whitespace in output filename
        while (*output_file == ' ' || *output_file == '\t')
                output_file++;

        if (*output_file == '\0') {
                if (s == 0)
                        fprintf(stderr, "Error: no output file\n");
                else
                        fprintf(stderr, "Error: no input file\n");
                return 1;
        }

        // Parse command and arguments
        char *arg_vect[ARG_MAX + 1];
        int i = 0;
        char *arg = strtok(cmd_copy, " \t");

        while (arg != NULL) {
                if (i >= ARG_MAX) {
                        fprintf(stderr, "Error: too many process arguments\n");
                        return 1;
                }
                arg_vect[i++] = arg;
                arg = strtok(NULL, " \t");
        }
        arg_vect[i] = NULL;

        if (arg_vect[0] == NULL) {
                fprintf(stderr, "Error: missing command\n");
                return 1;
        }

        // Open file with appropriate mode
        int fd = (s == 0) ? 
                open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644) :
                open(output_file, O_RDONLY);

        if (fd == -1) {
                fprintf(stderr, "Error: cannot open %s file\n", 
                        (s == 0) ? "output" : "input");
                return 1;
        }

        // Perform redirection
        if (dup2(fd, (s == 0) ? STDOUT_FILENO : STDIN_FILENO) == -1) {
                perror("dup2");
                close(fd);
                return 1;
        }
        close(fd);

        // Execute command
        execvp(arg_vect[0], arg_vect);
        fprintf(stderr, "Error: command not found\n");
        return 1;
}

int main(void)
{
        char cmd[CMDLINE_MAX];
        char *eof;

        while (1)
        {
                char *nl;

                /* Print prompt */
                printf("sshell@ucd$ ");
                fflush(stdout);

                /* Get command line */
                eof = fgets(cmd, CMDLINE_MAX, stdin);
                if (!eof)
                        /* Make EOF equate to exit */
                        strncpy(cmd, "exit\n", CMDLINE_MAX);

                /* Print command line if stdin is not provided by terminal */
                if (!isatty(STDIN_FILENO))
                {
                        printf("%s", cmd);
                        fflush(stdout);
                }

                /* Remove trailing newline from command line */
                nl = strchr(cmd, '\n');
                if (nl)
                        *nl = '\0';

                /* Builtin command */
                if (!strcmp(cmd, "exit"))
                { // this does not allow for whitespace like the sshell_ref does, but will let slide (THE NEW SSHELL_REF ACCOUNTS FOR THIS!!!)
                        fprintf(stderr, "Bye...\n");
                        fprintf(stderr, "+ completed 'exit' [0]\n");
                        break;
                }

                // skip empty line in the case the user presses enter with no text or only whitespace
                int is_blank = 1;
                for (int i = 0; cmd[i] != '\0'; i++)
                {
                        if (cmd[i] != ' ' && cmd[i] != '\t')
                        {
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

                // Replace the single pipe check with a loop that counts all pipes
                char *pipe_check = cmd_copy;
                pipe_count = 0;
                while ((pipe_check = strchr(pipe_check, '|')) != NULL) {
                        pipe_count++;
                        pipe_check++; // Move past the found pipe symbol
                }

                // Then modify the argument parsing to just collect arguments
                int invalid_combination = 0;
                while (arg != NULL)
                {
                        if (i >= ARG_MAX)
                        {
                                too_many_args = 1;
                                break; // Stop parsing shen we've hit the limit
                        }

                        // Check for mislocated redirection
                        if (strchr(cmd_copy, '|') != NULL) {
                                if (strchr(cmd_copy, '>') != NULL) {
                                    fprintf(stderr, "Error: mislocated background sign\n");  // Change this line
                                    invalid_combination = 1;
                                    break;
                                }
                                if (strchr(cmd_copy, '<') != NULL) {
                                    fprintf(stderr, "Error: mislocated input redirection\n");  // And this one
                                    invalid_combination = 1;
                                    break;
                                }
                        }

                        arg_vect[i++] = arg;
                        arg = strtok(NULL, " \t");
                }

                // Skipping execution if invalid combination (redirecting before piping)
                if (invalid_combination) {
                        continue;
                }

                if (strchr(cmd_copy, '|') != NULL)
                {
                        s = 1;
                }
                if (strchr(cmd_copy, '>') != NULL || strchr(cmd_copy, '<') != NULL)
                {
                        s = 2;
                }
                if (strchr(cmd_copy, '&') != NULL)
                {
                        s = 3;
                }

                //fprintf(stderr, "s = '%d'", s);

                arg_vect[i] = NULL;

                if (too_many_args)
                {
                        fprintf(stderr, "Error: too many process arguments\n");
                        continue;
                }

                arg_vect[i] = NULL; // null terminate

                /* PWD */
                if (!strcmp(arg_vect[0], "pwd"))
                {
                        char cwd[PATH_MAX];
                        if (getcwd(cwd, sizeof(cwd)) != NULL)
                        {
                                printf("%s\n", cwd);
                                fprintf(stderr, "+ completed '%s' [0]\n", cmd_copy);
                        }
                        else
                        {
                                perror("pwd");
                                fprintf(stderr, "+ completed '%s' [1]\n", cmd_copy);
                        }
                        continue;
                }

                /* CHANGE DIRECTORY */
                if (!strcmp(arg_vect[0], "cd"))
                {
                        char *target = arg_vect[1];

                        // Try to change directory
                        if (chdir(target) == 0)
                        {
                                fprintf(stderr, "+ completed '%s' [0]\n", cmd_copy);
                        }
                        else
                        {
                                fprintf(stderr, "Error: cannot cd into directory\n");
                                fprintf(stderr, "+ completed '%s' [1]\n", cmd_copy);
                        }

                        continue;
                }

                pid_t pid = fork();
                if (pid == 0)
                {
                        // child
                        switch (s)
                        {
                        case 1:
                                /* pipeline */
                                {
                                        int exit_status[4] = {0}; // Max 4 commands (3 pipes)
                                        piping(cmd_copy, pipe_count, exit_status);
                                        exit(0);
                                }

                        case 2:
                                /* output redirection */
                                {
                                        int ret = redirection(cmd_copy);
                                        if (ret == 1) {
                                                exit(1);  // Exit with error status
                                        }
                                        exit(0);  // Exit with success status
                                }

                        case 3:
                                /* background process */
                                {
                                        // Ignore SIGCHLD signal to prevent zombie processes
                                        signal(SIGCHLD, SIG_IGN);

                                        // Clean '&' from argument vector
                                        int bg_argc = 0;
                                        while (arg_vect[bg_argc] != NULL) {
                                                char *amp = strchr(arg_vect[bg_argc], '&');
                                                if (amp) {
                                                        *amp = '\0';
                                                        if (strlen(arg_vect[bg_argc]) == 0) {
                                                        arg_vect[bg_argc] = NULL;
                                                        }
                                                }
                                                bg_argc++;
                                        }

                                        // Execute command in background
                                        execvp(arg_vect[0], arg_vect);
                                        fprintf(stderr, "Error: command not found\n");
                                        exit(1);
                                }

                        default:
                                execvp(arg_vect[0], arg_vect);
                                fprintf(stderr, "Error: command not found\n");
                                exit(1);
                        }
                }
                else if (pid > 0)
                {
                        // parent
                        if (s == 3)
                        {
                                // Store background process info
                                bg_pid = pid;
                                strncpy(bg_cmd, cmd_copy, CMDLINE_MAX);
                                
                        }
                        else 
                        {
                                int status;
                                
                                // Check if background process completed
                                if (bg_pid != -1) {
                                        int bg_status;
                                        if (waitpid(bg_pid, &bg_status, WNOHANG) > 0) {
                                                fprintf(stderr, "+ completed '%s' [%d]\n", 
                                                        bg_cmd, WEXITSTATUS(bg_status));
                                                bg_pid = -1;
                                        }
                                }
                                
                                // Wait for foreground process
                                waitpid(pid, &status, 0);
                                if (s != 1 && !(s == 2 && WEXITSTATUS(status) != 0))
                                {
                                        fprintf(stderr, "+ completed '%s' [%d]\n", 
                                                cmd_copy, WEXITSTATUS(status));
                                }
                        }
                        
                }
                else
                {
                        perror("fork");
                        continue; // allow shell to continue upon fork error
                }
        }

        return EXIT_SUCCESS;
}
