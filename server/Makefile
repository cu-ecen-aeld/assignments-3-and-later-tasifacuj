CFLAGS += -Wall -Werror
DEPS = aesd_server_thrd.h
all: aesdsocket 

%.o: %.c $(DEPS)
	$(CC) -g -c -o $@ $< $(CFLAGS)

aesdsocket: aesd_server_thrd.o main_thrd.o
	$(CC) -o aesdsocket aesd_server_thrd.o main_thrd.o 

clean:
	rm -f aesdsocket aesdsocket_thrd.o main_thrd.o

