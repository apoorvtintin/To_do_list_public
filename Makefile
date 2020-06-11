CC = gcc
CFLAGS  = -pthread -Wall -Werror -g
LLVM_PATH = /usr/local/depot/llvm-3.9.1/bin/

CFILES = $(wildcard *.c)
HFILES = $(wildcard *.h)

.PHONY: app clean client server format

app: server client

client: client.c libutil.a
	$(CC) $(CFLAGS) $^ -o $@
util.o:	util.c
	gcc -O -g -c util.c
libutil.a:	util.o
	ar rcs libutil.a util.o
server: server.c libutil.a
	$(CC) $(CFLAGS) $^ -o server

format: $(CFILES) $(HFILES)
	$(LLVM_PATH)clang-format -style=file -i $^

clean:
	rm -rf client server *.o
