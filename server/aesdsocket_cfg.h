#pragma once

#define	MAXLINE		4096	/* max text line length */
#define	MAXSOCKADDR  128	/* max socket address structure size */
#define	BUFFSIZE	8192	/* buffer size for reads and writes */
#define	LISTENQ		1024	/* 2nd argument to listen() */
#define INFTIM        -1    /* infinite poll timeout */
#define	SA	struct sockaddr


#define SERV_PORT   9000
#define LOG_TIMER_INT   10
#define USE_AESD_CHAR_DEVICE 1

#ifdef USE_AESD_CHAR_DEVICE
    #define LOG_PATH "/dev/aesdchar"
#else
    #define LOG_PATH  "/var/tmp/aesdsocketdata"
#endif