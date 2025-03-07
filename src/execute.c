#include <execute.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

char *get_exec_path(char *cmd) {
    if (access(cmd, X_OK) == 0) {
        return strdup(cmd); // If the command is an absolute/relative path and executable
    }

    char *path = getenv("PATH");
    if (!path) {
        return NULL;
    }

    char *path_dup = strdup(path);
    char *token = strtok(path_dup, ":");

    while (token) {
        char full_path[MAX_PATH_LENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", token, cmd);

        if (access(full_path, X_OK) == 0) {
            free(path_dup);
            return strdup(full_path);
        }

        token = strtok(NULL, ":");
    }

    free(path_dup);
    return NULL;
}

int exec_cmd(char *cmd, char *const argv[]) {
    char *exec_path = get_exec_path(cmd);
    if (!exec_path) {
        fprintf(stderr, "Command not found: %s\n", cmd);
        return 127; // Common exit code for command not found
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        free(exec_path);
        return -1;
    } else if (pid == 0) {
        // Child process
        
        // Reset signal handlers to default in the child
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        
        // Execute command
        execvp(exec_path, argv);
        
        // If execvp returns, there was an error
        perror("execvp");
        free(exec_path);
        exit(127); // If execvp fails, return 127
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        free(exec_path);

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            // If the process was terminated by a signal (e.g., SIGINT)
            if (WTERMSIG(status) == SIGINT) {
                // Don't print a newline, readline will handle it
                // Just ensure cursor is visible
                fprintf(stdout, "\033[?25h"); // Show cursor ANSI escape code
                fflush(stdout);
            }
            return 128 + WTERMSIG(status); // Standard signal exit code pattern
        } else {
            return -1; // Indicate abnormal termination
        }
    }
}