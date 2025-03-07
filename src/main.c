#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <sys/prctl.h>

#include <core/execute.h>
#include <core/data.h>
#include <core/config.h>
#include <core/history.h>
#include <core/util/signal.h>
#include <core/completion.h>

#define MAX_CMD_LEN 4096

int main(int argc, char *argv[]) {
    prctl(PR_SET_NAME, "jash", 0, 0, 0);
    rl_readline_name = "jash";

    char *line;
    char prompt[MAX_PROMPT_LENGTH];

    init_shell_data();
    config_init();

    init_complete();
    
    change_cwd(glob_config->default_directory);
    apply_cursor_style();

    history_init();
    
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