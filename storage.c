/* @author Apoorv Gupta <apoorvgupta@hotmail.co.uk> */
/* @brief functions for methods declared in storage.h */

/* HEADER FILES */
#include <stdlib.h>
#include "openssl/sha.h"

#include "storage.h" 
#include "db.h"
#include "c_s_iface.h"

/* DEFINATIONS AND GLOBAL VARIABLES */

SHA_CTX sha_context;

/* PRIVATE functins */
static uint64_t generate_key(uint8_t *raw_data, int client_id, uint64_t data_len) {
	//buffer
	uint8_t hash[5] = SHA1_Transform(sha_context, raw_data);	
}

/* PUBLIC APIs */
int storage_init() {
	SHA1_INIT(sha_context);
    return hash_table_init();
}

void storage_deinit() {
    hash_table_deinit();
}

int handle_storage(client_ctx_t *client_ctx) {
    client_request_t *req = &client_ctx->req;
    msg_type_t msg_type = req->msg_type;
    int ret = 0;
	uint8_t *buffer;
    uint64_t old_key;

    switch(msg_type) {
        case MSG_ADD:
            req->hash_key = generate_key(req->task, client_ctx->client_id);
            ret = hash_table_insert(req->hash_key, req->task, req->task_len);
        break;
        case MSG_MODIFY:
            old_key = req->hash_key;
            req->hash_key = generate_key(req->task, client_ctx->client_id);
            ret = hash_table_modify(old_key, req->hash_key, req->task, req->task_len);
        break;
        case MSG_REMOVE:
            ret = hash_table_delete(req->hash_key);
        break;
        case MSG_GET_ALL:
            buffer = calloc(hash_table_get_dat_size(), sizeof(uint8_t));
            ret = hash_table_get_all(buffer);
        break;
        case INVALID:
        case MSG_HEARTBEAT:
            ret = 0;
        default:
            ret = -1;
    }
    return ret;
}
