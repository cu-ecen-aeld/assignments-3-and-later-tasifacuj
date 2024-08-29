#pragma once
#include <stdbool.h>

bool aesd_thrd_initialize( char const* filename, bool is_daemon );

void aesd_thrd_run();

void aesd_thrd_shutdown();

void aesd_thrd_signal_triggered( int sig );