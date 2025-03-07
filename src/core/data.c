#include <core/data.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

shell_data_t *global_shell_data = NULL;

void init_shell_data() {
    global_shell_data = malloc(sizeof(shell_data_t));
    
    uid_t uid = getuid();
    char hostname[HOST_NAME_MAX+1];
    gethostname(hostname, HOST_NAME_MAX+1);

    struct passwd *pw = getpwuid(uid);
    global_shell_data->user = pw->pw_name;
    global_shell_data->hostname = strdup(hostname);
    global_shell_data->home = strdup(pw->pw_dir);
    global_shell_data->cwd = strdup(pw->pw_dir);
    global_shell_data->prompt = "$$USER$$@$$HOST$$:$$DIR$$$ ";
}

void change_cwd(char *new_cwd) {
    if (new_cwd != NULL && strcmp(new_cwd, "~") == 0) {
        new_cwd = global_shell_data->home;
    }

    if (new_cwd == NULL) {
        if(chdir(getenv("HOME")) != 0) {
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


    free(global_shell_data->cwd);
    global_shell_data->cwd = strdup(getenv("PWD")); // should not use ".." but the actual path
}

void send_prompt() {
    char output[MAX_PROMPT_LENGTH];

    const char* placeholders[] = {"$$USER$$", "$$HOST$$", "$$DIR$$"};
    const char* replacements[] = {global_shell_data->user, global_shell_data->hostname, global_shell_data->cwd};

    char buffer[MAX_PROMPT_LENGTH];
    strncpy(buffer, global_shell_data->prompt, MAX_PROMPT_LENGTH);

    for (int i = 0; i < 3; i++) {
        char *pos;
        while((pos = strstr(buffer, placeholders[i])) != NULL) {
            char temp[MAX_PROMPT_LENGTH];

            // check if cwd is home directory and abbreviate with a tilde
            if (i == 2 && strcmp(replacements[i], global_shell_data->home) == 0) {
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
    const char* replacements[] = {global_shell_data->user, global_shell_data->hostname, global_shell_data->cwd};

    char buffer[MAX_PROMPT_LENGTH];
    strncpy(buffer, global_shell_data->prompt, MAX_PROMPT_LENGTH);

    for (int i = 0; i < 3; i++) {
        char *pos;
        while((pos = strstr(buffer, placeholders[i])) != NULL) {
            char temp[MAX_PROMPT_LENGTH];

            // check if cwd is home directory and abbreviate with a tilde
            if (i == 2 && strcmp(replacements[i], global_shell_data->home) == 0) {
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