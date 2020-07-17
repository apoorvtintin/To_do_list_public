/* @author Apoorv Gupta <apoorvgupta@hotmail.co.uk> */
/* @brief functions for methods declared in storage.h */

/* HEADER FILES */
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "c_s_iface.h"
#include "db.h"
#include "storage.h"

/* DEFINATIONS AND GLOBAL VARIABLES */
union hashstr2key {
    char hash[8];
    uint64_t key;
};
/* PRIVATE functins */
static uint64_t generate_key(uint8_t *raw_data, int client_id,
                             uint64_t data_len) {
    // buffer
    char raw_buffer[TASK_LENGTH];
    // char buffer[5];
    union hashstr2key instance;
    // uint64_t ret_key;
    SHA_CTX context;

    if (data_len < TASK_LENGTH) {
        snprintf(raw_buffer, TASK_LENGTH, "%d", client_id);
    }
    memcpy(raw_buffer + 5, raw_data, data_len);
    if (!SHA1_Init(&context))
        return -1;

    if (!SHA1_Update(&context, (unsigned char *)raw_data, data_len + 5))
        return -1;

    if (!SHA1_Final(instance.hash, &context))
        return -1;
    // printf("SHA1 %lu\n", instance.key);
    return instance.key;
}

/* PUBLIC APIs */
int storage_init() { return hash_table_init(); }

void storage_deinit() { hash_table_deinit(); }

void export_db(char *file) {
    export_db_internal(file);
    return;
}

void print_state() {
    print_state_internal();
    return;
}

int import_db(char *file) {
    printf("Import DB\n");
    import_db_internal(file);
    return 0;
}

int handle_storage(client_ctx_t *client_ctx) {
    client_request_t *req = &client_ctx->req;
    msg_type_t msg_type = req->msg_type;
    int ret = 0;
    uint8_t *buffer;
    uint64_t old_key;

    switch (msg_type) {
    case MSG_ADD:
        req->hash_key =
            generate_key(req->task, client_ctx->client_id, req->task_len);
        ret = hash_table_insert(req->hash_key, req->task, req->task_len);
        print_htable();
        break;
    case MSG_MODIFY:
        hash_table_modify(req->hash_key, req->task_status);
        print_htable();
        break;
    case MSG_REMOVE:
        ret = hash_table_delete(req->hash_key);
        print_htable();
        break;
    case MSG_GET_ALL:
        buffer = calloc(hash_table_get_dat_size(), sizeof(uint8_t));
        ret = hash_table_get_all(buffer);
        break;
    case INVALID:
    case MSG_HEARTBEAT:
        ret = 0;
        break;
    case MSG_CHK_PT: {
        ret = import_db(client_ctx->req.filename);
        ret = 0;
        remove(client_ctx->req.filename);
        print_state();
    }
    default:
        ret = -1;
    }
    return ret;
}
