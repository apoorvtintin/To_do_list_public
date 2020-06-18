/*
 * This file contains common data structures required
 * for the client and server
 */
#ifndef _C_S_IFACE_H
#define _C_S_IFACE_H

#include <stdint.h>

typedef struct sockaddr SA;

/* Flags for mod_flags */
#define MOD_FLAGS_TASK_STRING_MODIFIED 0x1
#define MOD_FLAGS_DATE_MODIFIED 0x2
#define MOD_FLAGS_STATUS_MODIFIED 0x4

#define MAX_REQ_FIELDS 6
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
    MSG_HEARTBEAT
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
};

struct message_response {
    uint64_t hash_key;
    int client_id;
    char status[TASK_LENGTH];
};

#define MAXMSGSIZE sizeof(struct message_modify)

typedef struct _client_request {
    msg_type_t msg_type;
    char task[1024];
    char date[32];
    enum t_status task_status;
    int mod_flags;
    uint64_t task_len;
    uint64_t hash_key;
} client_request_t;
#endif
