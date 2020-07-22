
/** @author Apoorv Gupta <apoorvgupta@hotmail.co.uk> */

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

#include "c_s_iface.h"
#include "util.h"
#include "server.h"
#include "replication_util.h"
#include "confuse.h"
#include "state.h"

// Global Variables
long verbose = 0;
rep_manager_data data;
pthread_mutex_t *lock;

int mode = 0;
int primary_replica_id = -1;

struct membership_data {
    int server_status[MAX_REPLICAS];
    int num_servers;
	int needs_checkpoint[MAX_REPLICAS];
};

struct membership_data membership;

// FORWARD DECLARATIONS
static int stop_quiesce_all();
static int quiesce_all();
static int quiesce_stop_message(int replica_id);
static int quiesce_message(int replica_id);
static void *gfd_hbt();
static void init_gfd_hbt();
static int handle_fault_detector_message(client_ctx_t *conn_client_ctx);
static int read_config_file(char *path);
static int restart_server(int replica_id);
static int handle_current_state();
static int handle_state(replication_manager_message fault_detector_ctx);
static int
parse_fault_detector_kv(replication_manager_message *fault_detector_ctx,
                        char *key, char *value);

static void init_client_ctx(client_ctx_t *ctx) {
    ctx->fd = -1;
    memset(&ctx->addr, 0, sizeof(ctx->addr));
    memset(&ctx->req, 0, sizeof(ctx->req));
    return;
}

void init_server_state() {
	int i = 0;

	for (i = 0; i < MAX_REPLICAS; i++) {
		data.node[i].state = FAULTED;
	}

	return;
}

