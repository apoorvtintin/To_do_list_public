#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include "c_s_iface.h"
#include "server.h"
#include "util.h"

// Global variables
int verbose = 0;

// Local help functions

void print_user_req(client_ctx_t *client_ctx)
{
    client_request_t *msg_ptr = &client_ctx->req;
    static int once = 0;
    if(once == 0)
    {
        char buffer[4096];
        int ch = snprintf(buffer, sizeof(buffer),"%-2s%-8s%4s %-30s%8s %-10s%4s %-11s%4s %-8s%2s\n", 
                "|","MSG Type", "","TASK", "","Date", "","Task Status", "","Mod Flags", "|");
        ch = ch-2;
        printf("%s", buffer);
        memset(buffer, 0, sizeof(buffer));
        memset(buffer, '=', ch);
        buffer[0] = '+';
        buffer[ch] = '+';
        printf("%s\n", buffer);
        once = 1;
    }
    char buffer[4096];

    int ch = snprintf(buffer, sizeof(buffer),"%-2s%-8d%4s %-30s%8s %-10s%4s %-11d%4s %-8d%3s\n", 
            "|",
            msg_ptr->msg_type,"",
            msg_ptr->task,"",
            msg_ptr->date,"",
            msg_ptr->task_status,"",
            msg_ptr->mod_flags,
            "|");
    printf("%s", buffer);memset(buffer, 0, sizeof(buffer));
    ch = ch -2;
    memset(buffer, '-', ch);
    buffer[0] = '+';
    buffer[ch] = '+';
    printf("%s\n", buffer);
#if 0
    printf("----------------------------------------------------------\n");    
    printf("    %-10d\t\t %-12s\t\t %8d\t %8.2f\n\n", 100, "Mohammed", 2, 10000000);
    printf("| Message Type : %d                                      |\n", msg_ptr->msg_type);
    printf("| Task         : %s                                      |\n", msg_ptr->task);
    printf("| Task Date    : %s                                      |\n", msg_ptr->date);
    printf("| Task Status  : %d                                      |\n", msg_ptr->task_status);
    printf("| Mod flags    : %d                                      |\n", msg_ptr->mod_flags);
    printf("----------------------------------------------------------\n");    
#endif
}

void write_client_responce(client_ctx_t *client_ctx, char *status, char *msg) {
    char resp_buf[MAX_LENGTH];
    int resp_len = snprintf(resp_buf, sizeof(resp_buf), "Status: %s\r\n"
                                                        "Client ID: %d\r\n"
                                                        "Msg: %s\r\n"
                                                        "\r\n",
                            status, client_ctx->client_id, msg);
    if (resp_len > sizeof(resp_buf)) {
        fprintf(stderr, "server resp is greater than the resp_buffer!!");
        return;
    }
    if (sock_writen(client_ctx->fd, resp_buf, resp_len) == -1) {
        fprintf(stderr, "Error while writing responce to client\n");
    }
    return;
}

int parse_kv(client_ctx_t *client_ctx, char *key, char *value) {

    if (strcmp(key, "Client ID") == 0) {
        if (str_to_int(value, &client_ctx->client_id) != 0) {
            fprintf(stderr, "Client_id conversion failed; Malformed req!!!\n");
            // TODO: write to client error.
            write_client_responce(client_ctx, "FAIL",
                                  "Malformed Client ID received");
            return -1;
        }
    } else if (strcmp(key, "Message Type") == 0) {
        if (str_to_int(value, (int *)&client_ctx->req.msg_type) != 0) {
            // TODO: write error to client;
            write_client_responce(client_ctx, "FAIL",
                                  "Malformed Msg Type received");
            fprintf(stderr, "Msg Id Conversion failed!!!\n");
            return -1;
        }
    } else if (strcmp(key, "Task") == 0) {
        strncpy(client_ctx->req.task, value, sizeof(client_ctx->req.task));
    } else if (strcmp(key, "Task status") == 0) {
        if (str_to_int(value, (int *)&client_ctx->req.task_status) != 0) {
            // TODO: write error to client;
            write_client_responce(client_ctx, "FAIL", "Malformed Task Status");
            fprintf(stderr, "Msg Id Conversion failed!!!\n");
            return -1;
        }
    } else if (strcmp(key, "Due date") == 0) {
        strncpy(client_ctx->req.date, value, sizeof(client_ctx->req.date));
    }
    return 0;
}

