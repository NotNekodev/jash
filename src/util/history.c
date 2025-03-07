#include <util/history.h>
#include <readline/history.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <data.h>

void history_init() {
    // Initialize readline history
    using_history();
    
    // Limit history to MAX_HISTORY_LEN entries
    stifle_history(MAX_HISTORY_LEN);
    
    // Optionally load history from a file
    char history_file[1024];
    snprintf(history_file, sizeof(history_file), "%s/.jash_history", global_shell_data->home);
    read_history(history_file);
}

void history_add(const char *cmd) {
    if (cmd == NULL || *cmd == '\0')
        return;
        
    // Add command to history
    add_history(cmd);
}

void history_cleanup() {
    // Save history to a file
    char history_file[1024];
    snprintf(history_file, sizeof(history_file), "%s/.jash_history", global_shell_data->home);
    write_history(history_file);
    
    // Clear history
    clear_history();
}