#include <core/history.h>
#include <readline/history.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <core/data.h>
#include <core/config.h>

void history_init() {
    using_history();
    
    stifle_history(glob_config->history_size);
    
    char history_file[glob_config->history_size + 512];
    snprintf(history_file, sizeof(history_file), "%s", glob_config->history_file);
    read_history(history_file);
}

void history_add(const char *cmd) {
    if (cmd == NULL || *cmd == '\0')
        return;
    add_history(cmd);
}

void history_cleanup() {
    char history_file[glob_config->history_size + 512];
    snprintf(history_file, sizeof(history_file), "%s", glob_config->history_file);
    write_history(history_file);
    
    clear_history();
}