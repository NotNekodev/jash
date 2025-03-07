#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>
#include <core/completion.h>

#define INITIAL_COMMAND_BUFFER 1024
#define PATH_BUFFER_SIZE 4096
#define HASH_SIZE 8192
#define EXEC_CACHE_SIZE 1024
#define MAX_MATCHES_INIT 128

static char **command_completion(const char *text, int start, int end);
static char *command_generator(const char *text, int state);
static char *filename_generator(const char *text, int state);

static char **path_commands = NULL;
static int num_path_commands = 0;
static int max_commands = 0;
static int command_matches_found = 0;
static pthread_mutex_t path_commands_mutex = PTHREAD_MUTEX_INITIALIZER;

static char **hash_table[HASH_SIZE] = {NULL};
static int hash_entries[HASH_SIZE] = {0};
static int hash_sizes[HASH_SIZE] = {0};

static char **builtin_commands = NULL;
static int num_builtins = 0;

typedef struct {
    char *path;
    int is_executable;
    time_t last_checked;
} exec_cache_entry_t;

static exec_cache_entry_t exec_cache[EXEC_CACHE_SIZE];
static unsigned int exec_cache_clock = 0;

static inline unsigned int hash_string(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash % HASH_SIZE;
}

static inline int string_exists_in_hash(const char *str) {
    unsigned int hash = hash_string(str);
    for (int i = 0; i < hash_entries[hash]; i++)
        if (strcmp(hash_table[hash][i], str) == 0) return 1;
    return 0;
}

static inline void add_string_to_hash(const char *str) {
    unsigned int hash = hash_string(str);
    
    if (!hash_table[hash]) {
        hash_sizes[hash] = 16;
        hash_table[hash] = malloc(hash_sizes[hash] * sizeof(char*));
        hash_entries[hash] = 0;
    }
    
    if (hash_entries[hash] >= hash_sizes[hash]) {
        hash_sizes[hash] *= 2;
        hash_table[hash] = realloc(hash_table[hash], hash_sizes[hash] * sizeof(char*));
    }
    
    hash_table[hash][hash_entries[hash]++] = strdup(str);
}

static void clear_hash_table() {
    for (int i = 0; i < HASH_SIZE; i++) {
        if (hash_table[i]) {
            for (int j = 0; j < hash_entries[i]; j++) free(hash_table[i][j]);
            free(hash_table[i]);
            hash_table[i] = NULL;
            hash_entries[i] = 0;
            hash_sizes[i] = 0;
        }
    }
}

static void clear_exec_cache() {
    for (int i = 0; i < EXEC_CACHE_SIZE; i++) {
        if (exec_cache[i].path) {
            free(exec_cache[i].path);
            exec_cache[i].path = NULL;
        }
    }
    exec_cache_clock = 0;
}

void register_builtin_commands(char **commands, int count) {
    if (builtin_commands) {
        for (int i = 0; i < num_builtins; i++) free(builtin_commands[i]);
        free(builtin_commands);
    }
    
    builtin_commands = malloc(count * sizeof(char*));
    num_builtins = count;
    
    for (int i = 0; i < count; i++) builtin_commands[i] = strdup(commands[i]);
}

void init_complete() {
    rl_attempted_completion_function = command_completion;
    rl_completion_append_character = '\0';
    memset(exec_cache, 0, sizeof(exec_cache));
    scan_path_for_commands();
}

static inline int has_exe_extension(const char *filename) {
    size_t len = strlen(filename);
    if (len < 4) return 0;
    return (strcasecmp(filename + len - 4, ".exe") == 0);
}

static inline int is_executable_file_with_stat(const char *path, const struct stat *st_ptr) {
    struct stat st_local;
    const struct stat *st = st_ptr;
    
    if (!st) {
        if (stat(path, &st_local) != 0) return 0;
        st = &st_local;
    }
    
    if (S_ISREG(st->st_mode)) return (st->st_mode & S_IXUSR) || has_exe_extension(path);
    return 0;
}

