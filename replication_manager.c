#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "c_s_iface.h"
#include "util.h"
#include "server.h"

// Definitions
#define MAX_REPLICAS 3

// Structures
enum replica_state {
    RUNNING,
    FAULTED,
    SENT_STARTUP_REQ
};

typedef struct replica_node_t {
    int replica_id;
    enum replica_state;
    struct sockaddr_in factory_addr;
} replica_node;

typedef struct rep_manager_data_t {
    int num_replicas;
    replica_node node[MAX_REPLICAS];
} rep_manager_data;

// Global Variables
int verbose = 0;

int main(int argc, char *argv[]) {
    int listen_fd = -1, optval = 1, accept_ret_val = -1, opt;
    struct sockaddr_in server_addr;

    if (argc < 2) {
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

    if(read_config_file(argv[1], server_addr) != 0) {
        fprintf(stderr, "READ CONFIG FILE ERR!\n");
        exit(EXIT_FAILURE);
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
    client_ctx_t conn_client_ctx;
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
        init_client_ctx(&conn_client_ctx);
        conn_client_ctx.fd = accept_ret_val;
        memcpy(&conn_client_ctx.addr, &client_addr,
                sizeof(struct sockaddr_in));

        handle_fault_detector_message(conn_client_ctx);
    }
    return 0;
}

int handle_fault_detector_message(client_ctx_t conn_client_ctx) {
    int msg_len = 0;
    char msg_buf[MAXMSGSIZE]; // define a MAXLINE macro.
    char key[MAXMSGSIZE], value[MAXMSGSIZE];
    unsigned int input_fields_counter = 0;
    sock_buf_read client_fd;
    init_buf_fd(&client_fd, client_ctx->fd);
    memset(msg_buf, 0, MAXMSGSIZE);
    memset(key, 0, MAXMSGSIZE);
    memset(value, 0, MAXMSGSIZE);
    fault_detector_ctx; // replication manager stuff

    while (1) {
        msg_len = sock_readline(&client_fd, msg_buf, MAXMSGSIZE);
        if (msg_len == 0) {
            // The client closed the connection we should to.
            return -1;
        }

        if (msg_len == 2 && msg_buf[0] == '\r' && msg_buf[1] == '\n') {
            // End of message.
            break;
        }
        if (input_fields_counter >= MAX_REQ_FIELDS) {
            fprintf(stderr, "Client sent too many input fields \n");
            write_client_responce(client_ctx, "FAIL",
                                  "req had more than req num of fields");
            return -1;
        }
        if (sscanf(msg_buf, "%[^:]: %[^\r\n]", key, value) != 2) {
            fprintf(stderr, "Malformed key value received in request!!!\n");
            write_client_responce(client_ctx, "FAIL",
                                  "Malformed Key-Value received in input");
            return -1;
        }
        if (parse_fault_detector_kv(fault_detector_ctx, key, value) == -1) {
            return -1;
        }
        ++input_fields_counter;
    }
    return handle_command(fault_detector_ctx);
}

int handle_fault_detector_message(fault_detector_ctx) {

    return 0;
}
