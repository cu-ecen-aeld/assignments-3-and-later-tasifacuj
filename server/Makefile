CFLAGS ?= -Wall -Werror
DEPS = aesd_server_thrd.h
LDFLAGS ?= -lpthread -lrt
all: aesdsocket 

%.o: %.c $(DEPS)
	$(CC) -g -c -o $@ $< $(CFLAGS) $(LDFLAGS)

aesdsocket: aesd_server_thrd.o main_thrd.o
	$(CC)  aesd_server_thrd.o main_thrd.o -o $@ $(LDFLAGS)

clean:
	rm -f aesdsocket aesdsocket_thrd.o main_thrd.o

