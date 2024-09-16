#include "aesd_server_thrd.h"
#include "aesdsocket_cfg.h"
#include "slist/queue.h"

#include <string.h>
#include <sys/socket.h>	/* basic socket definitions */
#include <netinet/in.h>
#include <signal.h>
#include <syslog.h>

#include <errno.h>
#include <limits.h>		/* for OPEN_MAX */
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct ThreadData {
    pthread_mutex_t*        write_lock;
    int                     fd;
    volatile sig_atomic_t   is_completed;
    pthread_t               p_tid;
} ;

struct slist_data_s {
    struct ThreadData* td;
    SLIST_ENTRY( slist_data_s ) entries;
};

typedef struct slist_data_s slist_data_t;

struct t_eventData{
    int myData;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static char const*          filename_ = NULL;
static int                  log_fd = -1, connfd = -1;
static int                  listenfd;
static  socklen_t           clilen;
static struct sockaddr_in   cliaddr, servaddr;
static const int	        on = 1;
volatile sig_atomic_t       sigint_triggered = 0;
volatile sig_atomic_t       sigterm_triggered = 0;
static pthread_mutex_t      write_lock;
static pthread_mutex_t      meta_lock;
timer_t                     timer_id = 0;
static int                  file_size = 0;
static unsigned             thread_clients = 0;

// static struct itimerspec its = {   .it_value.tv_sec  = LOG_TIMER_INT,
//                                 .it_value.tv_nsec = 0,
//                                 .it_interval.tv_sec  = LOG_TIMER_INT,
//                                 .it_interval.tv_nsec = 0
//                             };

// static struct sigevent sev = { 0 };

#define POLL_SZ 2
static struct pollfd clientfds[ POLL_SZ ];

SLIST_HEAD(slisthead, slist_data_s) thrd_head;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void log_remote_peer_name( int client_sock, bool is_open );
void* connection_handler( void* arg );
static void add_thread( struct ThreadData* td );
static void do_maintenance();
static void process_message( int clientfd, char const* buf, int len );
static void dump_file_to_client( int fd );
static void make_daemon( void );
#ifndef USE_AESD_CHAR_DEVICE
static void timer_handler();
#endif
// static bool start_timer( void );
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool aesd_thrd_initialize( char const* filename, bool is_daemon ){
    syslog( LOG_DEBUG, "> aesd_thrd_initialize" );
    int rc = pthread_mutex_init( &write_lock, NULL );

    if( rc != 0 ){
        syslog( LOG_ERR, "Failed to initialize write lock." );
        return false;
    }

    rc = pthread_mutex_init( &meta_lock, NULL );

    if( rc != 0 ){
        syslog( LOG_ERR, "Failed to initialize meta lock." );
        return false;
    }

    SLIST_INIT( &thrd_head );

    filename_ = filename;
    syslog( LOG_DEBUG, "> open %s\n", filename_ );
    // log_fd = open( filename, O_RDWR, S_IRUSR | S_IWUSR );

    // if( log_fd < 0 ){
    //     syslog( LOG_ERR, "Cannot create %s, err: %s\n", filename, strerror( errno ) );
    //     return false;
    // }

    listenfd = socket( AF_INET, SOCK_STREAM, 0 );

    if( listenfd < 0 ){
        syslog( LOG_ERR, "Failed to create socket\n" );
        return false;
    }

    setsockopt( listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );
    memset( &servaddr, 0, sizeof( servaddr ) );
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl( INADDR_ANY );
    servaddr.sin_port = htons( SERV_PORT );
    rc = bind( listenfd, ( struct sockaddr* )&servaddr, sizeof( servaddr ) );

    if( rc < 0 ){
        syslog( LOG_ERR, "Failed to bind to 9000 port\n" );
        return false;
    }

    if( is_daemon ){
        make_daemon();
    }

    listen( listenfd, LISTENQ );
    
    // if( !start_timer() ){
    //     syslog( LOG_ERR, "Failed to start timer" );
    //     return false;
    // }
    clientfds[0].fd = listenfd;
    clientfds[0].events = POLLIN;

    return true;
}

void aesd_thrd_run(){
    syslog( LOG_DEBUG, "> aesd_thrd_run" );

    for( ;; ){
        int nready = poll( clientfds, 1, LOG_TIMER_INT * 1000 );
        

        if ( sigint_triggered ) {
            syslog( LOG_DEBUG, "sigint triggered, exiting...\n" );
            break;
        }

        if ( sigterm_triggered ) {
            syslog( LOG_DEBUG, "sigterm triggered\n" );
            break;
        }

#ifndef USE_AESD_CHAR_DEVICE
        if ( nready == 0 && thread_clients > 0 ) {
            timer_handler();
            continue;
        }
#endif
        if( nready > 0 ){
            connfd = accept( listenfd, (SA *) &cliaddr, &clilen );

            log_remote_peer_name( connfd, true );
            
            struct ThreadData* thrd = ( struct ThreadData* )malloc( sizeof( struct ThreadData) );
            thrd->write_lock     = &write_lock;
            thrd->fd            = connfd;
            thrd->is_completed  = 0;
            
            int rc = pthread_create( &thrd->p_tid, NULL, connection_handler, thrd ); 

            if( rc != 0 ){
                syslog( LOG_ERR, "> Failed to create connection handler thread" );
                close( connfd );
                free( thrd );
            }else{
                add_thread( thrd );
            }

            do_maintenance();
        }
    }
}

void aesd_thrd_signal_triggered( int sig ){
    if( sig == SIGINT ){
        sigint_triggered = 1;
    }

    if( sig == SIGTERM ){
        sigterm_triggered = 1;
    }
}

void aesd_thrd_shutdown(){
    // close( log_fd );
    close( listenfd );
    remove( filename_ );
    pthread_mutex_destroy( &write_lock );
    slist_data_t *datap = NULL;

    while ( !SLIST_EMPTY( &thrd_head ) ) {
        datap = SLIST_FIRST( &thrd_head );
        struct ThreadData* td = datap->td;
        pthread_t tid = td->p_tid;
        pthread_join( tid, NULL );
        SLIST_REMOVE_HEAD( &thrd_head, entries );
        free( td );
        free( datap );
        syslog( LOG_DEBUG, "> Thrd %lu completed.", tid );
    }

    // timer_delete( timer_id );
    syslog( LOG_DEBUG, "aesd_thrd_shutdown completed." );
}

//----------------------------------------------------- private impl -----------------------------------------------------//
//////////////////////////////////////////////////////////////////////////////////////////////
static void log_remote_peer_name( int client_sock, bool is_open ){
    char ipstr[INET6_ADDRSTRLEN];

    struct sockaddr_storage addr;
    socklen_t len;
    getpeername( client_sock, (struct sockaddr*)&addr, &len );
    struct sockaddr_in *s = (struct sockaddr_in *)&addr;
    inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof( ipstr ));

