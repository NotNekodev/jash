#ifndef __SIGNAL_H__
#define __SIGNAL_H__

#include <signal.h>

extern volatile sig_atomic_t command_running;

void sigint_handler(int sig);

void init_signals();

#endif // __SIGNAL_H__