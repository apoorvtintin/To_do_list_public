#include "c_s_iface.h"
#include "util.h"
#include "server.h"
#include "replication_util.h"

// Global Variables
int verbose = 0;
rep_manager_data data;

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

    if(read_config_file(argv[1], server_addr, &data) != 0) {
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

        handle_fault_detector_message(&conn_client_ctx);
    }
    return 0;
}

int handle_fault_detector_message(client_ctx_t *conn_client_ctx) {
    int msg_len = 0;
    char msg_buf[MAXMSGSIZE]; // define a MAXLINE macro.
    char key[MAXMSGSIZE], value[MAXMSGSIZE];
    unsigned int input_fields_counter = 0;
    sock_buf_read client_fd;
    init_buf_fd(&client_fd, conn_client_ctx->fd);
    memset(msg_buf, 0, MAXMSGSIZE);
    memset(key, 0, MAXMSGSIZE);
    memset(value, 0, MAXMSGSIZE);
    replication_manager_message fault_detector_ctx; // replication manager stuff

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
            return -1;
        }
        if (sscanf(msg_buf, "%[^:]: %[^\r\n]", key, value) != 2) {
            fprintf(stderr, "Malformed key value received in request!!!\n");
            return -1;
        }
        if (parse_fault_detector_kv(fault_detector_ctx, key, value) == -1) {
            return -1;
        }
        ++input_fields_counter;
    }
    handle_state(fault_detector_ctx);
}

int handle_state(replication_manager_message fault_detector_ctx) {
    // change internal state
    int replica_id = fault_detector_ctx.replica_id;
    data.node[replica_id].state = fault_detector_ctx.state;
    // check if unstable state
    // take action based on that
    handle_current_state();
    return 0;
}

int handle_current_state() {
    int replica_iter, active_count, inactive_count, startup_req;
    active_count = 0;
    inactive_count = 0;
    startup_req = 0;

    for(replica_iter = 0; replica_iter < data.num_replicas; replica_iter++) {
        
        if(data.node[replica_iter].state == RUNNING) {
            active_count++;
        } 

        else if (data.node[replica_iter].state == FAULTED) {
            if(restart_server(replica_iter) != 0) {
                fprintf(stderr, "Startup req could not be sent\n");
            } else {
                data.node[replica_iter].state = SENT_STARTUP_REQ;
            }
            inactive_count++;
        }

        else if(data.node[replica_iter].state == SENT_STARTUP_REQ) {
            inactive_count++;
            startup_req++;
        }

        else {
            fprintf(stderr, "Unhandled state :/\n");
        }
    }

    fprintf(stdout, "Number of active replicas %d, " 
                    "Number of inactive replicas %d, "
                    "Number of startup requests sent %d\n",
                    active_count, inactive_count, startup_req);
    return 0;
}

int restart_server(int replica_id) {
    int clientfd = 0;
    int status = 0;

    char buf[MAX_LENGTH];

    sprintf(buf,"Replica ID: %d\r\n"
            "Factory Req: %d\r\n",
            replica_id, STARTUP);

    clientfd = connect_to_server(&data.node[replica_id].factory);
    if (clientfd < 0) {
        printf("connect failed: %s\n", strerror(errno));
        return -1;
    }

    status = write(clientfd, buf, MAX_LENGTH);
    if (status < 0) {
        printf("Write failed: %s\n", strerror(errno));
        close(clientfd);
        return -1;
    }

    close(clientfd);

    return 0;
}