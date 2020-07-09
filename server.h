/*
 * @file server.h
 * @author Mohammed Sameer
 * @brief this file includes all the common utility functions and definitions
 * for the distributed app.
 */

#ifndef _SERVER_H_
#define _SERVER_H_

typedef struct _server_ctx {
    int fd;
    struct sockaddr_in addr;
} server_ctx_t;

typedef struct _client_ctx {
    int fd;
    int client_id;
    struct sockaddr_in addr;
    client_request_t req;
} client_ctx_t;

#endif
