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
#include "local_f_detector.h"
#include "server.h"
#include "storage.h"
#include "util.h"
#include "libconfuse/confuse.h"
#include "replication_util.h"

// Definitions
#define SERVER_PATH "server"
#define FAULT_DETECTOR_PATH "local_f_detector"

// Global Variables
int verbose = 0;
extern char **environ; /* Defined by libsc */
int replica_id = -1;

void sigchld_handler(int sig)  {
    int olderr = errno;
    int status;
    while ((waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0);
    errno = olderr;
}

// Global variables
int verbose = 0;
int read_config_file(char *path, struct sockaddr_in server_addr) {
    static cfg_bool_t verbose = cfg_false;
    char *port, ip;
	cfg_opt_t opts[] = {
		CFG_SIMPLE_BOOL("verbose", &verbose),
		CFG_SIMPLE_STR("ip", &ip),
		CFG_SIMPLE_STR("port", &port),
        CFG_SIMPLE_INT("replica_id", &replica_id),
		CFG_END()
	};
	cfg_t *cfg;
    cfg = cfg_init(opts, 0);
    cfg_parse(cfg, "global.conf");

    //DEBUG
    printf("IP: %s port: %s \n", verbose);
}

int factory_init(char *server_path, char *fault_detector_path) {
    if(server_path == NULL) {
        fprinf(stderr,"servre path NULL\n");
        return -1;
    }
    // Handles terminated or stopped child
    Signal(SIGCHLD, sigchld_handler);
    if(spawn_server(server_path) & 
       spawn_fault_detector(fault_detector_path)) {
        fprintf(stderr, "Spawn error");
        return -1;
    }
    return 0;
}

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

    //initialize factory and spawn server and fault detector
    factory_init(SERVER_PATH, FAULT_DETECTOR_PATH);

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
        handle_replication_manager_message(conn_client_ctx);
    }
    return 0;
}

int handle_replication_manager_message(client_ctx_t conn_client_ctx) {
    int msg_len = 0;
    char msg_buf[MAXMSGSIZE]; // define a MAXLINE macro.
    char key[MAXMSGSIZE], value[MAXMSGSIZE];
    unsigned int input_fields_counter = 0;
    sock_buf_read client_fd;
    init_buf_fd(&client_fd, conn_client_ctx.fd);
    memset(msg_buf, 0, MAXMSGSIZE);
    memset(key, 0, MAXMSGSIZE);
    memset(value, 0, MAXMSGSIZE);
    factory_message message; // replication manager stuff

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
            write_client_responce(conn_client_ctx, "FAIL",
                                  "req had more than req num of fields");
            return -1;
        }
        if (sscanf(msg_buf, "%[^:]: %[^\r\n]", key, value) != 2) {
            fprintf(stderr, "Malformed key value received in request!!!\n");
            write_client_responce(conn_client_ctx, "FAIL",
                                  "Malformed Key-Value received in input");
            return -1;
        }
        if (parse_rep_manager_kv(&message, key, value) == -1) {
            return -1;
        }
        ++input_fields_counter;
    }
    return handle_rep_man_command(message);
}

int handle_rep_man_command(factory_message message) {
    if(message.replica_id == replica_id) {
        if(message.req == STARTUP) {
            spawn_server(SERVER_PATH);
        }
    }
    return 0;
}

int spawn_server(char* path) {
    //Fork server
    pid_t pid = fork();
    if(pid == 0)
        if (execve(path, NULL, environ) < 0) {
            //proccess execve error
            fprinf(stderr,"server path incorrect or binary does not exist\n");
            exit(0);
        }
    else if(pid < 0) {
        fprintf(stderr,"error occurred while forking\n");
    }
}

int spawn_fault_detector(char* path) {
    //Fork server
    pid_t pid = fork();
    if(pid == 0)
        if (execve(path, NULL, environ) < 0) {
            //proccess execve error
            fprinf(stderr,"fault detector path incorrect or binary does not exist\n");
            exit(0);
        }
    else if(pid < 0) {
        fprintf(stderr,"error occurred while forking\n");
    }
}