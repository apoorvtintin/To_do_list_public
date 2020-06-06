CC = gcc
CFLAGS  = -Wall -Werror

.PHONY: app clean client server

app: server client

client: client.c
	$(CC) $(CFLAGS) $^ -o $@

server: server.c
	$(CC) $(CFLAGS) $^ -o server

clean:
	rm -rf client server