void* handle_connection(void *arg)
{
    pthread_detach(pthread_self());
    client_ctx_t *client_ctx = arg;
    int msg_len = 0;
    char msg_buf[MAXMSGSIZE]; // define a MAXLINE macro.
    char key[MAXMSGSIZE], value[MAXMSGSIZE];
    unsigned int input_fields_counter = 0;
    sock_buf_read client_fd;
    init_buf_fd(&client_fd, client_ctx->fd);
    while(1)
    {
        msg_len = sock_readline(&client_fd, msg_buf, MAXMSGSIZE);
        if(msg_len == 0)
        {
            // The client closed the connection we should to.
            goto _EXIT;
        }

        if (msg_len == 2 && msg_buf[0] == '\r' && msg_buf[1] == '\n') {
            // End of message.
            break;
        }
        if (input_fields_counter >= MAX_REQ_FIELDS) {
            fprintf(stderr, "Client sent too many input fields \n");
            // TODO: write some error to client.
            write_client_responce(client_ctx, "FAIL",
                                  "req had more than req num of fields");
            goto _EXIT;
        }
        if (sscanf(msg_buf, "%[^:]: %[^\r\n]", key, value) != 2) {
            fprintf(stderr, "Malformed key value received in request!!!\n");
            write_client_responce(client_ctx, "FAIL",
                                  "Malformed Key-Value received in input");
            // TODO: write error to the client.
            goto _EXIT;
        }
        if (parse_kv(client_ctx, key, value) == -1) {
            goto _EXIT;
        }
        ++input_fields_counter;
        //        write(1, msg_buf, msg_len);
        //        write(1,"\n",1);
    }
    // TODO: put a check to see if all the required fields are present.
    print_user_req(client_ctx);
    //
    // send responce.
    write_client_responce(client_ctx, "OK", "Success");
_EXIT:
    if (client_ctx->fd >= 0) {
        close(client_ctx->fd);
        client_ctx->fd = -1;
    }
    if(client_ctx)
    {
        free(client_ctx);
        client_ctx = NULL;
    }
    return (void *)0;
}

void init_server_ctx(server_ctx_t *ctx) {
    ctx->fd = -1;
    memset(&ctx->addr, 0, sizeof(ctx->addr));
    return;
}

void init_client_ctx(client_ctx_t *ctx) {
    ctx->fd = -1;
    memset(&ctx->addr, 0, sizeof(ctx->addr));
    return;
}

int main(int argc, char *argv[]) {
    int listen_fd = -1, optval = 1, accept_ret_val = -1, opt;
    struct sockaddr_in server_addr;
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
    if (verbose > 5 || verbose < 0) {
        fprintf(stderr, "Warning: illegal verbose value ignoring it.\n");
    }
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr.s_addr) != 1) {
        fprintf(stderr, "Entered IP Address invalid!\n");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_port = htons(atoi(argv[2]));
    listen_fd = socket(AF_INET, SOCK_STREAM, 0); // create a TCP socket.
    if (listen_fd < 0) {
        fprintf(stderr, "Socket creation failed. Reason: %s\n",
                strerror(errno));
        exit(EXIT_FAILURE);
    }
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
               sizeof(int));
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) !=
        0) {
        fprintf(stderr, "Bind to address failed, Reason: %s\n",
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (listen(listen_fd, 50) < 0) {
        fprintf(stderr, "listen() failed. Reason: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct sockaddr client_addr;
    socklen_t client_addr_len;
    pthread_t th_id;
    memset(&client_addr, 0, sizeof(struct sockaddr));

    while (1) {
        accept_ret_val = accept(listen_fd, &client_addr, &client_addr_len);
        if (verbose >= 3) {
            printf("Connection accepted !!!\n");
        }
        if (accept_ret_val < 0) {
            if (errno == ECONNABORTED) {
                continue;
            } else {
                fprintf(stderr, "Error on accept exiting. Reason: %s\n",
                        strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
        client_ctx_t *conn_client_ctx = malloc(sizeof(client_ctx_t));
        init_client_ctx(conn_client_ctx);
        conn_client_ctx->fd = accept_ret_val;
        memcpy(&conn_client_ctx->addr, &client_addr, sizeof(struct sockaddr_in));    
        if(pthread_create(&th_id, NULL,handle_connection, 
                    (void *)conn_client_ctx) != 0)
        {
            fprintf(stderr, "!!!!!!!!!!Thread Creation failed!!!!!!!");
            if(conn_client_ctx->fd != -1)
            {
                close(conn_client_ctx->fd);
                free(conn_client_ctx);
            }   
        }
    }
    return 0;
}