int main(int argc, char *argv[]) {
    int listen_fd = -1, optval = 1, accept_ret_val = -1;
    struct sockaddr_in server_addr;

    if (read_config_file(argv[1]) != 0) {
        printf("READ CONFIG FILE ERR!\n");
        exit(EXIT_FAILURE);
    }

    memset(&membership, 0, sizeof(struct membership_data));

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, data.server_ip, &server_addr.sin_addr.s_addr) != 1) {
        printf("Entered IP Address invalid!\n");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_port = htons(atoi(data.port));
    listen_fd = socket(AF_INET, SOCK_STREAM, 0); // create a TCP socket.
    if (listen_fd < 0) {
        printf("Socket creation failed. Reason: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
               sizeof(int));
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) !=
        0) {
        printf("Bind to address failed, Reason: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (listen(listen_fd, 50) < 0) {
        printf("listen() failed. Reason: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    free(data.server_ip);
    free(data.port);

    struct sockaddr client_addr;
    socklen_t client_addr_len;
    client_ctx_t conn_client_ctx;

    init_gfd_hbt();

	init_server_state();

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
                printf("Error on accept exiting. Reason: %s\n",
                       strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
        init_client_ctx(&conn_client_ctx);
        conn_client_ctx.fd = accept_ret_val;
        memcpy(&conn_client_ctx.addr, &client_addr, sizeof(struct sockaddr_in));

        handle_fault_detector_message(&conn_client_ctx);
        close(accept_ret_val);
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
            printf("Client sent too many input fields \n");
            return -1;
        }
        if (sscanf(msg_buf, "%[^:]: %[^\r\n]", key, value) != 2) {
            printf("Malformed key value received in request!!!\n");
            return -1;
        }
        if (parse_fault_detector_kv(&fault_detector_ctx, key, value) == -1) {
            return -1;
        }
        // printf("buffer key %s, value %s\n", key, value);
        ++input_fields_counter;
    }
    return handle_state(fault_detector_ctx);
}

void print_current_state_info(replication_manager_message fault_detector_ctx) {
    int id = fault_detector_ctx.replica_id;
    int i = 0;

    printf("\n***************************\n");

    if ((membership.server_status[id] == 0) &&
        (fault_detector_ctx.state == RUNNING)) {
        membership.server_status[id] = 1;
        membership.num_servers++;
        printf("Server %d is up!\n", id);
    } else if ((membership.server_status[id] == 1) &&
               (fault_detector_ctx.state == FAULTED)) {
        membership.server_status[id] = 0;
        membership.num_servers--;
        printf("Server %d is down!\n", id);
    }

    printf("\nActive Servers : %d\n", membership.num_servers);
    for (i = 0; i < MAX_REPLICAS; i++) {
        if (membership.server_status[i] == 0) {
            continue;
        } else {
            printf("Server %d: ACTIVE\n", i);
        }
    }
    printf("\n***************************\n\n");
    fflush(stdout);
}

void fill_message_for_primary_election(char *buf, int replica_id) {
	int i = 0;
	int replica_id_1 = -1;
	int replica_id_2 = -1;
    server_states_t state;

	rep_mode_t mode_rep;
	
	if (mode == 0) {
		mode_rep = PASSIVE_REP;
        state = PASSIVE_PREPRIMARY;
	} else if (mode == 1) {
		mode_rep = ACTIVE_REP;
        state = ACTIVE_RUNNING;
	}

	for (i = 0; i < MAX_REPLICAS; i++) {
		if (i == replica_id)
			continue;

		if (replica_id_1 == -1) {
			replica_id_1 = i;
			continue;
		}

		if (replica_id_2 == -1) {
			replica_id_2 = i;
			continue;
		}
	}

	sprintf(buf, "Replica ID: %d\r\n"
			"Factory Req: %d\r\n"
			"REP MODE: %d\r\n"
			"Server State: %d\r\n"
			"Checkpoint freq: %ld\r\n"
			"Replica 1 ID: %d\r\n"
			"Replica 1 IP: %s\r\n"
			"Replica 1 Port: %d\r\n"
			"Replica 2 ID: %d\r\n"
			"Replica 2 IP: %s\r\n"
			"Replica 2 Port: %d\r\n\r\n",
			replica_id, CHANGE_STATE,
			mode_rep, state, data.checkpoint_freq,
			replica_id_1, data.node[replica_id_1].server.server_ip,
			data.node[replica_id_1].server.port, replica_id_2,
			data.node[replica_id_2].server.server_ip,
			data.node[replica_id_2].server.port);

	return;
}

int elect_new_primary(int replica_id) {
	int clientfd = 0;
	int status = 0;
	char buf[MAX_LENGTH];

	memset(buf, 0, MAX_LENGTH);

	fill_message_for_primary_election(buf, replica_id);
	
	// printf("New primary %d\n Buf %s\n", replica_id, buf);

	clientfd = connect_to_server(&data.node[replica_id].factory);
	if (clientfd < 0) {
		printf("Connect failed: %s\n", strerror(errno));
		return -1;
	}

	status = write(clientfd, buf, MAX_LENGTH);
	if (status < 0) {
		printf("Write failed: %s\n", strerror(errno));
		close(clientfd);
		return -1;
	}

	close(clientfd);
	
	primary_replica_id = replica_id;
	return 0;
}

int send_change_status_to_server(int replica_id, rep_mode_t mode_rep,
						server_states_t server_state) {
	char buf[MAX_LENGTH];
	int clientfd = 0;
	int status = 0;

	memset(buf, 0, MAX_LENGTH);
	
	sprintf(buf, "Replica ID: %d\r\n"
			"Factory Req: %d\r\n"
			"REP MODE: %d\r\n"
			"Server State: %d\r\n\r\n",
			replica_id, CHANGE_STATE,
			mode_rep, server_state);

	// printf("Sending status change to %d\n  Buf %s\n", replica_id, buf);
	
	clientfd = connect_to_server(&data.node[replica_id].factory);
	if (clientfd < 0) {
		printf("Connect failed: %s\n", strerror(errno));
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

int instruct_primary_to_send_chkpt(int replica_id) {
	char buf[MAX_LENGTH];
	int clientfd = 0;
	int status = 0;

	memset(buf, 0, MAX_LENGTH);
	
	sprintf(buf, "Replica ID: %d\r\n"
			"Factory Req: %d\r\n\r\n",
			replica_id, SEND_CHKPT);

	// printf("Instruct primary to send chkpt %d\n  Buf %s\n", replica_id, buf);
	
	clientfd = connect_to_server(&data.node[replica_id].factory);
	if (clientfd < 0) {
		printf("Connect failed: %s\n", strerror(errno));
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

int handle_state(replication_manager_message fault_detector_ctx) {
    int ret = 0;
	int status = 0;

    pthread_mutex_lock(lock);
    // change internal state
    int replica_id = fault_detector_ctx.replica_id;
    data.node[replica_id].last_heartbeat = 1;

	// printf("Recieved message on RM Replica ID %d State %d\n",
	//				replica_id, fault_detector_ctx.state);

	if (fault_detector_ctx.state == RUNNING) {
		if (data.node[replica_id].state == FAULTED) {
			// Start up stage or recovery stage

			if (mode == 0) {
				// Passive mode

				if (replica_id != primary_replica_id) {
					// After the backup replica sends the first heartbeat,
					// Change it's status to BACKUP.

					status = send_change_status_to_server(replica_id, PASSIVE_REP, PASSIVE_BACKUP);
					if (status < 0) {
						printf("Sending state to backup failed\n");
						ret = -1;
					}
				} else {
					// After the primary replica comes up,
					// Change it's status to PRIMARY

					status = elect_new_primary(primary_replica_id);
					if (status < 0) {
						printf("Primary election failed\n");
						ret = -1;
					}
				}

				if (membership.needs_checkpoint[replica_id] == 1) {
					status = instruct_primary_to_send_chkpt(primary_replica_id);
					if (status < 0) {
						printf("Instructing primary to send checkpoint failed\n");
						return -1;
					}
				}
			} else if (mode == 1) {
				// Active mode

				status = send_change_status_to_server(replica_id, ACTIVE_REP, ACTIVE_RECOVER);
				if (status < 0) {
					printf("Sending state to backup failed\n");
					ret = -1;
				}

                status = elect_new_primary(primary_replica_id);
                if (status < 0) {
                    printf("Primary election failed\n");
                    ret = -1;
                }

                //quiesce them all servers
                status = quiesce_all();
				if (status < 0) {
					printf("send all quiesce request failed\n");
					ret = -1;
				}
                printf("sent quiesce all message\n");

                //send and apply checkpoint
				status = instruct_primary_to_send_chkpt(primary_replica_id);
				if (status < 0) {
					printf("Instructing primary to send checkpoint failed\n");
					ret = -1;
				}
                printf("sent checkpoint message\n");

                //stop quisce
                status = stop_quiesce_all();
				if (status < 0) {
					printf("send stop all quiesce request failed\n");
					ret = -1;
				}

                status = send_change_status_to_server(replica_id, ACTIVE_REP, ACTIVE_RUNNING);
				if (status < 0) {
					printf("Sending state to backup failed\n");
					ret = -1;
				}
			}
		}

		data.node[replica_id].state = fault_detector_ctx.state;
	
	} else if (fault_detector_ctx.state == FAULTED) {
		if (mode == 0) {
			// Passive mode

			if (primary_replica_id == -1) {
				// No primary replica. Elect new one

				if (restart_server(replica_id) != 0) {
					printf("Startup req could not be sent\n");
				} else {
					printf("Sent startup messaage to server %d\n", replica_id);
				}
				
				primary_replica_id = replica_id;

			} else if (replica_id == primary_replica_id) {
				// Primary replica faulted. Restart it.
				// Elect new replica and ask it to send
				// checkpoint

				if (restart_server(replica_id) != 0) {
					printf("Startup req could not be sent\n");
				} else {
					printf("Sent startup messaage to server %d\n", replica_id);
				}
				
				primary_replica_id = (replica_id + 1) % MAX_REPLICAS;
				status = elect_new_primary(primary_replica_id);
				if (status < 0) {
					printf("Primary election failed\n");
					ret = -1;
				}
			
				membership.needs_checkpoint[replica_id] = 1;	
			} else {
				// Backup replica faulted. Restart it.

				if (restart_server(replica_id) != 0) {
					printf("Startup req could not be sent\n");
				} else {
					printf("Sent startup messaage to server %d\n", replica_id);
				}
				
				membership.needs_checkpoint[replica_id] = 1;	
			}

		} else if (mode == 1) {
			// Active mode

			if (restart_server(replica_id) != 0) {
				printf("Startup req could not be sent\n");
			} else {
				printf("Sent startup messaage to server %d\n", replica_id);
            }

			if (primary_replica_id == -1) {
				// No primary replica. Elect new one
				primary_replica_id = replica_id;


			} else if (replica_id == primary_replica_id) {
				// Primary replica faulted. Restart it.
				// Elect new replica and ask it to send
				// checkpoint
				primary_replica_id = (replica_id + 1) % MAX_REPLICAS;
                status = elect_new_primary(primary_replica_id);
                if (status < 0) {
                    printf("Primary election failed\n");
                    ret = -1;
                }
			}
        } 
		data.node[replica_id].state = FAULTED;
	}

    // check if unstable state
    // take action based on that
    print_current_state_info(fault_detector_ctx);

    handle_current_state();
    pthread_mutex_unlock(lock);

    return ret;
}

int handle_current_state() {
    int replica_iter, active_count, inactive_count, startup_req;
    active_count = 0;
    inactive_count = 0;
    startup_req = 0;

    for (replica_iter = 0; replica_iter < data.num_replicas; replica_iter++) {

        if (data.node[replica_iter].state == RUNNING) {
            active_count++;
        }

        else if (data.node[replica_iter].state == FAULTED) { 
            inactive_count++;
        }

        else if (data.node[replica_iter].state == SENT_STARTUP_REQ) {
            inactive_count++;
            startup_req++;
        }

        else {
            printf("Unhandled state :/\n");
        }
    }

    return 0;
}

int restart_server(int replica_id) {
    int clientfd = 0;
    int status = 0;

    char buf[MAX_LENGTH];

    sprintf(buf, "Replica ID: %d\r\n"
                 "Factory Req: %d\r\n"
				 "Server IP: %s\r\n"
				 "Server Port: %d\r\n\r\n",
            replica_id, STARTUP, data.node[replica_id].server.server_ip,
			data.node[replica_id].server.port);

	// printf("Starting server %d Buf %s", replica_id, buf);

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
        CFG_SIMPLE_STR("replication_mode", &data.mode_str),
        CFG_SIMPLE_STR("configuration_manager_ip", &data.server_ip),
        CFG_SIMPLE_STR("configuration_manager_port", &data.port),
        CFG_SIMPLE_INT("configuration_manager_verbose", &verbose),
        CFG_SIMPLE_INT("checkpoint_frequency", &data.checkpoint_freq),
        CFG_SIMPLE_INT("configuration_manager_num_replicas",
                       &(data.num_replicas)),
        CFG_SIMPLE_STR("factory_0_manager_ip", &data.node[0].factory_ip),
        CFG_SIMPLE_STR("factory_0_manager_port", &data.node[0].factory_port),
        CFG_SIMPLE_STR("factory_1_manager_ip", &data.node[1].factory_ip),
        CFG_SIMPLE_STR("factory_1_manager_port", &data.node[1].factory_port),
        CFG_SIMPLE_STR("factory_2_manager_ip", &data.node[2].factory_ip),
        CFG_SIMPLE_STR("factory_2_manager_port", &data.node[2].factory_port),
        CFG_SIMPLE_STR("server_0_ip", &data.node[0].server_ip),
        CFG_SIMPLE_STR("server_0_port", &data.node[0].server_port),
        CFG_SIMPLE_STR("server_1_ip", &data.node[1].server_ip),
        CFG_SIMPLE_STR("server_1_port", &data.node[1].server_port),
        CFG_SIMPLE_STR("server_2_ip", &data.node[2].server_ip),
        CFG_SIMPLE_STR("server_2_port", &data.node[2].server_port),
        CFG_SIMPLE_INT("hbt_timeout", &(data.hbt_timeout)), CFG_END()};

    cfg_t *cfg;
    cfg = cfg_init(opts, 0);

    if ((parse_ret = cfg_parse(cfg, path)) != CFG_SUCCESS) {
        if (parse_ret == CFG_FILE_ERROR) {
            printf("REPLICATION MANAGER config file not found\n");
        } else {
            printf("REPLICATION MANAGER config file parse error\n");
        }
        return -1;
    }
    strncpy(data.node[0].factory.server_ip, data.node[0].factory_ip, 20);
    data.node[0].factory.port = atoi(data.node[0].factory_port);
    strncpy(data.node[1].factory.server_ip, data.node[1].factory_ip, 20);
    data.node[1].factory.port = atoi(data.node[1].factory_port);
    strncpy(data.node[2].factory.server_ip, data.node[2].factory_ip, 20);
    data.node[2].factory.port = atoi(data.node[2].factory_port);
	strncpy(data.node[0].server.server_ip,
				data.node[0].server_ip, 20);
	data.node[0].server.port = atoi(data.node[0].server_port);
	strncpy(data.node[1].server.server_ip,
				data.node[1].server_ip, 20);
	data.node[1].server.port = atoi(data.node[1].server_port);
	strncpy(data.node[2].server.server_ip,
				data.node[2].server_ip, 20);
	data.node[2].server.port = atoi(data.node[2].server_port);

	if (!strncmp(data.mode_str, "passive", strlen("passive"))) {
		printf("\nReplication mode: Passive\n\n");
		mode = 0;
	} else if(!strncmp(data.mode_str, "active", strlen("active"))) {
		printf("\nReplication mode: Active\n\n");
		mode = 1;
	}

    return 0;
}

static int
parse_fault_detector_kv(replication_manager_message *fault_detector_ctx,
                        char *key, char *value) {
    if (strcmp(key, "Replica ID") == 0) {
        if (str_to_int(value, &fault_detector_ctx->replica_id) != 0) {
            printf("replica_id conversion failed; Malformed req!!!\n");
            return -1;
        }
    } else if (strcmp(key, "Message Type") == 0) {
        if (str_to_int(value, (int *)&fault_detector_ctx->state) != 0) {
            printf("Msg type Conversion failed!!!\n");
            return -1;
        }
    }
    return 0;
}

static void init_gfd_hbt() {
    int replica_iter;
    pthread_t tid;

    for (replica_iter = 0; replica_iter < data.num_replicas; replica_iter++) {
        data.node[replica_iter].last_heartbeat = time(NULL);
    }
    lock = malloc(sizeof(pthread_mutex_t));
    if (pthread_mutex_init(lock, NULL) != 0) {
        exit(-1);
    }
    pthread_create(&tid, NULL, gfd_hbt, NULL);
    return;
}

static void *gfd_hbt() {

    pthread_detach(pthread_self());

    int replica_iter;
    while (1) {
        pthread_mutex_lock(lock);
        for (replica_iter = 0; replica_iter < data.num_replicas;
             replica_iter++) {
            if (data.node[replica_iter].last_heartbeat == 0) {
                printf("REPLICA ID %d HEARTBEAT TIMEOUT conisdered dead\n",
                       replica_iter);
                membership.server_status[replica_iter] = 0;
                // data.node[replica_iter].state  = FAULTED;
            }
            data.node[replica_iter].last_heartbeat = 0;
        }
        pthread_mutex_unlock(lock);
        sleep(data.hbt_timeout);
    }
    return NULL; // should not exit
}

static int quiesce_message(int replica_id) {
    char buf[MAX_LENGTH];
	int clientfd = 0;
	int status = 0;

	memset(buf, 0, MAX_LENGTH);
	
	sprintf(buf, "Replica ID: %d\r\n"
			"Factory Req: %d\r\n\r\n",
			replica_id, QUIESCE_BEGIN);

	printf("Instruct primary to send chkpt %d\n  Buf %s\n", replica_id, buf);
	
	clientfd = connect_to_server(&data.node[replica_id].factory);
	if (clientfd < 0) {
		printf("Connect failed: %s\n", strerror(errno));
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

static int quiesce_stop_message(int replica_id) {
    char buf[MAX_LENGTH];
	int clientfd = 0;
	int status = 0;

	memset(buf, 0, MAX_LENGTH);
	
	sprintf(buf, "Replica ID: %d\r\n"
			"Factory Req: %d\r\n\r\n",
			replica_id, QUIESCE_STOP);

	printf("Instruct primary to send chkpt %d\n  Buf %s\n", replica_id, buf);
	
	clientfd = connect_to_server(&data.node[replica_id].factory);
	if (clientfd < 0) {
		printf("Connect failed: %s\n", strerror(errno));
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

static int quiesce_all() {
    int ret = 0;
    for(int i = 0; i < MAX_REPLICAS; i++) {
        if(data.node[i].state == RUNNING) {
            ret += quiesce_message(i);
        }
    }
    return ret;
}

static int stop_quiesce_all() {
    int ret = 0;
    for(int i = 0; i < MAX_REPLICAS; i++) {
        if(data.node[i].state == RUNNING) {
            ret += quiesce_stop_message(i);
        }
    }
    return ret;
}