static inline int is_executable_file(const char *path) {
    if (!path) return 0;
    
    unsigned int hash = 0;
    const char *p = path;
    while (*p) hash = (hash * 31) + *p++;
    unsigned int idx = hash % EXEC_CACHE_SIZE;
    
    time_t now = time(NULL);
    
    if (exec_cache[idx].path && strcmp(exec_cache[idx].path, path) == 0) {
        if (now - exec_cache[idx].last_checked < 5) return exec_cache[idx].is_executable;
        int result = is_executable_file_with_stat(path, NULL);
        exec_cache[idx].is_executable = result;
        exec_cache[idx].last_checked = now;
        return result;
    }
    
    unsigned int start_idx = idx;
    while (1) {
        if (!exec_cache[idx].path || now - exec_cache[idx].last_checked > 300) {
            if (exec_cache[idx].path) free(exec_cache[idx].path);
            int result = is_executable_file_with_stat(path, NULL);
            exec_cache[idx].path = strdup(path);
            exec_cache[idx].is_executable = result;
            exec_cache[idx].last_checked = now;
            return result;
        }
        
        idx = (idx + 1) % EXEC_CACHE_SIZE;
        if (idx == start_idx) break;
    }
    
    int victim = exec_cache_clock++ % EXEC_CACHE_SIZE;
    if (exec_cache[victim].path) free(exec_cache[victim].path);
    int result = is_executable_file_with_stat(path, NULL);
    exec_cache[victim].path = strdup(path);
    exec_cache[victim].is_executable = result;
    exec_cache[victim].last_checked = now;
    return result;
}

static void* scan_directory(void *arg) {
    char *dir = (char*)arg;
    DIR *dp = opendir(dir);
    if (!dp) {
        free(dir);
        return NULL;
    }
    
    int local_max = 256;
    int local_count = 0;
    char **local_commands = malloc(local_max * sizeof(char*));
    
    char full_path[PATH_BUFFER_SIZE];
    int dir_len = strlen(dir);
    memcpy(full_path, dir, dir_len);
    full_path[dir_len] = '/';
    
    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_type == DT_DIR || entry->d_name[0] == '.') continue;
        
        strcpy(full_path + dir_len + 1, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == 0 && is_executable_file_with_stat(full_path, &st)) {
            if (local_count >= local_max - 1) {
                local_max *= 2;
                local_commands = realloc(local_commands, local_max * sizeof(char*));
            }
            
            int is_exe = has_exe_extension(entry->d_name);
            char *name_to_store = strdup(entry->d_name);
            if (is_exe) name_to_store[strlen(name_to_store) - 4] = '\0';
            
            int found_local = 0;
            for (int i = 0; i < local_count; i++) {
                if (strcmp(local_commands[i], name_to_store) == 0) {
                    found_local = 1;
                    free(name_to_store);
                    break;
                }
            }
            
            if (!found_local) {
                local_commands[local_count++] = name_to_store;
                
                if (is_exe) {
                    char *name_with_exe = strdup(entry->d_name);
                    found_local = 0;
                    for (int i = 0; i < local_count - 1; i++) {
                        if (strcmp(local_commands[i], name_with_exe) == 0) {
                            found_local = 1;
                            free(name_with_exe);
                            break;
                        }
                    }
                    
                    if (!found_local) local_commands[local_count++] = name_with_exe;
                }
            }
        }
    }
    closedir(dp);
    
    if (local_count > 0) {
        pthread_mutex_lock(&path_commands_mutex);
        
        if (num_path_commands + local_count >= max_commands) {
            max_commands = num_path_commands + local_count + 256;
            path_commands = realloc(path_commands, max_commands * sizeof(char*));
        }
        
        for (int i = 0; i < local_count; i++) {
            if (!string_exists_in_hash(local_commands[i])) {
                path_commands[num_path_commands++] = local_commands[i];
                add_string_to_hash(local_commands[i]);
            } else {
                free(local_commands[i]);
            }
        }
        
        pthread_mutex_unlock(&path_commands_mutex);
    } else {
        for (int i = 0; i < local_count; i++) free(local_commands[i]);
    }
    
    free(local_commands);
    free(dir);
    return NULL;
}

