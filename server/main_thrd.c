#include <signal.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "aesd_server_thrd.h"
#include "aesdsocket_cfg.h"


void signal_handler( int signo );

int main( int argc, char** argv ){
    bool is_daemon = false;

    if( argc > 1 && 0 == strcmp( "-d", argv[ 1 ] ) ){
        is_daemon = true;
    }

    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;

    if (-1 == sigaction(SIGINT, &sa, NULL)) {
        syslog( LOG_ERR, "failed to install signal handler for SIGINT" );
        return -1;
    }
    
    if ( -1 == sigaction( SIGTERM, &sa, NULL ) ) {
        syslog( LOG_ERR, "failed to install signal handler for SIGUSR1" );
        return 1;
    }
    

    if( !aesd_thrd_initialize( LOG_PATH, is_daemon ) ){
        syslog( LOG_ERR, "Failed to initialize server" );
        return -1;
    }

    aesd_thrd_run();
    aesd_thrd_shutdown();
    return 0;
}

void signal_handler( int signo ){
    if( signo == SIGINT || signo == SIGTERM ){
        syslog( LOG_DEBUG, "Caught signal, exiting" );
        aesd_thrd_signal_triggered( signo );
    }
}
