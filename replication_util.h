
/* @author Apoorv Gupta <apoorvgupta@hotmail.co.uk> */

/* HEADER FILES */

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

#include "state.h"

// Definitions
#define MAX_REPLICAS 3

// Structures
enum replica_state {
	RUNNING = 0,
	FAULTED = 1,
	SENT_STARTUP_REQ = 2,
};

typedef struct replica_node_t {
    int replica_id;
    enum replica_state state;
    char *factory_ip;
    char *factory_port;
	char *server_ip;
	char *server_port;
    struct server_info factory;
	struct server_info server;
    time_t last_heartbeat;
} replica_node;

typedef struct rep_manager_data_t {
    long num_replicas;
	long checkpoint_freq;
	char *mode_str;
    char *server_ip;
    char *port;
    replica_node node[MAX_REPLICAS];
    time_t hbt_timeout;
} rep_manager_data;

typedef struct replication_manager_t {
    struct server_info rep_manager;
    int replica_id;
} replication_manager;

typedef struct replication_manager_message_t {
    int replica_id;
    enum replica_state state;
} replication_manager_message;

typedef struct factory_data_t {
    long num_starts;
    long replica_id;
    char *server_ip;
    char *port;
    char *spawned_server_ip;
    char *spawned_server_port;
    char *replication_manager_ip;
    char *replication_manager_port;
    char *lfd_heartbeat;
} factory_data;

enum factory_req {
    STARTUP = 0,
	CHANGE_STATE = 1,
	SEND_CHKPT = 2,
    QUIESCE_BEGIN = 3,
    QUIESCE_STOP = 4,
    NO_ACTION = 5
};

typedef struct factory_message_t {
    int replica_id;
	int bkp_replica_id_1;
	int bkp_replica_id_2;
    enum factory_req req;
	rep_mode_t mode_rep;
	int checkpoint_freq;
	server_states_t server_state;
	struct server_info server_arr[MAX_REPLICAS];
} factory_message;
