/* @author Apoorv Gupta <apoorvgupta@hotmail.co.uk> */
/* @brief functions for methods declared in storage.h */

/* HEADER FILES */
//#include "/usr/local/opt/openssl/include/openssl/sha.h"

#include "storage.h" 
#include "db.h"
#include "c_s_iface.h"

/* DEFINATIONS AND GLOBAL VARIABLES */



/* PRIVATE functins */


/* PUBLIC APIs */
uint64_t generate_key(uint8_t *raw_data, int client_id) {

}

int storage_init() {
    return hash_table_init();
}

void storage_deinit() {
    hash_table_deinit();
}

int handle_storage(client_ctx_t *client_ctx) {
    client_request_t *req = &client_ctx->req;
    msg_type_t msg_type = req->msg_type;
    int ret = 0;

    switch(msg_type) {
        case MSG_ADD:
            req->hash_key = generate_key(req->task, client_ctx->client_id);
            ret = hash_table_insert(req->hash_key, req->task, req->len);
        break;
        case MSG_MODIFY:
            uint64_t old_key = req->hash_key;
            req->hash_key = (req->task, client_ctx->client_id));
            ret = hash_table_modify(old_key, req->hash_key, req->task, req->len);
        break;
        case MSG_REMOVE:
            ret = hash_table_delete(req->hash_key);
        break;
        case MSG_GET_ALL:
            uint8_t *buffer = calloc(hash_table_get_dat_size(), sizeof(uint8_t));
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