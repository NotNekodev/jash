#include "core/config.h"
#include <core/util/sanitize.h>

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char* trim_trailing_whitespace(char *str) {
    if (!str || *str == '\0')
        return str;
    
    size_t len = strlen(str);
    char* end = str + len - 1;
    
    while (end >= str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    
    return str;
}

void tilde_to_complete_path(const char* path, char* expanded_path, size_t size) {
    if (path[0] == '~') {
        const char *home = glob_config->home;  // Get the home directory
        if (!home) {
            fprintf(stderr, "HOME environment variable is not set.\n");
            return;
        }
        
        snprintf(expanded_path, size, "%s%s", home, path + 1);
    } else {
        strncpy(expanded_path, path, size - 1);
        expanded_path[size - 1] = '\0';
    }
}

char* sanitize_path_arg(const char* arg) {
    if (!arg) return NULL;
    
    if (arg[0] == '~' && (arg[1] == '/' || arg[1] == '\0')) {
        char expanded_path[1024];
        tilde_to_complete_path(arg, expanded_path, sizeof(expanded_path));
        return strdup(expanded_path);
    }
    
    return strdup(arg);
}

void sanitize_command_args(char** argv) {
    if (!argv) return;
    
    for (int i = 0; argv[i] != NULL; i++) {
        // Check if this argument looks like a path with tilde
        if (argv[i][0] == '~' && (argv[i][1] == '/' || argv[i][1] == '\0')) {
            char expanded_path[1024];
            tilde_to_complete_path(argv[i], expanded_path, sizeof(expanded_path));
            
            argv[i] = strdup(expanded_path);
        }
    }
}