void scan_path_for_commands() {
    char *path_env = getenv("PATH");
    if (!path_env) return;
    
    cleanup_completion();
    clear_hash_table();
    clear_exec_cache();
    
    max_commands = INITIAL_COMMAND_BUFFER;
    path_commands = malloc(max_commands * sizeof(char*));
    num_path_commands = 0;
    
    int dir_count = 1;
    for (char *p = path_env; *p; p++) if (*p == ':') dir_count++;
    
    pthread_t *threads = malloc(dir_count * sizeof(pthread_t));
    int thread_count = 0;
    
    char *path_copy = strdup(path_env);
    char *start = path_copy;
    char *end;
    
    while (start && *start) {
        end = strchr(start, ':');
        if (end) *end = '\0';
        
        if (*start) {
            char *dir_copy = strdup(start);
            pthread_create(&threads[thread_count++], NULL, scan_directory, dir_copy);
        }
        
        if (end) start = end + 1;
        else start = NULL;
    }
    
    for (int i = 0; i < thread_count; i++) pthread_join(threads[i], NULL);
    
    free(threads);
    free(path_copy);
}

void cleanup_completion() {
    pthread_mutex_lock(&path_commands_mutex);
    if (path_commands) {
        for (int i = 0; i < num_path_commands; i++) free(path_commands[i]);
        free(path_commands);
        path_commands = NULL;
        num_path_commands = 0;
        max_commands = 0;
    }
    pthread_mutex_unlock(&path_commands_mutex);
    clear_exec_cache();
}

static char **command_completion(const char *text, int start, int end) {
    if (rl_line_buffer[0] == '\0' || (start == 0 && text[0] == '\0')) {
        rl_attempted_completion_over = 1;
        return NULL;
    }
    
    if (start == 0) {
        command_matches_found = 0;
        char **matches = rl_completion_matches(text, command_generator);
        
        if (!matches || command_matches_found == 0) return rl_completion_matches(text, filename_generator);
        return matches;
    } else {
        return rl_completion_matches(text, filename_generator);
    }
}

static char *command_generator(const char *text, int state) {
    static int list_index, len;
    static int checking_builtins = 1;
    
    if (!state) {
        list_index = 0;
        len = strlen(text);
        checking_builtins = 1;
    }

    if (checking_builtins) {
        while (list_index < num_builtins) {
            char *name = builtin_commands[list_index++];
            
            if (strncmp(name, text, len) == 0) {
                command_matches_found = 1;
                return strdup(name);
            }
        }
        
        checking_builtins = 0;
        list_index = 0;
    }
    
    while (list_index < num_path_commands) {
        char *name = path_commands[list_index++];
        
        if (strncmp(name, text, len) == 0) {
            command_matches_found = 1;
            return strdup(name);
        }
    }
    
    return NULL;
}

