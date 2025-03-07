#include <core/data.h>
#include <core/config.h>

#include <stddef.h>
#include <string.h>
#include <lib/inih.h>

config_t *glob_config = NULL;

static int handler(void* user, const char* section, const char* name, const char* value) {
    config_t *config = (config_t *)user;

    if (strcmp(section, "Core") == 0) {
        if (strcmp(name, "Prompt") == 0) {
            config->prompt = strdup(value);
        } else if (strcmp(name, "HistorySize") == 0) {
            config->history_size = atoi(value);
        } else if (strcmp(name, "HistoryFile") == 0) {
            config->history_file = strdup(value);
        } else if (strcmp(name, "DefaultDirectory") == 0) {
            config->default_directory = strdup(value);
        } else if (strcmp(name, "Debug") == 0) {
            config->debug = strcmp(value, "true") == 0 ? 1 : 0;
        }
    } else if (strcmp(section, "Cursor") == 0) {
        if (strcmp(name, "Style") == 0) {
            strncpy(config->cursor_style, value, sizeof(config->cursor_style) - 1);
        } else if (strcmp(name, "BlinkEnabled") == 0) {
            config->cursor_blink_enabled = strcmp(value, "true") == 0 ? 1 : 0;
        } else if (strcmp(name, "BlinkRate") == 0) {
            config->cursor_blink_rate = atoi(value);
        } else if (strcmp(name, "CustomSequence") == 0) {
            strncpy(config->cursor_custom_sequence, value, sizeof(config->cursor_custom_sequence) - 1);
        }
    }

    return 1;
}

void config_init() {
    char path_to_conf[1024];
    snprintf(path_to_conf, 1024, "%s/.jashconf.ini", global_shell_data->home);

    glob_config = malloc(sizeof(config_t));
    
    if (ini_parse(path_to_conf, handler, glob_config) < 0) {
        fprintf(stderr, "couldnt load config file\n");
        exit(1);
    }

    global_shell_data->prompt = glob_config->prompt;
}

void apply_cursor_style(void) {
    char* term = getenv("TERM");
    int is_xterm = (term && (strstr(term, "xterm") != NULL || strstr(term, "rxvt") != NULL));
    int is_linux = (term && strstr(term, "linux") != NULL);
    
    if (is_xterm) {
        if (strcmp(glob_config->cursor_style, "block") == 0) {
            printf("\033[%d q", glob_config->cursor_blink_enabled ? 1 : 2);
        } else if (strcmp(glob_config->cursor_style, "underline") == 0) {
            printf("\033[%d q", glob_config->cursor_blink_enabled ? 3 : 4);
        } else if (strcmp(glob_config->cursor_style, "bar") == 0) {
            printf("\033[%d q", glob_config->cursor_blink_enabled ? 5 : 6);
        } else if (strcmp(glob_config->cursor_style, "custom") == 0) {
            char processed[128] = {0};
            char* src = glob_config->cursor_custom_sequence;
            char* dst = processed;
            
            while (*src) {
                if (*src == '\\' && *(src+1) == '0' && *(src+2) == '3' && *(src+3) == '3') {
                    *dst++ = '\033';
                    src += 4;
                } else {
                    *dst++ = *src++;
                }
            }
            
            printf("%s", processed);
        }
        
        if (glob_config->cursor_blink_enabled && glob_config->cursor_blink_rate > 0) {
            printf("\033]12;%d\007", glob_config->cursor_blink_rate);
        }
    } else if (is_linux) {
        if (glob_config->cursor_blink_enabled) {
            printf("\033[?12h");
        } else {
            printf("\033[?12l");
        }
        
        if (strcmp(glob_config->cursor_style, "block") == 0) {
            printf("\033[?6c");
        } else if (strcmp(glob_config->cursor_style, "underline") == 0) {
            printf("\033[?2c");
        } else if (strcmp(glob_config->cursor_style, "bar") == 0) {
            printf("\033[?4c");
        } else if (strcmp(glob_config->cursor_style, "custom") == 0) {
            char processed[128] = {0};
            char* src = glob_config->cursor_custom_sequence;
            char* dst = processed;
            
            while (*src) {
                if (*src == '\\' && *(src+1) == '0' && *(src+2) == '3' && *(src+3) == '3') {
                    *dst++ = '\033';
                    src += 4;
                } else {
                    *dst++ = *src++;
                }
            }
            
            printf("%s", processed);
        }
        
        if (glob_config->cursor_blink_enabled && glob_config->cursor_blink_rate > 0) {
            int interval_deciseconds = glob_config->cursor_blink_rate / 100;
            if (interval_deciseconds < 1) interval_deciseconds = 1;
            printf("\033[?13;%d]", interval_deciseconds);
        }
    } else {
        if (strcmp(glob_config->cursor_style, "block") == 0) {
            printf("\033[2 q");
        } else if (strcmp(glob_config->cursor_style, "underline") == 0) {
            printf("\033[4 q");
        } else if (strcmp(glob_config->cursor_style, "bar") == 0) {
            printf("\033[6 q");
        } else if (strcmp(glob_config->cursor_style, "custom") == 0) {
            char processed[128] = {0};
            char* src = glob_config->cursor_custom_sequence;
            char* dst = processed;
            
            while (*src) {
                if (*src == '\\' && *(src+1) == '0' && *(src+2) == '3' && *(src+3) == '3') {
                    *dst++ = '\033';
                    src += 4;
                } else {
                    *dst++ = *src++;
                }
            }
            
            printf("%s", processed);
        }
        
        printf("\033[?12%c", glob_config->cursor_blink_enabled ? 'h' : 'l');
        
        if (glob_config->cursor_blink_enabled && glob_config->cursor_blink_rate > 0) {
            printf("\033]12;%d\007", glob_config->cursor_blink_rate);
        }
    }
    
    fflush(stdout);
}