CC = gcc
CFLAGS  = -pthread -Wall -Werror -g -DM_THRD_LQ -std=c99
LLVM_PATH = /usr/local/depot/llvm-3.9.1/bin/
#LLVM_PATH = /Users/phani/code/llvm-project/build/bin/

CFILES = $(wildcard *.c)
HFILES = $(wildcard *.h)

BINS = server client local_f_detector factory replication_manager

.PHONY: app clean client server format factory replication_manager

app: server client local_f_detector factory replication_manager

factory: factory.c libconfuse.a libutil.a
	$(CC) $(CFLAGS) $^ -o $@

replication_manager: replication_manager.c libconfuse.a libutil.a
	$(CC) $(CFLAGS) $^ -o $@

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
log.o:	log.c
	gcc -O -g -c log.c
worker.o:	worker.c
	gcc -O -g -c worker.c
libutil.a:	util.o log.o worker.o
	ar rcs libutil.a util.o log.o worker.o
server: server.c libutil.a storage.o db.o
	$(CC) $(CFLAGS) $^ -o server -lcrypto



format: $(CFILES) $(HFILES)
	$(LLVM_PATH)clang-format -style=file -i $^

clean:
	rm -rf $(BINS) *.o logs/*
