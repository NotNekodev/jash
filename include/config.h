#ifndef __CONFIG_H__
#define __CONFIG_H__

typedef struct config {
    char *prompt;
} config_t;

extern config_t * glob_config;

void config_init();

#endif // __CONFIG_H__