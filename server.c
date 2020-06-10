#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "c_s_iface.h"
#include "util.h"
#include "msg_util.h"

// Global variables
int verbose = 0;

typedef struct _server_ctx
{
    int fd;
    struct sockaddr_in addr;
} server_ctx_t;

typedef struct _client_ctx
{
    int fd;
    struct sockaddr_in addr;
} client_ctx_t;


void handle_connection(client_ctx_t *client_ctx)
{
    int msg_len = 0;
    char msg_buf[MAXMSGSIZE];
    msg_len = sock_readn(client_ctx->fd, msg_buf, MAXMSGSIZE);
	if (msg_len < 0) {
	}
    // copy the buffer to message_storage.
    print_msg(msg_buf);
    if(client_ctx->fd >= 0)
    {
        close(client_ctx->fd);
        client_ctx->fd = -1;
    }
    return;
}

void init_server_ctx(server_ctx_t *ctx)
{
    ctx->fd = -1;
    memset(&ctx->addr, 0, sizeof(ctx->addr));
    return;
}

void init_client_ctx(client_ctx_t *ctx)
{
    ctx->fd = -1;
    memset(&ctx->addr, 0, sizeof(ctx->addr));
    return;
}

int main(int argc, char *argv[]) 
{
    int listen_fd = -1, optval = 1, accept_ret_val = -1, opt;
    struct sockaddr_in server_addr;
    client_ctx_t conn_client_ctx;
    if(argc < 3)
    {
        // print usage
        fprintf(stderr, "Usage: %s <ip_address> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    while ((opt = getopt(argc, argv, "v:")) != -1) {
        switch (opt) {
            case 'v':
                verbose = atoi(optarg);
                break;
        }
    }
    if(verbose > 5 || verbose < 0)
    {
        fprintf(stderr, "Warning: illegal verbose value ignoring it.\n"); 
    }
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, argv[1]
                , &server_addr.sin_addr.s_addr) != 1)
    {
        fprintf(stderr, "Entered IP Address invalid!\n");   
        exit(EXIT_FAILURE);
    }

    server_addr.sin_port = htons(atoi(argv[2])); 
    listen_fd = socket(AF_INET, SOCK_STREAM, 0); // create a TCP socket.
    if(listen_fd < 0)
    {
        fprintf(stderr, "Socket creation failed. Reason: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
            sizeof(int));
    if(bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        fprintf(stderr, "Bind to address failed, Reason: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if(listen(listen_fd, 50) < 0)
    {
        fprintf(stderr, "listen() failed. Reason: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct sockaddr client_addr;
    socklen_t client_addr_len;
    memset(&client_addr, 0, sizeof(struct sockaddr));

    while(1)
    {
        accept_ret_val = accept(listen_fd, &client_addr,&client_addr_len );
        if(verbose >= 3)
        {
            printf("Connection accepted !!!\n");
        }
        if(accept_ret_val < 0)
        {
            if(errno == ECONNABORTED)
            {
                continue;
            }
            else
            {
                fprintf(stderr, "Error on accept exiting. Reason: %s\n", 
                        strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
        init_client_ctx(&conn_client_ctx);
        conn_client_ctx.fd = accept_ret_val;
        memcpy(&conn_client_ctx.addr, &client_addr, sizeof(struct sockaddr_in));    
        handle_connection(&conn_client_ctx);
    }
    return 0;
}
