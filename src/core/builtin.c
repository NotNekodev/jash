#include <core/builtin.h>
#include <string.h>
#include <stdlib.h>
#include <core/data.h>
#include <core/completion.h>
#include <core/cmd/echo.h>

void init_builtin() {
    char *builtins[] = {"cd", "set", "unset", "echo", "exit"};
    register_builtin_commands(builtins, 5);
}

int builtin_handler(int argc, char **argv) {
    if (strcmp(argv[0], "cd") == 0) {
        change_cwd(argc > 1 ? argv[1] : NULL);
        return 0;
    } else if (strcmp(argv[0], "set") == 0) {
        char* key = argc > 1 ? argv[1] : NULL;
        char* value = argc > 2 ? argv[2] : NULL;
        if (key == NULL) {
            fprintf(stderr, "set: missing key\n");
            return -1;
        }
        if (value == NULL) {
            fprintf(stderr, "set: missing value\n");
            return -1;
        }
        setenv(key, value, 1);
        return 0;
    } else if (strcmp(argv[0], "unset") == 0) {
        char* key = argc > 1 ? argv[1] : NULL;
        if (key == NULL) {
            fprintf(stderr, "set: missing key\n");
            return -1;
        }
        unsetenv(key);
        return 0;
    } else if (strcmp(argv[0], "echo") == 0) {
        int i = exec_echo(argc, argv);
        return i;
    } else {
        return -2;
    }
}