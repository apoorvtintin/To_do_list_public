CC = gcc
CFLAGS  = -pthread -Wall -Werror -g -DM_THRD_LQ -std=c99
LLVM_PATH = /usr/local/depot/llvm-3.9.1/bin/

CFILES = $(wildcard *.c)
HFILES = $(wildcard *.h)

BINS = server client local_f_detector factory replication_manager

.PHONY: app clean client server format factory replication_manager
app: 
	mkdir -p bin obj
	make -C src libutil server client local_f_detector factory replication_manager

factory:
	make -C src factory

replication_manager:
	make -C src replication_manager

client:
	make -C src client

local_f_detector:
	make -C local_f_detector

server:
	make -C src server
libutil:
	make -C src libutil



format: 
	make -C src format

clean:
	make -C src clean
	rm -rf logs/* tmp/*
	rm -rf bin obj
