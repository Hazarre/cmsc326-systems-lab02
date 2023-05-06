all: setitimer-helper squish-pty squish-unix

CC = LD_LIBRARY_PATH=/usr/lib/x86_64-linux-musl/ gcc
CFLAGS = -Wall -W  -I/usr/include -I/usr/include/x86_64-linux-musl/ 
#CC = gcc
#CFLAGS = -Wall -W
LOADLIBES = -lm

setitimer-helper: setitimer-helper.o
squish-pty: squish-pty.o
squish-unix: squish-unix.o

clean: 
	rm -f *.o setitimer-helper squish-pty squish-unix
