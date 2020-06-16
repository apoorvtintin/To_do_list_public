/*
 * @file util.c
 * @brief This file implemts the utility functions defined in util.h
 * @author Mohammed Sameer (mohammes@andrew.cmu.edu)
 */

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "util.h"

ssize_t sock_readn(int fd, void *buf, size_t n) {
    size_t nleft;
    ssize_t nread;
    unsigned char *ptr;

    ptr = buf;
    nleft = n;
    for (nleft = n, ptr = buf; nleft > 0; nleft -= nread, ptr += nread) {
        if ((nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR) {
                // read was interrupted, no worries we call read
                // again.
                nread = 0;
            } else {
                // any other error, return let the user handle it.
                return -1;
            }
        } else if (nread == 0) {
            // EOF
            break;
        }
    }
    return (n - nleft); // return the read count.
}
ssize_t sock_writen(int fd, const void *buf, size_t n) {
    size_t nleft;
    ssize_t nwritten;
    const unsigned char *ptr;

    for (nleft = n, ptr = buf; nleft > 0; nleft -= nwritten, ptr += nwritten) {
        if ((nwritten = write(fd, ptr, nleft)) < 0) {
            if (errno == EINTR) {
                // read was interrupted, no worries we call read
                // again.
                nwritten = 0;
            } else {
                // any other error, return. let the user handle it.
                return -1;
            }
        }
    }
    return n;
}

void init_buf_fd(sock_buf_read *ptr, int fd) {
    ptr->fd = fd;
    memset(ptr->buf, 0, sizeof(ptr->buf));
    ptr->buf_ptr = ptr->buf;
    ptr->bytes_pend = 0;
}
ssize_t sock_read(sock_buf_read *ptr, void *buf, size_t n) {
    if (ptr->bytes_pend == 0) {
        ptr->bytes_pend = sock_readn(ptr->fd, ptr->buf, sizeof(ptr->buf));
        if (ptr->bytes_pend < 0) {
            ptr->bytes_pend = 0;
        }
        ptr->buf_ptr = ptr->buf;
    }
    if (ptr->bytes_pend != 0) {
        memcpy(buf, ptr->buf_ptr,
               ((n < ptr->bytes_pend) ? n : ptr->bytes_pend));
        ptr->bytes_pend -= ((n < ptr->bytes_pend) ? n : ptr->bytes_pend);
        ptr->buf_ptr += ((n < ptr->bytes_pend) ? n : ptr->bytes_pend);
        return ((n < ptr->bytes_pend) ? n : ptr->bytes_pend);
    }
    return 0;
}
ssize_t sock_readline(sock_buf_read *ptr, void *buf, size_t n) {
    char c;
    size_t nleft = n;
    char *out_buffer = buf;
    while (nleft > 0) {
        if (sock_read(ptr, &c, 1))
            ;
        out_buffer[n - nleft] = c;
        nleft -= 1;
        if (c == '\n') {
            break;
        }
    }
    return n - nleft;
}
// TODO: Test this out once when coding the server.

int str_to_int(char *str, int *res) {
    char *end = NULL;
    long int v = strtol(str, &end, 0);
    if (v == LONG_MIN || *end != '\0') {
        return -1;
    }
    *res = (int)v;
    return 0;
}

int connect_to_server(struct server_info *server) {
    struct sockaddr_in servaddr;
    int sockfd;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("Socket create failed: %s\n", strerror(errno));
    }

    memset(&servaddr, 0, sizeof(struct sockaddr_in));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(server->server_ip);
    servaddr.sin_port = htons(server->port);

    if (connect(sockfd, (SA *)&servaddr, sizeof(servaddr)) != 0) {
        return -1;
    }

    return sockfd;
}

void get_response_from_server(int clientfd, struct message_response *response) {

    char resp_buf[MAX_LENGTH];
    char temp[TASK_LENGTH];

    memset(resp_buf, 0, MAX_LENGTH);
    memset(temp, 0, TASK_LENGTH);

    sock_buf_read client_fd;
    init_buf_fd(&client_fd, clientfd);

    // printf("\n**********\n");

    while (sock_readline(&client_fd, resp_buf, MAX_LENGTH) > 0) {
        if (!strncmp(resp_buf, "\r\n", strlen("\r\n"))) {
            break;
        }

        if (!strncmp(resp_buf, "Status", strlen("Status"))) {
            sscanf(resp_buf, "Status: %s", response->status);
        }

        if (!strncmp(resp_buf, "Client ID", strlen("Client ID"))) {
            sscanf(resp_buf, "Client ID: %s", temp);
            response->client_id = atoi(temp);
        }

        if (!strncmp(resp_buf, "Key", strlen("Key"))) {
            sscanf(resp_buf, "Key: %s", temp);
            response->hash_key = strtoul(temp, NULL, 10);
            // printf("Task Key: %llu\n", response->hash_key);
        }
        // printf("%s\n", resp_buf);
    }

    return;
}

int parse_response_from_server(struct message_response *response, int client_id) {

    if (response->client_id != client_id) {
        printf("Response message not intended for the client!!\n");
        return -2;
    }

    if (!strncmp(response->status, "OK", strlen("OK"))) {
        return 0;
    } else if (!strncmp(response->status, "FAIL", strlen("FAIL"))) {
        return -1;
    }

    return 0;
}
