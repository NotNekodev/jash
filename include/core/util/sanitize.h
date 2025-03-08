#ifndef __UTIL_SANITIZE_H__
#define __UTIL_SANITIZE_H__

#include <stddef.h>

char* sanitize_path_arg(const char* arg);
char* trim_trailing_whitespace(char *str);
void sanitize_command_args(char** argv);

void tilde_to_complete_path(const char* path, char* expanded_path, size_t size);


#endif // __UTIL_SANITIZE_H__