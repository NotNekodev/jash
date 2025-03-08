#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <sys/prctl.h>

#include <core/execute.h>
#include <core/config.h>
#include <core/history.h>
#include <core/util/signal.h>
#include <core/completion.h>
#include <core/builtin.h>
#include <core/util/sanitize.h>

#define MAX_CMD_LEN 4096

int main(int argc, char *argv[]) {
    prctl(PR_SET_NAME, "jash", 0, 0, 0);
    rl_readline_name = "jash";

    char *line;
    char prompt[MAX_PROMPT_LENGTH];

    config_init();
    
    change_cwd(glob_config->default_directory);
    apply_cursor_style();

    history_init();

    if (glob_config->completion_enable) {
        init_builtin();
        init_complete();
    }
    
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

        trim_trailing_whitespace(line);
        
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

        sanitize_command_args(cmd_argv);

        if (strcmp(cmd_argv[0], "exit") == 0) {
            free(line);
            free(cmd_copy);
            break;
        }

        int result = builtin_handler(i, cmd_argv);

        if (result == -2) {
            command_running = 1;
            
            exec_cmd(cmd_argv[0], cmd_argv);
            
            command_running = 0;
        } else {
            ;;
        }
        
        free(cmd_copy);
        free(line);
    }
    
    history_cleanup();
    
    free(glob_config->prompt);
    free(glob_config->history_file);
    free(glob_config->default_directory);
    free(glob_config->cwd);
    free(glob_config->home);
    free(glob_config->user);
    free(glob_config->hostname);
    free(glob_config);
    
    return 0;
}