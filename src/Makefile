CC = gcc
INCLUDE_DIR = -I../include/
LIBS_DIR = -L../libs/
LINK_LIBS = -lutil -lconfuse
CFLAGS  = -O -pthread -Wall -Werror -g -DM_THRD_LQ -std=c99 $(INCLUDE_DIR) $(LIBS_DIR)
LLVM_PATH = /usr/local/depot/llvm-3.9.1/bin/
BIN_DIR = ../bin/
OBJ_DIR = ../obj/

CFILES = $(wildcard *.c)
HFILES = $(wildcard *.h)

BINS = server client local_f_detector factory replication_manager

.PHONY: app clean client server format factory replication_manager libutil

app: libutil server client local_f_detector factory replication_manager

factory: factory.c
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)$@ $(LIBS_DIR) $(LINK_LIBS)

replication_manager: replication_manager.c 
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)$@ $(LIBS_DIR) $(LINK_LIBS)

client: client.c 
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)$@ $(LIBS_DIR) $(LINK_LIBS)

local_f_detector: local_f_detector.c
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)$@ $(LIBS_DIR) $(LINK_LIBS)

$(OBJ_DIR)db.o: db.c
	$(CC)  $(INCLUDE_DIR)   -c db.c -o $(OBJ_DIR)/db.o
$(OBJ_DIR)storage.o: storage.c
	$(CC) $(INCLUDE_DIR) -c storage.c -o $(OBJ_DIR)/storage.o
$(OBJ_DIR)util.o:	util.c
	$(CC) $(CFLAGS) -c util.c -o $(OBJ_DIR)/util.o
$(OBJ_DIR)log.o:	log.c
	$(CC) $(CFLAGS) -c log.c -o $(OBJ_DIR)/log.o
$(OBJ_DIR)worker.o:	worker.c
	$(CC) $(CFLAGS) -c worker.c -o $(OBJ_DIR)/worker.o
$(OBJ_DIR)state.o:	state.c
	$(CC) $(CFLAGS) -c state.c -o $(OBJ_DIR)/state.o
$(OBJ_DIR)chkpt.o:	chkpt.c
	$(CC) $(CFLAGS) -c chkpt.c -o $(OBJ_DIR)/chkpt.o
../libs/libutil.a:	$(OBJ_DIR)util.o $(OBJ_DIR)log.o $(OBJ_DIR)worker.o
	ar rcs ../libs/libutil.a $(OBJ_DIR)util.o $(OBJ_DIR)log.o $(OBJ_DIR)worker.o
libutil:	../libs/libutil.a
server: server.c $(OBJ_DIR)storage.o $(OBJ_DIR)db.o $(OBJ_DIR)state.o $(OBJ_DIR)chkpt.o
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)server -lcrypto $(LIBS_DIR) $(LINK_LIBS)



format: $(CFILES) $(HFILES)
	$(LLVM_PATH)clang-format -style=file -i $^

clean:
	rm -rf $(BINS) *.o logs/* ../libs/libutil.a $(BIN_DIR)* $(OBJ_DIR)*
