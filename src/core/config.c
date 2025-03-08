#include <core/config.h>

#include <readline/tilde.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#include <lib/inih.h>
#include <core/util/sanitize.h>

config_t *glob_config = NULL;

static int handler(void* user, const char* section, const char* name, const char* value) {
    config_t *config = (config_t *)user;

    if (strcmp(section, "Core") == 0) {
        if (strcmp(name, "Prompt") == 0) {
            config->prompt = strdup(value);
        } else if (strcmp(name, "HistorySize") == 0) {
            config->history_size = atoi(value);
        } else if (strcmp(name, "HistoryFile") == 0) {
            char buffer[1024];
            tilde_to_complete_path(value, buffer, sizeof(buffer));
            config->history_file = strdup(buffer);
        } else if (strcmp(name, "DefaultDirectory") == 0) {
            config->default_directory = strdup(value);
        }  else if (strcmp(name, "MaxCommandSize") == 0) {
            config->max_command_size = atoi(value);
        }
    } else if (strcmp(section, "Cursor") == 0) {
        if (strcmp(name, "Style") == 0) {
            strncpy(config->cursor_style, value, sizeof(config->cursor_style) - 1);
        } else if (strcmp(name, "BlinkEnabled") == 0) {
            config->cursor_blink_enabled = strcmp(value, "true") == 0 ? 1 : 0;
        }  else if (strcmp(name, "CustomSequence") == 0) {
            strncpy(config->cursor_custom_sequence, value, sizeof(config->cursor_custom_sequence) - 1);
        }
    } else if (strcmp(section, "Completion") == 0) {
        if (strcmp(name, "Enable") == 0) {
            config->completion_enable = strcmp(value, "true") == 0 ? 1 : 0;
        } else if (strcmp(name, "CacheTTL") == 0) {
            config->cache_ttl = atoi(value);
        }
    } else if (strcmp(section, "Commands") == 0) {
        if (strcmp(name, "XpgEcho") == 0) {
            config->xpg_echo = strcmp(value, "true") == 0 ? 1 : 0;
        } else if (strcmp(name, "UseBuiltinEcho") == 0) {
            config->use_builtin_echo = strcmp(value, "true") == 0 ? 1 : 0;
        }
    }

    return 1;
}

void config_init() {
    glob_config = malloc(sizeof(config_t));
    
    // Initialize shell data part (previously in init_shell_data)
    uid_t uid = getuid();
    char hostname[HOST_NAME_MAX+1];
    gethostname(hostname, HOST_NAME_MAX+1);

    struct passwd *pw = getpwuid(uid);
    glob_config->user = strdup(pw->pw_name);
    glob_config->hostname = strdup(hostname);
    glob_config->home = strdup(pw->pw_dir);
    glob_config->cwd = strdup(pw->pw_dir);
    
    char path_to_conf[1024];
    snprintf(path_to_conf, 1024, "%s/.jashconf.ini", glob_config->home);
    
    if (ini_parse(path_to_conf, handler, glob_config) < 0) {
        printf("warning: can't load config file %s\n", path_to_conf);
        glob_config->prompt = strdup("$$USER$$@$$HOST$$:$$DIR$$$");
        glob_config->history_size = 2048;

        char buffer[1024];
        tilde_to_complete_path("~/.jash_history", buffer, sizeof(buffer));
        glob_config->history_file = strdup(buffer);

        glob_config->default_directory = strdup("~");
        glob_config->max_command_size = 1024;
        strncpy(glob_config->cursor_style, "block", sizeof(glob_config->cursor_style) - 1);
        glob_config->cursor_blink_enabled = 1;
        strncpy(glob_config->cursor_custom_sequence, "\\033[2 q", sizeof(glob_config->cursor_custom_sequence) - 1);
        glob_config->completion_enable = 1;
        glob_config->cache_ttl = 3600;
        glob_config->xpg_echo = 0;
        glob_config->use_builtin_echo = 1;
    }
}

