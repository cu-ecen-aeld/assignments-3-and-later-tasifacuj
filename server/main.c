#include <signal.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "aesdsocket.h"
#include "aesdsocket_cfg.h"


void signal_handler( int signo );

static void skeleton_daemon();

int main( int argc, char** argv ){
    bool is_daemon = false;

    if( argc > 1 && 0 == strcmp( "-d", argv[ 1 ] ) ){
        is_daemon = true;
    }

    if( is_daemon ){
        skeleton_daemon();
    }else{
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
    }

    if( !aesd_initialize( LOG_PATH ) ){
        syslog( LOG_ERR, "Failed to initialize server" );
        return -1;
    }
    
    aesd_run();
    aesd_shutdown();
    return 0;
}

void signal_handler( int signo ){
    if( signo == SIGINT || signo == SIGTERM ){
        syslog( LOG_DEBUG, "Caught signal, exiting" );
        aesd_signal_triggered( signo );
    }
}

static void skeleton_daemon()
{
    pid_t pid;
    
    /* Fork off the parent process */
    pid = fork();
    
    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);
    
     /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);
    
    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);
    
    /* Catch, ignore and handle signals */
    /*TODO: Implement a working signal handler */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;

    if (-1 == sigaction(SIGINT, &sa, NULL)) {
        syslog( LOG_ERR, "failed to install signal handler for SIGINT" );
        exit(EXIT_FAILURE);
    }
    
    if ( -1 == sigaction( SIGTERM, &sa, NULL ) ) {
        syslog( LOG_ERR, "failed to install signal handler for SIGUSR1" );
        exit(EXIT_FAILURE);
    }
    
    /* Fork off for the second time*/
    pid = fork();
    
    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);
    
    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);
    
    /* Set new file permissions */
    umask(0);
    
    /* Change the working directory to the root directory */
    /* or another appropriated directory */
    chdir("/");
    
    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }
    
    // /* Open the log file */
    // openlog ("firstdaemon", LOG_PID, LOG_DAEMON);
}