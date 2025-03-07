#ifndef __SHELL_DATA_H__
#define __SHELL_DATA_H__

#include <stddef.h>
#include <pwd.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_PROMPT_LENGTH 2048

typedef struct shell_data {
    char *cwd;
    char *home;
    char *user;
    char *hostname;

    char *prompt;
} shell_data_t;

extern shell_data_t *global_shell_data;

void init_shell_data();
void change_cwd(char *new_cwd);
void send_prompt();
void send_prompt_to_buffer(char *output); // New function

#endif // __SHELL_DATA_H__