/*
 * @file server.h
 * @author Mohammed Sameer
 * @brief this file includes all the common utility functions and definitions
 * for the distributed app.
 */

#ifndef _SERVER_H_
#define _SERVER_H_
#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
typedef struct _server_ctx {
    int fd;
    struct sockaddr_in addr;
} server_ctx_t;

typedef struct _client_ctx {
    int fd;
    int client_id;
    struct sockaddr_in addr;
    client_request_t req;
    bool is_backlog;
} client_ctx_t;
#define CLIENT_CTX_INITIALISER                                                 \
    { -1, 0, {1}, CLIENT_REQUEST_INITIALISER, 0 }

#endif
