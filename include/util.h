/*
 * @file util.h
 * @author Mohammed Sameer
 * @brief this file includes all the common utility functions and definitions
 * for the distributed app.
 */

#ifndef __UTIL__H
#define __UTIL__H

#include "c_s_iface.h"

typedef struct buf_fd {
    int fd;
    char buf[8192];
    char *buf_ptr;
    size_t bytes_pend;
} sock_buf_read;

// Socket Read and write funnctions which account for
// short counts.
// reference: CSAPP:3e
ssize_t sock_readn(int fd, void *buf, size_t maxlen);
ssize_t sock_readline(sock_buf_read *ptr, void *buf, size_t n);
ssize_t sock_writen(int fd, const void *buf, size_t maxlen);
int str_to_int(char *str, int *res);
void init_buf_fd(sock_buf_read *ptr, int fd);

int connect_to_server(struct server_info *server);
int get_response_from_server(int clientfd, struct message_response *response);
int parse_response_from_server(struct message_response *response,
                               int client_id);
char *get_task_status_str(enum t_status stat);
char *get_msg_type_str(msg_type_t msg_type);

unsigned long get_file_size(char *filename);
void init_bsvr_ctx(bsvr_ctx *obj);

#endif