static char *filename_generator(const char *text, int state) {
    static int list_index;
    static char **matches = NULL;
    static int match_count = 0;
    static char *last_text = NULL;
    static int max_matches = 0;
    
    if (state == 0) {
        // Check if we need to gather new matches
        if (!last_text || strcmp(text, last_text) != 0) {
            // Clean up previous state
            if (last_text) {
                free(last_text);
                last_text = NULL;
            }
            
            if (matches) {
                for (int i = 0; i < match_count; i++) free(matches[i]);
                free(matches);
                matches = NULL;
            }
            
            // Save new text and reset state
            last_text = strdup(text);
            list_index = 0;
            match_count = 0;
            
            // Determine if we're processing the first argument
            int is_first_arg = 1;
            for (int i = 0; rl_line_buffer[i] && i < rl_point; i++) {
                if (isspace((unsigned char)rl_line_buffer[i])) {
                    is_first_arg = 0;
                    break;
                }
            }
            
            // Get current directory context
            char *dir_name = NULL;
            char *file_prefix = NULL;
            char *last_slash = NULL;
            
            if (*text == '\0') {
                dir_name = strdup(".");
                file_prefix = strdup("");
            } else {
                last_slash = strrchr(text, '/');
                if (last_slash) {
                    // Safely extract directory name without modifying original text
                    int dir_len = last_slash - text;
                    dir_name = malloc(dir_len + 1);
                    if (dir_len > 0) {
                        strncpy(dir_name, text, dir_len);
                        dir_name[dir_len] = '\0';
                    } else {
                        // Handle root directory case
                        dir_name[0] = '\0';
                    }
                    
                    // Handle empty directory case
                    if (*dir_name == '\0') {
                        free(dir_name);
                        dir_name = strdup("/");
                    }
                    
                    file_prefix = strdup(last_slash + 1);
                } else {
                    dir_name = strdup(".");
                    file_prefix = strdup(text);
                }
            }
            
            // Prepare result buffer
            max_matches = MAX_MATCHES_INIT;
            matches = malloc(max_matches * sizeof(char *));
            
            // Open directory
            DIR *dp = opendir(dir_name);
            if (dp) {
                struct dirent *entry;
                int prefix_len = strlen(file_prefix);
                
                // Scan directory entries
                while ((entry = readdir(dp)) != NULL) {
                    // Skip hidden files unless prefix starts with dot
                    if (entry->d_name[0] == '.' && file_prefix[0] != '.' &&
                        !(file_prefix[0] == '\\' && file_prefix[1] == '.'))
                        continue;
                    
                    // Check if name starts with our prefix
                    if (strncmp(entry->d_name, file_prefix, prefix_len) == 0) {
                        // Construct full path
                        char path_buf[PATH_BUFFER_SIZE];
                        struct stat st;
                        
                        if (strcmp(dir_name, "/") == 0)
                            snprintf(path_buf, PATH_BUFFER_SIZE, "/%s", entry->d_name);
                        else if (strcmp(dir_name, ".") == 0)
                            snprintf(path_buf, PATH_BUFFER_SIZE, "%s", entry->d_name);
                        else
                            snprintf(path_buf, PATH_BUFFER_SIZE, "%s/%s", dir_name, entry->d_name);
                        
                        // Check if file exists and get its type
                        if (stat(path_buf, &st) == 0) {
                            // Expand buffer if needed
                            if (match_count >= max_matches) {
                                max_matches *= 2;
                                matches = realloc(matches, max_matches * sizeof(char *));
                            }
                            
                            // Construct match string with proper prefixing
                            char *match_str;
                            if (*text == '\0' || last_slash == NULL) {
                                match_str = strdup(entry->d_name);
                            } else {
                                // Get the directory part from the original text
                                int dir_part_len = last_slash - text;
                                int match_len = dir_part_len + 1 + strlen(entry->d_name) + 1;
                                match_str = malloc(match_len);
                                
                                // Build the final string safely without modifying text
                                strncpy(match_str, text, dir_part_len);
                                match_str[dir_part_len] = '/';
                                strcpy(match_str + dir_part_len + 1, entry->d_name);
                            }
                            
                            // Add directory indicator
                            if (S_ISDIR(st.st_mode)) {
                                int len = strlen(match_str);
                                char *temp = realloc(match_str, len + 2);
                                if (temp) {
                                    match_str = temp;
                                    match_str[len] = '/';
                                    match_str[len + 1] = '\0';
                                }
                            }
                            
                            matches[match_count++] = match_str;
                        }
                    }
                }
                closedir(dp);
            }
            
            free(dir_name);
            free(file_prefix);
        }
    }
    
    // Return next match
    if (list_index < match_count) {
        return strdup(matches[list_index++]);
    }
    
    // No more matches
    return NULL;
}