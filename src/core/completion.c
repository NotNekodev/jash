/*
This is really some wizardry but its really fast (put jeremy clarkson meme here) so im gonna keep it that way.
*/

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
#include <core/config.h>

#define PATH_BUFFER_SIZE 4096
#define HASH_SIZE 8192
#define EXEC_CACHE_SIZE 1024
#define MAX_MATCHES_INIT 128

static char **command_completion(const char *text, int start, int end);
static char *command_generator(const char *text, int state);
static char *filename_generator(const char *text, int state);

// Trie node structure
typedef struct TrieNode {
    struct TrieNode *children[128];  // ASCII character set
    char *command;                   // NULL if not a complete command
    time_t timestamp;                // When this node was last updated
} TrieNode;

static TrieNode *command_trie = NULL;
static pthread_mutex_t trie_mutex = PTHREAD_MUTEX_INITIALIZER;
static char **builtin_commands = NULL;
static int num_builtins = 0;
static int is_trie_initialized = 0;
static time_t last_path_scan = 0;
static int command_matches_found = 0;

typedef struct {
    char *path;
    int is_executable;
    time_t last_checked;
} exec_cache_entry_t;

static exec_cache_entry_t exec_cache[EXEC_CACHE_SIZE];
static unsigned int exec_cache_clock = 0;

// Hash map for quick command lookup
typedef struct {
    char *command;
    time_t timestamp;
} HashEntry;

static HashEntry hash_table[HASH_SIZE];

// Function prototypes for trie operations
static TrieNode* create_trie_node();
static void insert_into_trie(TrieNode *root, const char *command);
static void collect_trie_matches(TrieNode *node, const char *prefix, char ***matches, int *match_count, int *max_matches);
static void free_trie_node(TrieNode *node);
static int search_trie_commands(const char *prefix, char ***matches);

// Function prototypes for path scanning
static void lazy_scan_path();
static void* scan_directory_thread(void *arg);

static inline unsigned int hash_string(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash % HASH_SIZE;
}

static inline int check_hash_cache(const char *str) {
    unsigned int hash = hash_string(str);
    if (hash_table[hash].command && strcmp(hash_table[hash].command, str) == 0) {
        time_t now = time(NULL);
        if (now - hash_table[hash].timestamp < glob_config->cache_ttl) {
            return 1;
        }
    }
    return 0;
}

static inline void add_to_hash_cache(const char *str) {
    unsigned int hash = hash_string(str);
    
    if (hash_table[hash].command) {
        free(hash_table[hash].command);
    }
    
    hash_table[hash].command = strdup(str);
    hash_table[hash].timestamp = time(NULL);
}

