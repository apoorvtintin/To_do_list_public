/*
 * @file util.h
 * @author Mohammed Sameer
 * @brief this file includes all the common utility functions and definitions
 * for the distributed app.
 */
#ifndef __UTIL__H
#define __UTIL__H
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

#endif
