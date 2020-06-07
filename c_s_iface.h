/*
 * This file contains common data structures required 
 * for the client and server
 */
#ifndef _C_S_IFACE_H
#define _C_S_IFACE_H

#include <stdint.h>

/* Flags for mod_flags */
#define MOD_FLAGS_TASK_STRING_MODIFIED		0x1
#define MOD_FLAGS_DATE_MODIFIED				0x2
#define MOD_FLAGS_STATUS_MODIFIED			0x4

enum t_status {
	TASK_NOT_DONE = 0,
	TASK_DONE = 1,
};

typedef enum _msg_type
{
    INVALID,
    MSG_ADD,
    MSG_MODIFY,
    MSG_GET_ALL,
    MSG_REMOVE
} msg_type_t;

struct message_add {
    uint64_t msg_len;
    msg_type_t msg_type;
	char task[1024];
	char task_date[32];
	enum t_status task_status;
};

struct message_modify {
    uint64_t msg_len;
    msg_type_t msg_type;
	char task[1024];
	char new_task[1024];
	char new_date[32];
	enum t_status new_task_status;
	int mod_flags;					// This flag tells us which fields are to be modified
};

struct message_get_all {
    uint64_t msg_len;
    msg_type_t msg_type;
	char dummy_for_now;
};

struct message_remove {
    uint64_t msg_len;
    msg_type_t msg_type;
	char task[1024];
};

struct message_storage
{
    uint64_t msg_len;
    msg_type_t msg_type;
    char task[sizeof(struct message_modify)];
    // make sure that this corresponds to the longest message.

};
#endif
