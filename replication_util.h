
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

// Definitions
#define MAX_REPLICAS 3

// Structures
enum replica_state { RUNNING = 0, FAULTED = 1, SENT_STARTUP_REQ = 2 };

typedef struct replica_node_t {
    int replica_id;
    enum replica_state state;
    char *factory_ip;
    char *factory_port;
    struct server_info factory;
    time_t last_heartbeat;
} replica_node;

typedef struct rep_manager_data_t {
    long num_replicas;
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
    NO_ACTION = 1,
};

typedef struct factory_message_t {
    int replica_id;
    enum factory_req req;
} factory_message;