static void clear_hash_cache() {
    for (int i = 0; i < HASH_SIZE; i++) {
        if (hash_table[i].command) {
            free(hash_table[i].command);
            hash_table[i].command = NULL;
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

// Trie implementation
static TrieNode* create_trie_node() {
    TrieNode *node = (TrieNode*)malloc(sizeof(TrieNode));
    if (node) {
        memset(node->children, 0, sizeof(node->children));
        node->command = NULL;
        node->timestamp = time(NULL);
    }
    return node;
}

static void insert_into_trie(TrieNode *root, const char *command) {
    if (!root || !command) return;
    
    TrieNode *current = root;
    const unsigned char *p = (const unsigned char *)command;
    
    while (*p) {
        if (!current->children[*p]) {
            current->children[*p] = create_trie_node();
        }
        current = current->children[*p];
        p++;
    }
    
    // Mark end of command
    if (!current->command) {
        current->command = strdup(command);
    }
    current->timestamp = time(NULL);
}

static void collect_trie_matches(TrieNode *node, const char *prefix, char ***matches, int *match_count, int *max_matches) {
    if (!node) return;
    
    // If this node represents a complete command, add it
    if (node->command) {
        // Check if we need to expand our matches array
        if (*match_count >= *max_matches) {
            *max_matches *= 2;
            *matches = realloc(*matches, *max_matches * sizeof(char*));
        }
        
        (*matches)[(*match_count)++] = strdup(node->command);
    }
    
    // Recursively check all possible children
    for (int i = 0; i < 128; i++) {
        if (node->children[i]) {
            collect_trie_matches(node->children[i], prefix, matches, match_count, max_matches);
        }
    }
}

static int search_trie_commands(const char *prefix, char ***matches) {
    if (!command_trie || !prefix) return 0;
    
    TrieNode *current = command_trie;
    const unsigned char *p = (const unsigned char *)prefix;
    
    // Navigate to the node corresponding to the prefix
    while (*p && current) {
        current = current->children[*p];
        p++;
    }
    
    // If we couldn't find the prefix, return 0
    if (!current) return 0;
    
    // Initialize matches array
    int max_matches = MAX_MATCHES_INIT;
    *matches = malloc(max_matches * sizeof(char*));
    int match_count = 0;
    
    // Collect all commands that start with the given prefix
    collect_trie_matches(current, prefix, matches, &match_count, &max_matches);
    
    return match_count;
}

static void free_trie_node(TrieNode *node) {
    if (!node) return;
    
    // Recursively free all children
    for (int i = 0; i < 128; i++) {
        if (node->children[i]) {
            free_trie_node(node->children[i]);
        }
    }
    
    // Free the command if it exists
    if (node->command) {
        free(node->command);
    }
    
    free(node);
}

// Initialize completion
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
    
    // Initialize command_trie if not already done
    pthread_mutex_lock(&trie_mutex);
    if (!command_trie) {
        command_trie = create_trie_node();
        
        // Insert builtin commands into trie
        for (int i = 0; i < num_builtins; i++) {
            insert_into_trie(command_trie, builtin_commands[i]);
        }
    }
    pthread_mutex_unlock(&trie_mutex);
    
    memset(exec_cache, 0, sizeof(exec_cache));
    memset(hash_table, 0, sizeof(hash_table));
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
        if (now - exec_cache[idx].last_checked < glob_config->cache_ttl) return exec_cache[idx].is_executable;
        int result = is_executable_file_with_stat(path, NULL);
        exec_cache[idx].is_executable = result;
        exec_cache[idx].last_checked = now;
        return result;
    }
    
    unsigned int start_idx = idx;
    while (1) {
        if (!exec_cache[idx].path || now - exec_cache[idx].last_checked > glob_config->cache_ttl) {
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

static void* scan_directory_thread(void *arg) {
    char *dir = (char*)arg;
    DIR *dp = opendir(dir);
    if (!dp) {
        free(dir);
        return NULL;
    }
    
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
            pthread_mutex_lock(&trie_mutex);
            
            int is_exe = has_exe_extension(entry->d_name);
            char *name_to_store = strdup(entry->d_name);
            if (is_exe) name_to_store[strlen(name_to_store) - 4] = '\0';
            
            insert_into_trie(command_trie, name_to_store);
            add_to_hash_cache(name_to_store);
            free(name_to_store);
            
            if (is_exe) {
                char *name_with_exe = strdup(entry->d_name);
                insert_into_trie(command_trie, name_with_exe);
                add_to_hash_cache(name_with_exe);
                free(name_with_exe);
            }
            
            pthread_mutex_unlock(&trie_mutex);
        }
    }
    closedir(dp);
    free(dir);
    return NULL;
}

static void lazy_scan_path() {
    time_t now = time(NULL);
    
    // Don't scan more often than every 5 minutes
    if (now - last_path_scan < 300) return;
    
    last_path_scan = now;
    
    char *path_env = getenv("PATH");
    if (!path_env) return;
    
    char *path_copy = strdup(path_env);
    char *start = path_copy;
    char *end;
    
    int dir_count = 1;
    for (char *p = path_env; *p; p++) if (*p == ':') dir_count++;
    
    pthread_t *threads = malloc(dir_count * sizeof(pthread_t));
    int thread_count = 0;
    
    while (start && *start) {
        end = strchr(start, ':');
        if (end) *end = '\0';
        
        if (*start) {
            char *dir_copy = strdup(start);
            pthread_create(&threads[thread_count++], NULL, scan_directory_thread, dir_copy);
        }
        
        if (end) start = end + 1;
        else start = NULL;
    }
    
    // Don't wait for threads to complete - let them run in background
    for (int i = 0; i < thread_count; i++) {
        pthread_detach(threads[i]);
    }
    
    free(threads);
    free(path_copy);
}

void scan_path_for_commands() {
    // Reset the completion system first
    cleanup_completion();
    
    // Initialize new trie and scan immediately
    pthread_mutex_lock(&trie_mutex);
    command_trie = create_trie_node();
    
    // Add builtins to trie
    for (int i = 0; i < num_builtins; i++) {
        insert_into_trie(command_trie, builtin_commands[i]);
    }
    pthread_mutex_unlock(&trie_mutex);
    
    // Perform an immediate scan
    last_path_scan = 0;  // Force scan
    lazy_scan_path();
}

void cleanup_completion() {
    pthread_mutex_lock(&trie_mutex);
    if (command_trie) {
        free_trie_node(command_trie);
        command_trie = NULL;
    }
    pthread_mutex_unlock(&trie_mutex);
    
    clear_exec_cache();
    clear_hash_cache();
}

static char **command_completion(const char *text, int start, int end) {
    // Initialize the trie on first completion if needed
    if (!is_trie_initialized) {
        pthread_mutex_lock(&trie_mutex);
        if (!command_trie) {
            command_trie = create_trie_node();
            
            // Add builtins to trie
            for (int i = 0; i < num_builtins; i++) {
                insert_into_trie(command_trie, builtin_commands[i]);
            }
            is_trie_initialized = 1;
        }
        pthread_mutex_unlock(&trie_mutex);
    }
    
    
    rl_completion_append_character = '\0';

    // Trigger a lazy scan of PATH if needed
    lazy_scan_path();
    
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
    static char **matches = NULL;
    static int match_count = 0;
    static int current_match = 0;
    
    if (!state) {
        // Free previous matches if any
        if (matches) {
            for (int i = 0; i < match_count; i++) {
                free(matches[i]);
            }
            free(matches);
            matches = NULL;
        }
        
        // First try builtin commands (directly, not through trie)
        int builtin_matches = 0;
        int len = strlen(text);
        for (int i = 0; i < num_builtins; i++) {
            if (strncmp(builtin_commands[i], text, len) == 0) {
                builtin_matches++;
            }
        }
        
        // Search trie for matches
        match_count = search_trie_commands(text, &matches);
        
        // If we found at least one match, we'll return it
        if (match_count > 0) {
            command_matches_found = 1;
            current_match = 0;
        }
    }
    
    // Return the next match if available
    if (current_match < match_count) {
        return strdup(matches[current_match++]);
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