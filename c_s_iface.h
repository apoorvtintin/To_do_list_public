/*
 * This file contains common data structures required 
 * for the client and server
 */

/* Flags for mod_flags */
#define MOD_FLAGS_TASK_STRING_MODIFIED		0x1
#define MOD_FLAGS_DATE_MODIFIED				0x2
#define MOD_FLAGS_STATUS_MODIFIED			0x4

enum t_status {
	TASK_NOT_DONE = 0,
	TASK_DONE = 1,
};

struct message_add {
	char task[1024];
	char task_date[32];
	enum t_status task_status;
};

struct message_modify {
	char task[1024];
	char new_task[1024];
	char new_date[32];
	enum t_status new_task_status;
	int mod_flags;					// This flag tells us which fields are to be modified
};

struct message_get_all {
	char dummy_for_now;
};

struct message_remove {
	char task[1024];
};
