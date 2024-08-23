#pragma once
#include <stdbool.h>

bool aesd_initialize( char const* filename, bool is_daemon );

void aesd_run();

void aesd_shutdown();

void aesd_signal_triggered( int sig );