CC = gcc
CFLAGS  = -Wall -Werror

.PHONY: app clean client server

app: server client

client: client.c
	$(CC) $(CFLAGS) $^ -o $@
util.o:	util.c
	gcc -O -c util.c
libutil.a:	util.o
	ar rcs libutil.a util.o
server: server.c libutil.a
	$(CC) $(CFLAGS) $^ -o server

clean:
	rm -rf client server *.o
