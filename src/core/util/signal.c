#include <core/util/signal.h>

#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdlib.h>

volatile sig_atomic_t command_running = 0;

void sigint_handler(int sig) {
    if (!command_running) {
        printf("\n");
        fprintf(stdout, "\033[?25h");
        rl_on_new_line();
        rl_replace_line("", 0);
        rl_redisplay();
    }
}

void init_signals() {
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    rl_catch_signals = 0;
    rl_catch_sigwinch = 1;
    rl_set_signals();
}