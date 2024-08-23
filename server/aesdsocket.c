#include "aesdsocket.h"
#include "aesdsocket_cfg.h"
#include <string.h>
#include	<sys/types.h>	/* basic system data types */
#include	<sys/socket.h>	/* basic socket definitions */
#include <netinet/in.h>
#include <signal.h>
#include <poll.h>

#include <syslog.h>

#include <errno.h>
#include <limits.h>		/* for OPEN_MAX */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <arpa/inet.h>

static char buf[ MAXLINE ];

static struct sockaddr_in cliaddr, servaddr;
static int maxi, listenfd, connfd, sockfd;
static int nready;
static  socklen_t clilen;
ssize_t				n;
static int log_fd = -1;
static int file_size = 0;
static const int		on = 1;

#define  OPEN_MAX 256
static struct pollfd client[OPEN_MAX];
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void log_remote_peer_name( int client_sock, bool is_open );
static void process_message( int clientfd, char const* buf, int len );
static void dump_file_to_client( int fd );
static char const* filename_ = NULL;

volatile sig_atomic_t sigint_triggered = 0;
volatile sig_atomic_t sigterm_triggered = 0;


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void aesd_signal_triggered( int sig ){
    if( sig == SIGINT ){
        sigint_triggered = 1;
    }

    if( sig == SIGTERM ){
        sigterm_triggered = 1;
    }
}

bool aesd_initialize( char const* filename ){
    filename_ = filename;
    log_fd = open( filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR );

    if( log_fd < 0 ){
        syslog( LOG_ERR, "Cannot create %s, err: %s\n", filename, strerror( errno ) );
        return false;
    }

    listenfd = socket( AF_INET, SOCK_STREAM, 0 );

    if( listenfd < 0 ){
        syslog( LOG_ERR, "Failed to create socket\n" );
        return false;
    }

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    memset( &servaddr, 0, sizeof( servaddr ) );
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl( INADDR_ANY );
    servaddr.sin_port = htons( SERV_PORT );
    int rc = bind( listenfd, ( struct sockaddr* )&servaddr, sizeof( servaddr ) );

    if( rc < 0 ){
        syslog( LOG_ERR, "Failed to bind to 9000 port\n" );
        return false;
    }

    listen( listenfd, LISTENQ );

    client[0].fd = listenfd;
    client[0].events = POLLIN;
    
    for ( int i = 1; i < OPEN_MAX; i++ ){
        client[i].fd = -1; /* -1 indicates available entry */
    }
 
    maxi = 0; /* max index into client[] array */
    return true;
}

void aesd_run(){
    syslog( LOG_DEBUG, "aesd_run" );

    for( ;; ){
        int i = 1;
        nready = poll( client, maxi + 1, INFTIM );

        if ( sigint_triggered ) {
            syslog( LOG_DEBUG, "sigint triggered, exiting...\n" );
            break;
        }

        if ( sigterm_triggered ) {
            syslog( LOG_DEBUG, "sigterm triggered\n" );
            break;
        }

        if ( nready == -1 && errno != EINTR ) {
            syslog( LOG_ERR, "poll failed" );
            continue;
        }

        if ( client[0].revents & POLLIN ) {
            clilen = sizeof( cliaddr );
            connfd = accept( listenfd, (SA *) &cliaddr, &clilen );
            log_remote_peer_name( connfd, true );
            
            for( i = 1; i < OPEN_MAX; i ++ ){
                if( client[ i ].fd < 0 ){
                    client[ i ].fd = connfd;
                    break;
                }
            }//for

            if( i == OPEN_MAX ){
                syslog( LOG_ERR, "aesd server reached max clients num\n" );
                break;
            }

            client[ i ].events = POLLIN;

            if( i > maxi ){
                maxi = i;
            }

            if( -- nready <= 0 ){
                continue;/* no more readable descriptors */
            }
        }//client[0]

        for( i = 1; i <= maxi; i ++ ){
            if( ( sockfd = client[ i ].fd ) < 0 ){
                continue;
            }

            if (client[i].revents & (POLLIN | POLLERR)) {
                if ( ( n = read( sockfd, buf, MAXLINE ) ) < 0 ) {
                    if (errno == ECONNRESET) {
                         /* connection reset by client */
                        close(sockfd);
                        client[ i ].fd = -1;
                        log_remote_peer_name( connfd, false );
                    }else{
                        syslog( LOG_ERR, "aesd server failed to read\n" );
                        break;
                    }
                }else if( n == 0 ){// client disconnected
                    close( sockfd );
                    client[ i ].fd = -1;
                    log_remote_peer_name( connfd, false );
                }else{
                    buf[ n ] = '\0';
                    process_message( sockfd, buf, n );
                }

                if (--nready <= 0){
                    break;				/* no more readable descriptors */
                }
            }
        }// for
    }//while true
}

void aesd_shutdown(){
    syslog( LOG_DEBUG, "aesd_shutdown" );
    close( log_fd );
    close( listenfd );
    remove( filename_ );
}


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

static void process_message( int clientfd, char const* buf, int len ){
    if( len <= 0 ){
        syslog( LOG_ERR, "> Invalid len %d", len );
        return;
    }

    syslog( LOG_DEBUG, "> process_message %s, %d\n", buf, len );
    //printf( "> process_message %s, %d\n", buf, len );
    int nbytes;
    char const* nl = strchr( buf, '\n' );

    if( nl ){
        int partial_pos = nl - buf;// + '\n'
        syslog( LOG_DEBUG, "nl found at %d pos", partial_pos );
        nbytes = write( log_fd, buf, partial_pos + 1 );

        if( nbytes < 0 ){
            syslog( LOG_ERR, "Failed to write log data, err: %s\n", strerror( errno ) );
            return;
        }

        file_size += partial_pos + 1;
        dump_file_to_client( clientfd );
        int bytes_left = len - ( partial_pos + 1 );

        if( bytes_left > 0 ){
            process_message( clientfd, buf + partial_pos + 1, bytes_left );
        }
    }else{
        syslog( LOG_DEBUG, "nl not found" );
        nbytes = write( log_fd, buf, len );

        if( nbytes < 0 ){
            syslog( LOG_ERR, "Failed to write log data, err: %s\n", strerror( errno ) );
            return;
        }

        file_size += len;
    }   
}

static void dump_file_to_client( int fd_client ){
    fsync( log_fd );
    int rc = lseek( log_fd, 0, SEEK_SET );

    if( rc < 0 ){
        syslog( LOG_ERR, "Failed to seek to file start" );
    }else{
        int bytes_left = file_size;

        while( bytes_left > 0 ){
            char tmpbuf[ BUFFSIZE ];
            memset( tmpbuf, 0, sizeof( tmpbuf ) );
            int rn = read( log_fd, tmpbuf, sizeof( tmpbuf ) );
            // tmpbuf[rn ] = '\0';
            syslog( LOG_DEBUG, "> dump_file_to_client: file_size = %d, bytes_left = %d, read chunk = %d\n", file_size, bytes_left, rn );
            // printf( "> chunk to send (%s)", tmpbuf );

            if( rn > 0 ){
                write( fd_client, tmpbuf, rn );
                bytes_left -= rn;
            }else{
                syslog( LOG_ERR, "read returned %d, err: %s\n", rn, strerror( errno ) );
                break;//????
            }
        }

        lseek( log_fd, 0, SEEK_SET );
        lseek( log_fd, file_size, SEEK_SET );
    }
}