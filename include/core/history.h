#ifndef __HISTORY_H__
#define __HISTORY_H__

#include <stddef.h>

#define MAX_HISTORY_LEN 1024

void history_init();
void history_add(const char *cmd);
void history_cleanup();

#endif // __HISTORY_H__