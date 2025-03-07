#include "core/config.h"
#include <core/cmd/echo.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

void expand_env_vars(const char *input, char *output, size_t output_size) {
    const char *p = input;
    char *out = output;
    size_t remaining = output_size - 1; /* Leave room for null terminator */
    
    while (*p && remaining > 0) {
        if (*p == '$' && *(p + 1)) {
            /* Check if it's ${VAR} format */
            if (*(p + 1) == '{') {
                const char *end = strchr(p + 2, '}');
                if (end) {
                    size_t var_len = end - (p + 2);
                    char var_name[256];
                    
                    if (var_len < sizeof(var_name)) {
                        strncpy(var_name, p + 2, var_len);
                        var_name[var_len] = '\0';
                        
                        const char *value = getenv(var_name);
                        if (value) {
                            size_t val_len = strlen(value);
                            if (val_len <= remaining) {
                                strcpy(out, value);
                                out += val_len;
                                remaining -= val_len;
                            }
                        }
                        p = end + 1; /* Skip past the closing } */
                        continue;
                    }
                }
            } 
            /* Check if it's $VAR format */
            else if (isalpha(*(p + 1)) || *(p + 1) == '_') {
                const char *var_start = p + 1;
                const char *var_end = var_start;
                
                /* Find the end of the variable name */
                while (*var_end && (isalnum(*var_end) || *var_end == '_')) {
                    var_end++;
                }
                
                size_t var_len = var_end - var_start;
                char var_name[256];
                
                if (var_len < sizeof(var_name)) {
                    strncpy(var_name, var_start, var_len);
                    var_name[var_len] = '\0';
                    
                    const char *value = getenv(var_name);
                    if (value) {
                        size_t val_len = strlen(value);
                        if (val_len <= remaining) {
                            strcpy(out, value);
                            out += val_len;
                            remaining -= val_len;
                        }
                    }
                    p = var_end;
                    continue;
                }
            }
        }
        
        /* Just copy the character if it's not part of an environment variable */
        *out++ = *p++;
        remaining--;
    }
    
    *out = '\0'; /* Null terminate the output string */
}

int exec_echo(int argc, char *argv[]) {
    int i;
    int no_newline = 0;
    int interpret_escapes = 0;
    
    int process_options = 1;

    char *posixly_correct = getenv("POSIXLY_CORRECT");
    
    int xpg_echo = glob_config->xpg_echo;
    
    if (xpg_echo || posixly_correct != NULL) {
        interpret_escapes = 1;
        process_options = 0;
    }
    
    if (process_options) {
        for (i = 1; i < argc; i++) {
            if (argv[i][0] == '-') {
                if (strcmp(argv[i], "-n") == 0) {
                    no_newline = 1;
                } else if (strcmp(argv[i], "-e") == 0) {
                    interpret_escapes = 1;
                } else if (strcmp(argv[i], "-E") == 0) {
                    interpret_escapes = 0;
                } else if (strcmp(argv[i], "--help") == 0) {
                    printf("Usage: echo [-neE] [arg ...]\n");
                    printf("Write arguments to standard output.\n\n");
                    printf("  -n    do not output the trailing newline\n");
                    printf("  -e    enable interpretation of backslash escapes\n");
                    printf("  -E    disable interpretation of backslash escapes (default)\n");
                    return 0;
                } else {
                    break;
                }
            } else {
                break;
            }
        }
        
        argc -= (i - 1);
        argv += (i - 1);
    }
    
    for (i = 1; i < argc; i++) {
        if (i > 1) {
            putchar(' ');
        }
        
        char expanded[4096];
        expand_env_vars(argv[i], expanded, sizeof(expanded));
        
        if (interpret_escapes) {
            char *p = expanded;
            while (*p) {
                if (*p == '\\' && *(p + 1)) {
                    p++;
                    switch (*p) {
                        case 'a': putchar('\a'); break;  /* Alert */
                        case 'b': putchar('\b'); break;  /* Backspace */
                        case 'c': no_newline = 1; goto end_loop;  /* Suppress trailing newline */
                        case 'e': putchar('\033'); break;  /* Escape */
                        case 'f': putchar('\f'); break;  /* Form feed */
                        case 'n': putchar('\n'); break;  /* Newline */
                        case 'r': putchar('\r'); break;  /* Carriage return */
                        case 't': putchar('\t'); break;  /* Tab */
                        case 'v': putchar('\v'); break;  /* Vertical tab */
                        case '\\': putchar('\\'); break;  /* Backslash */
                        case '0':  /* Octal value */
                            {
                                char oct[4] = {0};
                                int j = 0;
                                
                                /* Get up to 3 octal digits */
                                while (*(p + 1) >= '0' && *(p + 1) <= '7' && j < 3) {
                                    oct[j++] = *(++p);
                                }
                                
                                putchar(strtol(oct, NULL, 8));
                            }
                            break;
                        case 'x':  /* Hex value */
                            {
                                if (*(p + 1) && *(p + 2) &&
                                    (isxdigit(*(p + 1)) && isxdigit(*(p + 2)))) {
                                    char hex[3] = {*(p + 1), *(p + 2), 0};
                                    putchar(strtol(hex, NULL, 16));
                                    p += 2;
                                } else {
                                    putchar('x');  /* Not a valid hex sequence */
                                }
                            }
                            break;
                        default: putchar(*p); break;  /* Just print the character */
                    }
                } else {
                    putchar(*p);
                }
                p++;
            }
        end_loop:
            ;
        } else {
            printf("%s", expanded);
        }
    }
    
    if (!no_newline) {
        putchar('\n');
    }
    
    return 0;
}