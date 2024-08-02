#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#include <syslog.h>
#include <string.h>
#include <errno.h>


int main( int argc, char** argv ){
    syslog( LOG_DEBUG, "num of args %i\n", argc );
    if( argc < 3 ){
        syslog( LOG_ERR, "Missng arguments\n" );
        return 1;
    }

    char const* filename = argv[ 1 ];
    char const* content = argv[ 2 ];

    int fd = open( filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR );

    if( fd < 0 ){
        syslog( LOG_ERR, "Cannot create %s, err: %s\n", filename, strerror( errno ) );
        return 1;
    }
    
    syslog( LOG_DEBUG, "Writing %s to %s\n", content, filename );
    int nbytes = write( fd, content, strlen( content ) );

    if( nbytes < 0 ){
        syslog( LOG_ERR, "Failed to write content\n" );
        return 1;
    }

    close( fd );
    return 0;
}