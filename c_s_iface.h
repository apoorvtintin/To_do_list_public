/*
 * This file contains common data structures required
 * for the client and server
 */
#ifndef _C_S_IFACE_H
#define _C_S_IFACE_H

#include <stdint.h>

#include "state.h"

typedef struct sockaddr SA;

/* Flags for mod_flags */
#define MOD_FLAGS_TASK_STRING_MODIFIED 0x1
#define MOD_FLAGS_DATE_MODIFIED 0x2
#define MOD_FLAGS_STATUS_MODIFIED 0x4

#define MAX_REQ_FIELDS 10
#define MAX_LENGTH 8192
/* MAX_LEN-1 in string for use in scanf */
#define MAX_LENGTH_STR "8191"

#define TASK_LENGTH 1024
#define DATE_LENGTH 32

enum t_status {
    TASK_NOT_DONE = 0,
    TASK_DONE = 1,
};

typedef enum _msg_type {
    INVALID,
    MSG_ADD,
    MSG_MODIFY,
    MSG_GET_ALL,
    MSG_REMOVE,
    MSG_HEARTBEAT,
    MSG_CHK_PT,
	MSG_REP_MGR
} msg_type_t;

struct message_add {
    char task[TASK_LENGTH];
    char task_date[DATE_LENGTH];
    enum t_status task_status;
};

struct message_modify {
    char task[TASK_LENGTH];
    char new_task[TASK_LENGTH];
    char new_date[DATE_LENGTH];
    enum t_status new_task_status;
    int mod_flags; // This flag tells us which fields are to be modified
};

struct message_get_all {
    char dummy_for_now;
};

struct message_remove {
    char task[TASK_LENGTH];
    int req_no;
};

struct message_response {
    uint64_t hash_key;
    int client_id;
    char status[TASK_LENGTH];
    int req_no;
};

#define MAXMSGSIZE sizeof(struct message_modify)
typedef struct _payload
{
    char *data;
    int64_t size;
} payload_t;
#define PAYLOAD_INITIALISER {NULL,0}



struct server_info {
    char server_ip[1024];
    int port;
};
#define SERVER_INFO_INITIALISER {{0}, 0}

typedef struct _bsvr_ctx
{
    int fd;
    struct server_info info;
    unsigned int server_id;
} bsvr_ctx;
#define BSVR_CTX_INITIALISER {-1, SERVER_INFO_INITIALISER, -1}

typedef struct _rep_mgr_msg
{
    rep_mode_t rep_mode;
    server_states_t server_state;
    bsvr_ctx bckup_svr[2];
} rep_mgr_msg_t;
#define REP_MGR_MSG_INITIALISER {0,0,{BSVR_CTX_INITIALISER}}


typedef struct _client_request {
    msg_type_t msg_type;
    char task[1024];
    char date[32];
    enum t_status task_status;
    int mod_flags;
    uint64_t task_len;
    uint64_t hash_key;
    uint64_t req_no;
    payload_t payload;
    char filename[1024];
    rep_mgr_msg_t rep_mgr_msg;
} client_request_t;
#define CLIENT_REQUEST_INITIALISER {0,{0}, {0},0,0,0,0,0,PAYLOAD_INITIALISER,{0}, REP_MGR_MSG_INITIALISER}

#endif
