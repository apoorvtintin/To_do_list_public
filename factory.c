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
#include <sys/wait.h>
#include <signal.h>

#include "c_s_iface.h"
#include "local_f_detector.h"
#include "server.h"
#include "storage.h"
#include "util.h"
#include "confuse.h"
#include "replication_util.h"

// Definitions
#define SERVER_PATH "server"
#define FAULT_DETECTOR_PATH "local_f_detector"

// Global Variables
long verbose = 0;
extern char **environ; /* Defined by libsc */
long replica_id = -1;
factory_data f_data;

// Forward Declarations
static int handle_replication_manager_message(client_ctx_t conn_client_ctx);
static int handle_rep_man_command(factory_message message);
static int spawn_server(char* path);
static int spawn_fault_detector(char* path);
static int parse_rep_manager_kv (
    factory_message *rep_manager_ctx, 
    char *key, char *value);

static void init_client_ctx(client_ctx_t *ctx) {
    ctx->fd = -1;
    memset(&ctx->addr, 0, sizeof(ctx->addr));
    memset(&ctx->req, 0, sizeof(ctx->req));
    return;
}

void sigchld_handler(int sig)  {
    int olderr = errno;
    int status;
    while ((waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0);
    errno = olderr;
}

static int read_config_file(char *path) {
    int parse_ret;

	cfg_opt_t opts[] = {
		CFG_SIMPLE_STR("factory_ip", &f_data.server_ip),
		CFG_SIMPLE_STR("factory_port", &f_data.port),
        CFG_SIMPLE_INT("factory_replica_id", &replica_id),
        CFG_SIMPLE_INT("factory_verbose", &verbose),
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

    //DEBUG
    printf("factory IP: %s port: %s \n", f_data.server_ip, f_data.port);

    return 0;
}

int factory_init(char *server_path, char *fault_detector_path) {
    if(server_path == NULL) {
        fprintf(stderr,"servre path NULL\n");
        return -1;
    }

    // Create environment variable
    if (putenv("MY_ENV=42") < 0) {
        perror("putenv error");
        exit(1);
    }

    // Handles terminated or stopped child
    signal(SIGCHLD, sigchld_handler);
    if(spawn_server(server_path) || 
       spawn_fault_detector(fault_detector_path)) {
        fprintf(stderr, "Spawn error");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    int listen_fd = -1, optval = 1, accept_ret_val = -1;
    struct sockaddr_in server_addr;

    if (argc < 1) {
        // print usage
        fprintf(stderr, "Usage: %s <configuration file path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if(read_config_file(argv[1]) != 0) {
        fprintf(stderr, "READ CONFIG FILE ERR!\n");
        exit(EXIT_FAILURE);
    }

    if (verbose > 5 || verbose < 0) {
        fprintf(stderr, "Warning: illegal verbose value ignoring it.\n");
    }

    //initialize factory and spawn server and fault detector
    factory_init(SERVER_PATH, FAULT_DETECTOR_PATH);

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, f_data.server_ip, &server_addr.sin_addr.s_addr) != 1) {
        fprintf(stderr, "Entered IP Address invalid!\n");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_port = htons(atoi(f_data.port));
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

    free(f_data.server_ip);
    free(f_data.port);

    struct sockaddr client_addr;
    socklen_t client_addr_len;
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

    printf("recieved message from replication manager\n");
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
        printf("buf %s\n", msg_buf);
        if (parse_rep_manager_kv(&message, key, value) == -1) {
            return -1;
        }
        ++input_fields_counter;
    }
    return handle_rep_man_command(message);
}

int handle_rep_man_command(factory_message message) {
    printf("global replica ID %ld replica ID %d message enum %d\n",replica_id ,message.replica_id,
        message.req);
    if(message.replica_id == replica_id) {
        if(message.req == STARTUP) {
            spawn_server(SERVER_PATH);
        }
    } else {
        printf("wrong replica ID\n");
    }
    return 0;
}

int spawn_server(char* path) {
    //command line arguments
    char *newargv[] = { path, "127.0.0.1", "12345", NULL };
    
    //Fork server
    pid_t pid = fork();
    if(pid == 0) {
        if (execve(path, newargv, environ) < 0) {
            //proccess execve error
            fprintf(stderr,"server path incorrect or binary does not exist\n");
            exit(0);
        }
    } else if(pid < 0) {
        fprintf(stderr,"error occurred while forking\n");
        return -1;
    }
    return 0;
}

int spawn_fault_detector(char* path) {
    //command line arguments
    char *newargv[] = { path, "127.0.0.1", "12345" ,"5" , "127.0.0.1", "12346", "0", NULL };

    //Fork server
    pid_t pid = fork();
    if(pid == 0) {
        if (execve(path, newargv, environ) < 0) {
            //proccess execve error
            fprintf(stderr,"fault detector path incorrect or binary does not exist\n");
            exit(0);
        }
    }
    else if(pid < 0) {
        fprintf(stderr,"error occurred while forking\n");
        return -1;
    }
    return 0;
}

static int parse_rep_manager_kv (
    factory_message *rep_manager_ctx, 
    char *key, char *value) 
{
    if (strcmp(key, "Replica ID") == 0) {
        if (str_to_int(value, &rep_manager_ctx->replica_id) != 0) {
            fprintf(stderr, "replica_id conversion failed; Malformed req!!!\n");
            return -1;
        }
    } else if (strcmp(key, "Factory Req") == 0) {
        if (str_to_int(value, (int *)&rep_manager_ctx->req) != 0) {
            fprintf(stderr, "Msg type Conversion failed!!!\n");
            return -1;
        }
    }
    return 0;
}