    if( is_open ){
        syslog( LOG_DEBUG, "> Accepted connection from %s\n", ipstr );
    }else{
        syslog( LOG_DEBUG, "> Closed connection from %s\n", ipstr );
    }
}

void* connection_handler( void* arg ){
    char buf[ MAXLINE ];
    struct ThreadData* td = ( struct ThreadData* )arg;
    int n;

    for( ;; ){
        if ( ( n = read( td->fd, buf, MAXLINE - 1 ) ) < 0 ) {
            /* connection reset by client */
            close( td->fd );
            td->is_completed = 1;
            log_remote_peer_name( td->fd, false );
            break;
        }else if( n == 0 ){// client disconnected
            close( td->fd );
            td->is_completed = 1;
            log_remote_peer_name( td->fd, false );
            break;
        }else{
            buf[ n ] = '\0';
            process_message( td->fd, buf, n );
        }
    }

    return NULL;
}

static void add_thread( struct ThreadData* td ){
    syslog( LOG_DEBUG, "> adding new thread %lu", td->p_tid );
    slist_data_t* datap = malloc(sizeof(slist_data_t));
    datap->td = td;
    SLIST_INSERT_HEAD( &thrd_head, datap, entries );
    thread_clients ++;
}

// https://man.archlinux.org/man/core/man-pages/SLIST_REMOVE.3.en
static void do_maintenance(){
    syslog( LOG_DEBUG, "> do maintenance" );
    slist_data_t *item = NULL;
    slist_data_t *tmp_item = NULL;

    SLIST_FOREACH_SAFE( item, &thrd_head, entries, tmp_item ) {
        struct ThreadData* td = item->td;

        if( td->is_completed ){
            pthread_t tid = td->p_tid;
            pthread_join( tid, NULL );
            SLIST_REMOVE( &thrd_head, item, slist_data_s, entries );
            free( td );
            free( item );
            syslog( LOG_DEBUG, "> Thrd %lu completed.", tid );
            thread_clients --;
        }
    }
}

static int write_safe( char const* buf, int len ){
    int rc = pthread_mutex_lock( &write_lock );

    if( 0 != rc ){
        syslog( LOG_ERR, "> Failed to lock write lock" );
        return -1;
    }

    log_fd = open( filename_, O_RDWR, S_IRUSR | S_IWUSR );

    if( log_fd < 0 ){
        syslog( LOG_ERR, "Cannot create %s, err: %s\n", filename_, strerror( errno ) );
        return -1;
    }

    int nbytes = write( log_fd, buf, len );
    pthread_mutex_unlock( &write_lock );
    close( log_fd );
    return nbytes;
}

