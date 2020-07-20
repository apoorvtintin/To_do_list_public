
/* @author Apoorv Gupta <apoorvgupta@hotmail.co.uk> */

/* HEADER FILES */

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
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>

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
factory_data f_data;
int outfile_append = 1; // to control append to outfiles
int first_time = 0;
int mode = 0;

// Forward Declarations
static int handle_replication_manager_message(client_ctx_t conn_client_ctx);
static int handle_rep_man_command(factory_message message);
static int spawn_server(char *path);
static int spawn_fault_detector(char *path);
static int parse_rep_manager_kv(factory_message *rep_manager_ctx, char *key,
                                char *value);

static void init_client_ctx(client_ctx_t *ctx) {
    ctx->fd = -1;
    memset(&ctx->addr, 0, sizeof(ctx->addr));
    memset(&ctx->req, 0, sizeof(ctx->req));
    return;
}

void sigchld_handler(int sig) {
    int olderr = errno;
    int status;
    while ((waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
        ;
    errno = olderr;
}

static int read_config_file(char *path) {
    int parse_ret;

    cfg_opt_t opts[] = {
        CFG_SIMPLE_STR("factory_ip", &f_data.server_ip),
        CFG_SIMPLE_STR("factory_port", &f_data.port),
        CFG_SIMPLE_STR("replication_manager_ip",
                       &f_data.replication_manager_ip),
        CFG_SIMPLE_STR("replication_manager_port",
				&f_data.replication_manager_port),
		CFG_SIMPLE_STR("factory_spawned_server_ip", &f_data.spawned_server_ip),
		CFG_SIMPLE_STR("factory_spawned_server_port",
				&f_data.spawned_server_port),
        CFG_SIMPLE_STR("lfd_heartbeat", &f_data.lfd_heartbeat),
        CFG_SIMPLE_INT("factory_replica_id", &f_data.replica_id),
        CFG_SIMPLE_INT("factory_verbose", &verbose), CFG_END()};
    cfg_t *cfg;
    cfg = cfg_init(opts, 0);

    if ((parse_ret = cfg_parse(cfg, path)) != CFG_SUCCESS) {
        if (parse_ret == CFG_FILE_ERROR) {
            fprintf(stderr, "REPLICATION MANAGER config file not found\n");
        } else {
            fprintf(stderr, "REPLICATION MANAGER config file parse error\n");
        }
        return -1;
    }

    // DEBUG
    printf("factory IP: %s port: %s \n", f_data.server_ip, f_data.port);

    return 0;
}

int factory_init(char *server_path, char *fault_detector_path) {
    if (server_path == NULL) {
        fprintf(stderr, "servre path NULL\n");
        return -1;
    }
#if 0
    // Create environment variable
    if (putenv("MY_ENV=42") < 0) {
        perror("putenv error");
        exit(1);
    }
#endif

    // Handles terminated or stopped child
    signal(SIGCHLD, sigchld_handler);
#if 0
    if (spawn_server(server_path)) {
        fprintf(stderr, "Spawn error");
        return -1;
    }
#endif
    if (spawn_fault_detector(fault_detector_path)) {
        fprintf(stderr, "Spawn error");
        return -1;
    }
    outfile_append = 0; // next spawns will append file
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

    if (read_config_file(argv[1]) != 0) {
        fprintf(stderr, "READ CONFIG FILE ERR!\n");
        exit(EXIT_FAILURE);
    }

    if (verbose > 5 || verbose < 0) {
        fprintf(stderr, "Warning: illegal verbose value ignoring it.\n");
    }

    // initialize factory and spawn server and fault detector
    factory_init(SERVER_PATH, FAULT_DETECTOR_PATH);

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, f_data.server_ip, &server_addr.sin_addr.s_addr) !=
        1) {
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
            } else if (errno == EINTR) {
                continue;
            } else {
                fprintf(stderr, "Error on accept exiting. Reason: %s\n",
                        strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
        init_client_ctx(&conn_client_ctx);
        conn_client_ctx.fd = accept_ret_val;
        memcpy(&conn_client_ctx.addr, &client_addr, sizeof(struct sockaddr_in));
        handle_replication_manager_message(conn_client_ctx);
        close(accept_ret_val);
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
        if (parse_rep_manager_kv(&message, key, value) == -1) {
            return -1;
        }
		printf("%s\n", msg_buf);
		memset(msg_buf, 0, MAXMSGSIZE);
        ++input_fields_counter;
    }
    return handle_rep_man_command(message);
}

void fill_change_state_buf(char *buf, factory_message *message) {

	if (message->server_state == PASSIVE_PRIMARY) {
		sprintf(buf, "Client ID: %d\r\n"
					 "Request No: %d\r\n"
					 "Message Type: %d\r\n"
					 "REP MODE: %d\r\n"
					 "Server State: %d\r\n"
					 "Checkpoint freq: %d\r\n"
					 "Replica 1 ID: %d\r\n"
					 "Replica 1 IP: %s\r\n"
					 "Replica 1 Port: %d\r\n"
					 "Replica 2 ID: %d\r\n"
					 "Replica 2 IP: %s\r\n"
					 "Replica 2 Port: %d\r\n\r\n",
					 0, 0, MSG_REP_MGR, message->mode_rep,
					 message->server_state, message->checkpoint_freq,
					 message->bkp_replica_id_1,
					 message->server_arr[0].server_ip,
					 message->server_arr[0].port, message->bkp_replica_id_2,
					 message->server_arr[1].server_ip,
					 message->server_arr[1].port);
	} else {
		sprintf(buf, "Client ID: %d\r\n"
					 "Request No: %d\r\n"
					 "Message Type: %d\r\n"
					 "REP MODE: %d\r\n"
					 "Server State: %d\r\n\r\n",
					 0, 0, MSG_REP_MGR, message->mode_rep,
					 message->server_state);
			
	}

	return;
}

int send_change_state_message(factory_message message) {
	struct server_info server;
	int client_fd = 0;
	int status = 0;
	char buf[MAX_LENGTH];
	
	memset(&server, 0, sizeof(struct server_info));
	memset(buf, 0, MAX_LENGTH);

	fill_change_state_buf(buf, &message);

	printf("\n\nSending message to server BUF %s\n\n", buf);

	server.port = atoi(f_data.spawned_server_port);
	memcpy(server.server_ip, f_data.spawned_server_ip, 1024);

	client_fd = connect_to_server(&server);
	if (client_fd < 0) {
		printf("Connecting to server failed: %s\n", strerror(errno));
		return -1;
	}

	status = write(client_fd, buf, MAX_LENGTH);
	if (status < 0) {
		printf("Write failed: %s\n", strerror(errno));
		close(client_fd);
		return -1;
	}

	close(client_fd);

	return 0;
}

int send_checkpoint_to_bkps(factory_message message) {
	struct server_info server;
	int client_fd = 0;
	int status = 0;
	char buf[MAX_LENGTH];
	
	memset(&server, 0, sizeof(struct server_info));
	memset(buf, 0, MAX_LENGTH);
	
	sprintf(buf, "Client ID: %d\r\n"
			     "Request No: %d\r\n"
			     "Message Type: %d\r\n\r\n",
				 0, 0, MSG_SEND_CHKPT); 
		
	printf("\n\nSend checkpoint to server BUF %s\n\n", buf);

	server.port = atoi(f_data.spawned_server_port);
	memcpy(server.server_ip, f_data.spawned_server_ip, 1024);

	client_fd = connect_to_server(&server);
	if (client_fd < 0) {
		printf("Connecting to server failed: %s\n", strerror(errno));
		return -1;
	}

	status = write(client_fd, buf, MAX_LENGTH);
	if (status < 0) {
		printf("Write failed: %s\n", strerror(errno));
		close(client_fd);
		return -1;
	}

	close(client_fd);

	return 0;
}

int handle_rep_man_command(factory_message message) {
    int status = 0;

    printf("global replica ID %ld replica ID %d message enum %d\n",
           f_data.replica_id, message.replica_id, message.req);
    if (message.replica_id == f_data.replica_id) {
        if (message.req == STARTUP) {
            spawn_server(SERVER_PATH);
        } else if (message.req == CHANGE_STATE) {
			status = send_change_state_message(message);
			if (status < 0) {
				printf("Send change state message failed"
						"for server %d\n", message.replica_id);
			}
		} else if (message.req == SEND_CHKPT) {
			status = send_checkpoint_to_bkps(message);
			if (status < 0) {
				printf("Sending checkpoints message failed %d\n", 
						message.replica_id);
			}
		}
    } else {
        printf("wrong replica ID\n");
    }
    return 0;
}

int spawn_server(char *path) {
    // command line arguments
    char buf[10];
    snprintf(buf, 10, "%lu", f_data.replica_id);
	char *newargv[] = {path, f_data.spawned_server_ip,
							f_data.spawned_server_port, buf, NULL};
	char filename[1024];
    int ofd;
    int olderr = errno;

    memset(filename, 0, 1024);

    // Fork server
    pid_t pid = fork();
    if (pid == 0) {
        // open file for IO redirection

        sprintf(filename, "logs/server_replica_%ld_out", f_data.replica_id);

        ofd = open(filename, O_WRONLY | O_CREAT | O_TRUNC,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (ofd < 0) {
            printf("could not open outfile for server\n");
            errno = olderr;
            exit(0);
        } else {
            dup2(ofd, STDOUT_FILENO);
            dup2(ofd, STDERR_FILENO);
        }

        if (execve(path, newargv, environ) < 0) {
            // proccess execve error
            fprintf(stderr, "server path incorrect or binary does not exist\n");
            exit(0);
        }

    } else if (pid < 0) {
        fprintf(stderr, "error occurred while forking\n");
        return -1;
    }
    return 0;
}

int spawn_fault_detector(char *path) {
    // command line arguments
    char replica_id[10];
    char *newargv[] = {path,
                       f_data.spawned_server_ip,
                       f_data.spawned_server_port,
                       f_data.lfd_heartbeat,
                       f_data.replication_manager_ip,
                       f_data.replication_manager_port,
                       replica_id,
                       NULL};
    char filename[1024];
    int ofd;
    int olderr = errno;

    memset(filename, 0, 1024);

    sprintf(filename, "logs/lfd_serverid_%ld_out", f_data.replica_id);
    sprintf(replica_id, "%ld", f_data.replica_id);

    // Fork server
    pid_t pid = fork();
    if (pid == 0) {
        // open file for IO redirection
        ofd = open(filename, O_WRONLY | O_CREAT | (O_TRUNC & outfile_append),
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (ofd < 0) {
            printf("could not open outfile for lfd\n");
            errno = olderr;
        } else {
            dup2(ofd, STDOUT_FILENO);
            dup2(ofd, STDERR_FILENO);
        }

        if (execve(path, newargv, environ) < 0) {
            // proccess execve error
            fprintf(stderr,
                    "fault detector path incorrect or binary does not exist\n");
            exit(0);
        }
    } else if (pid < 0) {
        fprintf(stderr, "error occurred while forking\n");
        return -1;
    }
    return 0;
}

static int parse_rep_manager_kv(factory_message *rep_manager_ctx, char *key,
                                char *value) {

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
#if 0
    } else if (strcmp(key, "Server IP") == 0) {
		memcpy(f_data.spawned_server_ip, value, 1024);
    } else if (strcmp(key, "Server Port") == 0) {
		memcpy(f_data.spawned_server_port, value, 1024);
#endif
    } else if (strcmp(key, "REP MODE") == 0) {
		if (str_to_int(value, (int *)&rep_manager_ctx->mode_rep) != 0) {
            fprintf(stderr, "Mode Conversion failed!!!\n");
            return -1;
		}
    } else if (strcmp(key, "Server State") == 0) {
		if (str_to_int(value, (int *)&rep_manager_ctx->server_state) != 0) {
            fprintf(stderr, "State Conversion failed!!!\n");
            return -1;
		}
    } else if (strcmp(key, "Replica 1 ID") == 0) {
		if (str_to_int(value, (int *)&rep_manager_ctx->bkp_replica_id_1) != 0) {
            fprintf(stderr, "Mode Conversion failed!!!\n");
            return -1;
		}
    } else if (strcmp(key, "Replica 1 IP") == 0) {
		memcpy(rep_manager_ctx->server_arr[0].server_ip, value, 1024);
    } else if (strcmp(key, "Replica 1 Port") == 0) {
		if (str_to_int(value, (int *)&rep_manager_ctx->server_arr[0].port) != 0) {
            fprintf(stderr, "Port 1 failed!!!\n");
            return -1;
		}
	} else if (strcmp(key, "Replica 2 ID") == 0) {
		if (str_to_int(value, (int *)&rep_manager_ctx->bkp_replica_id_2) != 0) {
			fprintf(stderr, "Mode Conversion failed!!!\n");
			return -1;
		}
	} else if (strcmp(key, "Replica 2 IP") == 0) {
		memcpy(rep_manager_ctx->server_arr[1].server_ip, value, 1024);
	} else if (strcmp(key, "Replica 2 Port") == 0) {
		if (str_to_int(value, (int *)&rep_manager_ctx->server_arr[1].port) != 0) {
			fprintf(stderr, "Port 2 failed!!!\n");
			return -1;
		}
	} else if (strcmp(key, "Checkpoint freq") == 0) {
		if (str_to_int(value, (int *)&rep_manager_ctx->checkpoint_freq) != 0) {
			fprintf(stderr, "Checkpoint read failed!!!\n");
			return -1;
		}
	}

    return 0;
}
