CC = gcc
CFLAGS  = -pthread -Wall -Werror -g
LLVM_PATH = /usr/local/depot/llvm-3.9.1/bin/
#LLVM_PATH = /Users/phani/code/llvm-project/build/bin/

CFILES = $(wildcard *.c)
HFILES = $(wildcard *.h)

.PHONY: app clean client server format

app: server client local_f_detector

client: client.c libutil.a
	$(CC) $(CFLAGS) $^ -o $@

local_f_detector: local_f_detector.c libutil.a
	$(CC) $(CFLAGS) $^ -o $@

db.o: db.c
	gcc -O -g -c db.c
storage.o: storage.c
	gcc -O -g -c storage.c
util.o:	util.c
	gcc -O -g -c util.c
libutil.a:	util.o
	ar rcs libutil.a util.o
server: server.c libutil.a storage.o db.o
	$(CC) $(CFLAGS) $^ -o server -lcrypto

format: $(CFILES) $(HFILES)
	$(LLVM_PATH)clang-format -style=file -i $^

clean:
	rm -rf client server *.o
