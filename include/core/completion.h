#ifndef __COMPLETION_H__
#define __COMPLETION_H__

void init_complete();
void scan_path_for_commands();
void cleanup_completion();

void register_builtin_commands(char **commands, int count);


#endif // __COMPLETION_H__