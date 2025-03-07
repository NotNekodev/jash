#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct config {
    /* Core */
    char *prompt;
    uint64_t history_size;
    char* history_file;
    char* default_directory;
    bool debug;

    /* Cursor */
    char cursor_style[64];
    bool cursor_blink_enabled;
    int cursor_blink_rate;
    char cursor_custom_sequence[64];
} config_t;

extern config_t * glob_config;

void config_init();

void apply_cursor_style();

#endif // __CONFIG_H__