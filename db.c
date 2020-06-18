
/* @author Apoorv Gupta <apoorvgupta@hotmail.co.uk> */

/* HEADER FILES */
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "c_s_iface.h"

/* DEFINATIONS */

/* GLOBAL VARIABLES */
hash_table *htable;

/* PRIVATE FUNCTIONS */
static uint32_t hash_func(uint64_t val) { return val % hash_table_len; }

static int insert_data_point(uint32_t hash, data_point *new) {
    if (new == NULL)
        return -1;
    if (htable->data[hash] == NULL) {
        htable->data[hash] = new;
    } else {
        data_point *temp = htable->data[hash];
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = new;
    }

    return 0;
}

static data_point *search_list(uint32_t hash, uint64_t key) {
    data_point *temp = htable->data[hash];
    while (temp != NULL) {
        if (key == temp->key) {
            return temp;
        }
    }
    return NULL;
}

/* PUBLIC APIs */
int hash_table_insert(uint64_t key, uint8_t *data, uint64_t len) {
    if (key == 0) {
        printf("ERROR: key is NULL");
        return -1;
    }
    uint32_t hash = hash_func(key);
    // printf("hash value is %u\n", hash);
    data_point *new = malloc(sizeof(data_point));
    if (new == NULL) {
        return -1;
    }
    new->key = key;
    new->data_len = len;
    memcpy(new->raw_data, data, len);
    new->next = NULL;
    htable->data_size += len;

    return insert_data_point(hash, new);
}

int hash_table_get(uint64_t key, uint8_t *buffer) {
    uint32_t hash = hash_func(key);
    data_point * task = search_list(hash, key);
    if (task == NULL) {
        // data not found
        return -1;
    }
    snprintf(buffer, MAX_DATA_LEN, "task: %s status: %d \r\n", task->raw_data, 
            task->task_status);
    return 0;
}

int hash_table_delete(uint64_t key) {
    uint32_t hash = hash_func(key);
    // printf("hash value is %u\n", hash);
    data_point *temp = htable->data[hash];
    data_point *prev = temp;
    while (temp != NULL) {
        if (key == temp->key) {
            if (temp == htable->data[hash]) {
                htable->data[hash] = temp->next;
            } else {
                prev->next = temp->next;
            }
            htable->data_size -= temp->data_len;
            free(temp);
            return 0;
        }
        prev = temp;
        temp = temp->next;
    }
    return -1;
}

int hash_table_modify(uint64_t key, enum t_status task_status) {
    uint32_t hash = hash_func(key);
    data_point *task = search_list(hash, key);
    if (task == NULL) {
        printf("ERROR: data not found");
        // data not found
        return -1;
    } 
    task->task_status = task_status;
    return 0;
}

int hash_table_get_all(uint8_t *buffer) {
    data_point *temp;
    uint8_t *buftemp = buffer;
    int index;
    for (index = 0; index < hash_table_len; index++) {
        temp = htable->data[index];
        while (temp != NULL) {
            buftemp +=
                snprintf(buftemp, MAX_DATA_LEN, "hash_key: %ld task: %s \r\n",
                         temp->key, temp->raw_data);
            temp = temp->next;
        }
    }
    return 0;
}

int hash_table_init() {
    int index;
    htable = malloc(sizeof(htable));
    htable->data_size = 0;
    for (index = 0; index < hash_table_len; index++) {
        htable->data[index] = NULL;
    }
}

void hash_table_deinit() {
    data_point *cur, *temp;
    int index;
    for (index = 0; index < hash_table_len; index++) {
        temp = htable->data[index];
        while (temp != NULL) {
            cur = temp;
            temp = temp->next;
            free(cur);
        }
    }
    free(htable);
}

int hash_table_get_dat_size() { return htable->data_size; }

void print_htable() {
    int index;
    data_point *temp;
    for (index = 0; index < hash_table_len; index++) {
        temp = htable->data[index];
        // printf("next index %d \n", index);
        while (temp != NULL) {
            // printf("task %s, key %lu\n", temp->raw_data, temp->key);
            temp = temp->next;
        }
    }
}

/* EOF */
