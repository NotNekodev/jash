#ifndef __EXECUTE_H__
#define __EXECUTE_H__

#include <stddef.h>

#define MAX_PATH_LENGTH 2048

char *get_exec_path(char *cmd);
int exec_cmd(char *cmd, char *const argv[]);

#endif // __EXECUTE_H__