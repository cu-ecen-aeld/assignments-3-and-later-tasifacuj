#pragma once
#include <stdbool.h>

bool aesd_initialize( char const* filename );

void aesd_run();

void aesd_shutdown();

void aesd_signal_triggered( int sig );