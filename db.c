
/* @author Apoorv Gupta <apoorvgupta@hotmail.co.uk> */

/* HEADER FILES */
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static uint8_t *search_list(uint32_t hash, uint64_t key) {
    data_point *temp = htable->data[hash];
    while (temp != NULL) {
        if (key == temp->key) {
            return temp->raw_data;
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
    uint8_t *raw_data = search_list(hash, key);
    if (raw_data == NULL) {
        // data not found
        return -1;
    }
    snprintf(buffer, MAX_DATA_LEN, "task: %s \r\n", raw_data);
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

int hash_table_modify(uint64_t key, uint64_t nkey, uint8_t *new_data,
                      uint64_t nlen) {
    uint32_t hash = hash_func(key);
    if (hash_table_delete(key) == -1) {
        printf("ERROR: data not found");
        // data not found
        return 0;
    }
    if (hash_table_insert(nkey, new_data, nlen)) {
        printf("ERROR: could not be inserted");
        return -1;
    }
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