static void process_message( int clientfd, char const* buf, int len ){   
    do{
        if( len <= 0 ){
            syslog( LOG_ERR, "> Invalid len %d", len );
            break;
        }

        syslog( LOG_DEBUG, "> process_message %s, %d\n", buf, len );
        int nbytes;

        nbytes = write_safe( buf, len );

        if( nbytes < 0 ){
            syslog( LOG_ERR, "Failed to write log data, err: %s\n", strerror( errno ) );
            break;
        }

        file_size += len;
        char const* nl = strchr( buf, '\n' );

        if( nl ){
            dump_file_to_client( clientfd );
        }   
    }while( 0 );
}

static void dump_file_to_client( int fd_client ){
    int rc = pthread_mutex_lock( &write_lock );

    if( 0 != rc ){
        syslog( LOG_ERR, "> Failed to lock write lock" );
        return;
    }

    log_fd = open( filename_, O_RDWR, S_IRUSR | S_IWUSR );

    if( log_fd < 0 ){
        syslog( LOG_ERR, "Cannot create %s, err: %s\n", filename_, strerror( errno ) );
        pthread_mutex_unlock( &write_lock );
        return;
    }

    // fsync( log_fd );
    // rc = lseek( log_fd, 0, SEEK_SET );

    // if( rc < 0 ){
    //     syslog( LOG_ERR, "Failed to seek to file start" );
    // }else
    {
        int bytes_left = file_size;
        int rn = 0;

        do{
            char tmpbuf[ BUFFSIZE ];
            memset( tmpbuf, 0, sizeof( tmpbuf ) );
            rn = read( log_fd, tmpbuf, sizeof( tmpbuf ) );
            // tmpbuf[rn ] = '\0';
            syslog( LOG_DEBUG, "> dump_file_to_client: file_size = %d, bytes_left = %d, read chunk = %d\n", file_size, bytes_left, rn );

            if( rn > 0 ){
                // printf( "> chunk to send (%s)\n", tmpbuf );
                int wr_rc = write( fd_client, tmpbuf, rn );

                if( wr_rc < 0 ){
                    syslog( LOG_ERR, "> write back failed with %s", strerror( errno ) );
                }

                bytes_left -= rn;
            }else{
                syslog( LOG_ERR, "read returned %d, err: %s\n", rn, strerror( errno ) );
                break;//????
            }
        }while( rn > 0 );

        // lseek( log_fd, 0, SEEK_SET );
        // lseek( log_fd, file_size, SEEK_SET );
        close( log_fd );
    }

    pthread_mutex_unlock( &write_lock );
}

static void make_daemon( void )
{
    pid_t pid;
    
    /* Fork off the parent process */
    pid = fork();
    
    /* An error occurred */
    if (pid < 0){
        syslog( LOG_ERR, "fork failed" );
        exit(EXIT_FAILURE);
    }
    
     /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);
    
    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);
    
    /* Catch, ignore and handle signals */
    /*TODO: Implement a working signal handler */
    
    
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
    /* or another appropaesdsocketriated directory */
    int ch_ret = chdir("/");

    if( ch_ret < 0 ){
        syslog( LOG_ERR, "Failed to change dir" );
        exit(EXIT_FAILURE);
    }
    
    /* Close all open file descriptors */
    close( STDIN_FILENO );
    close( STDOUT_FILENO );
    close( STDERR_FILENO );
}

#ifndef USE_AESD_CHAR_DEVICE
static void timer_handler() {
    time_t anytime;
    struct tm *current;
    char time_str[ 64 ];
    memset( time_str, 0, sizeof( time_str ) );
    time(&anytime);

    current = localtime(&anytime);
    strftime( time_str, 64, "timestamp:%Y-%m-%d %H:%M:%S\n", current );
    syslog( LOG_DEBUG, "%s", time_str );
    write_safe( time_str, strlen( time_str ) );
    // domaintenace = true;
}
#endif
// https://opensource.com/article/21/10/linux-timers
// https://stackoverflow.com/questions/55666829/counting-time-with-timer-in-c
// static bool start_timer( void ){
//     memset(&sev, 0, sizeof(struct sigevent));
//     sev.sigev_notify            = SIGEV_THREAD;
//     sev.sigev_notify_function   = &timer_handler;
//     // sev.sigev_value.sival_ptr = &info;

//     if( timer_create( CLOCK_REALTIME, &sev, &timer_id ) != 0 ) {
//          syslog( LOG_ERR, "timer_create failed" );
//          return false;
//     }

//     if( timer_settime(timer_id, 0, &its, NULL) != 0 ){
//         syslog( LOG_ERR, "timer_settime" );
//         return false;
//     }

//     return true;
// }