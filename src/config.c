#include "data.h"
#include <config.h>

#include <stddef.h>
#include <string.h>
#include <lib/inih.h>

config_t *glob_config = NULL;

static int handler(void* user, const char* section, const char* name, const char* value) {
    config_t *config = (config_t *)user;

    if (strcmp(section, "shell") == 0) {
        if (strcmp(name, "prompt") == 0) {
            config->prompt = strdup(value);
        }
    }
    return 1;
}

void config_init() {
    char path_to_conf[1024];
    snprintf(path_to_conf, 1024, "%s/.jashconf.ini", global_shell_data->home);

    glob_config = malloc(sizeof(config_t));
    
    if (ini_parse(path_to_conf, handler, glob_config) < 0) {
        printf("warn: couldnt load config using defaults!\n");
        glob_config->prompt = strdup("$$USER$$@$$HOST$$:$$DIR$$$ ");
    }

    global_shell_data->prompt = glob_config->prompt;
}