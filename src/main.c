#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <unistd.h>

#include <execute.h>
#include <data.h>
#include <config.h>
#include <util/history.h>
#include <sys/prctl.h>

#define MAX_CMD_LEN 4096

volatile sig_atomic_t command_running = 0;

void sigint_handler(int sig) {
    if (!command_running) {
        printf("\n");
        fprintf(stdout, "\033[?25h");
        rl_on_new_line();
        rl_replace_line("", 0);
        rl_redisplay();
    }
}

int main(int argc, char *argv[]) {
    char *line;
    char prompt[MAX_PROMPT_LENGTH];

    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    init_shell_data();
    config_init();
    
    change_cwd("~");

    prctl(PR_SET_NAME, "jash", 0, 0, 0);
    
    history_init();

    rl_catch_signals = 0;
    rl_catch_sigwinch = 1;
    rl_set_signals();
    
    while (1) {
        send_prompt_to_buffer(prompt);
        
        line = readline(prompt);
        
        if (line == NULL) {
            printf("\n");
            break;
        }
        
        if (line[0] == '\0') {
            free(line);
            continue;
        }
        
        history_add(line);
        
        if (strcmp(line, "exit") == 0) {
            free(line);
            break;
        }
        
        char *cmd_argv[MAX_CMD_LEN];
        char *cmd_copy = strdup(line);
        char *token = strtok(cmd_copy, " ");
        int i = 0;
        
        while (token) {
            cmd_argv[i] = token;
            token = strtok(NULL, " ");
            i++;
        }
        cmd_argv[i] = NULL;
        
        if (strcmp(cmd_argv[0], "cd") == 0) {
            change_cwd(i > 1 ? cmd_argv[1] : NULL);
        } else {
            command_running = 1;
            
            exec_cmd(cmd_argv[0], cmd_argv);
            
            command_running = 0;
        }
        
        free(cmd_copy);
        free(line);
    }
    
    history_cleanup();
    free(global_shell_data);
    
    return 0;
}