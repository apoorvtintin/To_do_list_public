/*
 * @file util.c
 * @brief This file implemts the utility functions defined in util.h
 * @author Mohammed Sameer (mohammes@andrew.cmu.edu)
 */

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

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
// TODO: Test this out once when coding the server.
ssize_t sock_readline(int fd, void *buf, size_t n) {
    size_t nleft;
    ssize_t nread;
    unsigned char *ptr = buf;
    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR) {
                nread = 0;
            } else {
                return -1;
            }
        } else if (nread == 0) {
            break;
        }
        nleft -= nread;
        ptr += nread;

        if ((ptr != buf) && (*(ptr - 1) == '\n')) {
            break;
        }
    }
    return nread;
}

int str_to_int(char *str, int *res) {
    char *end = NULL;
    long int v = strtol(str, &end, 0);
    if (v == LONG_MIN || *end != '\0') {
        return -1;
    }
    *res = (int)v;
    return 0;
}
