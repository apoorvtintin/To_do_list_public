CC = gcc
CFLAGS  = -pthread -Wall -Werror

.PHONY: app clean client server

app: server client

client: client.c libutil.a
	$(CC) $(CFLAGS) $^ -o $@
util.o:	util.c
	gcc -O -c util.c
msg_util.o:	msg_util.c
	gcc -O -c msg_util.c
libutil.a:	util.o
	ar rcs libutil.a util.o
libmsgutil.a:	msg_util.o
	ar rcs libmsgutil.a msg_util.o
server: server.c libutil.a libmsgutil.a
	$(CC) $(CFLAGS) $^ -o server

clean:
	rm -rf client server *.o
