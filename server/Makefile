CFLAGS += -Wall -Werror
DEPS = aesdsocket.h
all: aesdsocket 

%.o: %.c $(DEPS)
	$(CC) -g -c -o $@ $< $(CFLAGS)

aesdsocket: aesdsocket.o main.o
	$(CC) -o aesdsocket aesdsocket.o main.o 

clean:
	rm -f aesdsocket aesdsocket.o main.o

