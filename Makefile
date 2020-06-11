CC = gcc
CFLAGS  = -pthread -Wall -Werror -g

.PHONY: app clean client server

app: server client

client: client.c libutil.a
	$(CC) $(CFLAGS) $^ -o $@
util.o:	util.c
	gcc -O -g -c util.c
libutil.a:	util.o
	ar rcs libutil.a util.o
server: server.c libutil.a
	$(CC) $(CFLAGS) $^ -o server

clean:
	rm -rf client server *.o