void config_reload() {
    char path_to_conf[1024];
    snprintf(path_to_conf, 1024, "%s/.jashconf.ini", glob_config->home);

    if (!glob_config) {
        config_init();
        return;
    }

    char* path = getenv("PATH");
    glob_config->path_str = strdup(path);

    free(glob_config->prompt);
    free(glob_config->history_file);
    free(glob_config->default_directory);

    if (ini_parse(path_to_conf, handler, glob_config) < 0) {
        glob_config->prompt = strdup("$$USER$$@$$HOST$$:$$DIR$$$");
        glob_config->history_size = 2048;

        char buffer[1024];
        tilde_to_complete_path("~/.jash_history", buffer, sizeof(buffer));
        glob_config->history_file = strdup(buffer);
        
        glob_config->default_directory = strdup("~");
        glob_config->max_command_size = 1024;
        strncpy(glob_config->cursor_style, "block", sizeof(glob_config->cursor_style) - 1);
        glob_config->cursor_blink_enabled = 1;
        strncpy(glob_config->cursor_custom_sequence, "\\033[2 q", sizeof(glob_config->cursor_custom_sequence) - 1);
        glob_config->completion_enable = 1;
        glob_config->cache_ttl = 3600;
        glob_config->xpg_echo = 0;
        glob_config->use_builtin_echo = 1;
    }

    apply_cursor_style();
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
        
        printf("\033[?12%c", glob_config->cursor_blink_enabled ? 'h' : 'l');
    }
    
    fflush(stdout);
}

void change_cwd(char *new_cwd) {
    if (new_cwd != NULL && strcmp(new_cwd, "~") == 0) {
        new_cwd = glob_config->home;
    }

    if (new_cwd == NULL) {
        if(chdir(glob_config->home) != 0) {
            perror("cd error");
            return;
        }
    } else {
        if(chdir(new_cwd) != 0) {
            perror("cd error");
            return;
        }
    }
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        setenv("PWD", cwd, 1);
    }

    free(glob_config->cwd);
    glob_config->cwd = strdup(getenv("PWD")); // should not use ".." but the actual path
}

void send_prompt() {
    char output[MAX_PROMPT_LENGTH];

    const char* placeholders[] = {"$$USER$$", "$$HOST$$", "$$DIR$$"};
    const char* replacements[] = {glob_config->user, glob_config->hostname, glob_config->cwd};

    char buffer[MAX_PROMPT_LENGTH];
    strncpy(buffer, glob_config->prompt, MAX_PROMPT_LENGTH);

    for (int i = 0; i < 3; i++) {
        char *pos;
        while((pos = strstr(buffer, placeholders[i])) != NULL) {
            char temp[MAX_PROMPT_LENGTH];

            // check if cwd is home directory and abbreviate with a tilde
            if (i == 2 && strcmp(replacements[i], glob_config->home) == 0) {
                replacements[i] = "~";
            }

            size_t prefix_len = pos - buffer;
            snprintf(temp, MAX_PROMPT_LENGTH, "%.*s%s%s", (int)prefix_len, buffer, replacements[i], pos + strlen(placeholders[i]));
            strncpy(buffer, temp, MAX_PROMPT_LENGTH - 1);
            buffer[MAX_PROMPT_LENGTH - 1] = '\0';
        }
    }

    strncpy(output, buffer, MAX_PROMPT_LENGTH - 1);
    output[MAX_PROMPT_LENGTH - 1] = '\0';

    printf("%s ", output);
}

void send_prompt_to_buffer(char *output) {
    const char* placeholders[] = {"$$USER$$", "$$HOST$$", "$$DIR$$"};
    const char* replacements[] = {glob_config->user, glob_config->hostname, glob_config->cwd};

    char buffer[MAX_PROMPT_LENGTH];
    strncpy(buffer, glob_config->prompt, MAX_PROMPT_LENGTH);

    for (int i = 0; i < 3; i++) {
        char *pos;
        while((pos = strstr(buffer, placeholders[i])) != NULL) {
            char temp[MAX_PROMPT_LENGTH];

            // check if cwd is home directory and abbreviate with a tilde
            if (i == 2 && strcmp(replacements[i], glob_config->home) == 0) {
                replacements[i] = "~";
            }

            size_t prefix_len = pos - buffer;
            snprintf(temp, MAX_PROMPT_LENGTH, "%.*s%s%s", (int)prefix_len, buffer, replacements[i], pos + strlen(placeholders[i]));
            strncpy(buffer, temp, MAX_PROMPT_LENGTH - 1);
            buffer[MAX_PROMPT_LENGTH - 1] = '\0';
        }
    }

    size_t buffer_len = strlen(buffer);
    if (buffer_len + 2 < MAX_PROMPT_LENGTH) {
        buffer[buffer_len] = ' ';
        buffer[buffer_len + 1] = '\0';
    }

    strncpy(output, buffer, MAX_PROMPT_LENGTH - 1);
    output[MAX_PROMPT_LENGTH - 1] = '\0';
}