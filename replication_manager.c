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

#include "c_s_iface.h"
#include "util.h"
#include "server.h"
#include "replication_util.h"
#include "confuse.h"

// Global Variables
long verbose = 0;
rep_manager_data data;

//FORWARD DECLARATIONS
static int handle_fault_detector_message(client_ctx_t *conn_client_ctx);
static int read_config_file(char *path);
static int restart_server(int replica_id);
static int handle_current_state();
static int handle_state(replication_manager_message fault_detector_ctx);
static int parse_fault_detector_kv(
            replication_manager_message *fault_detector_ctx, 
            char *key, char *value);

static void init_client_ctx(client_ctx_t *ctx) {
    ctx->fd = -1;
    memset(&ctx->addr, 0, sizeof(ctx->addr));
    memset(&ctx->req, 0, sizeof(ctx->req));
    return;
}

int main(int argc, char *argv[]) {
    int listen_fd = -1, optval = 1, accept_ret_val = -1;
    struct sockaddr_in server_addr;

    if(read_config_file(argv[1]) != 0) {
        fprintf(stderr, "READ CONFIG FILE ERR!\n");
        exit(EXIT_FAILURE);
    }

    data.num_replicas = 1;

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, data.server_ip, &server_addr.sin_addr.s_addr) != 1) {
        fprintf(stderr, "Entered IP Address invalid!\n");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_port = htons(atoi(data.port));
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

    free(data.server_ip);
    free(data.port);

    struct sockaddr client_addr;
    socklen_t client_addr_len;
    client_ctx_t conn_client_ctx;

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
        if (parse_fault_detector_kv(&fault_detector_ctx, key, value) == -1) {
            return -1;
        }
        printf("buffer key %s, value %s\n", key, value);
        ++input_fields_counter;
    }
    return handle_state(fault_detector_ctx);
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

static int read_config_file(char *path) {

    int parse_ret;

	cfg_opt_t opts[] = {
		CFG_SIMPLE_STR("configuration_manager_ip", &data.server_ip),
		CFG_SIMPLE_STR("configuration_manager_port", &data.port),
        CFG_SIMPLE_INT("configuration_manager_verbose", &verbose),
        CFG_SIMPLE_INT("configuration_manager_num_replicas", &(data.num_replicas)),
		CFG_END()
	};
	cfg_t *cfg;
    cfg = cfg_init(opts, 0);
    
    if((parse_ret = cfg_parse(cfg, path)) != CFG_SUCCESS) {
        if(parse_ret == CFG_FILE_ERROR) {
            fprintf(stderr,"REPLICATION MANAGER config file not found\n");
        } else {
            fprintf(stderr,"REPLICATION MANAGER config file parse error\n");
        }
        return -1;
    } 
    return 0;
}

static int parse_fault_detector_kv(
            replication_manager_message *fault_detector_ctx, 
            char *key, char *value) 
{
    if (strcmp(key, "Replica ID") == 0) {
        if (str_to_int(value, &fault_detector_ctx->replica_id) != 0) {
            fprintf(stderr, "replica_id conversion failed; Malformed req!!!\n");
            return -1;
        }
    } else if (strcmp(key, "Message Type") == 0) {
        if (str_to_int(value, (int *)&fault_detector_ctx->state) != 0) {
            fprintf(stderr, "Msg type Conversion failed!!!\n");
            return -1;
        }
    }
    return 0;
}