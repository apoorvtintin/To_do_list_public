

/* @author Apoorv Gupta <apoorvgupta@hotmail.co.uk> */

/* HEADER FILES */
#include "c_s_iface.h"
#include <stdint.h>

/* DEFINATIONS */
#define SHA_40_len 4
#define hash_table_len 13
// prime close to 1024
#define MAX_DATA_LEN 512

/* DATABASE STRUCTURES */

typedef struct data_point_ {
    uint64_t key;
    uint8_t raw_data[MAX_DATA_LEN];
    uint64_t data_len;
    enum t_status task_status;
    struct data_point_ *next;
} data_point;

typedef struct hash_table_ {
    data_point *data[hash_table_len];
    uint64_t data_size;
} hash_table;

/* DATABASE PUBLIC API FUNCTIONS */

int hash_table_insert(uint64_t key, uint8_t *data, uint64_t len);

int hash_table_modify(uint64_t key, enum t_status task_status);

int hash_table_delete(uint64_t key);

int hash_table_get(uint64_t key, uint8_t *buffer);

int hash_table_get_all(uint8_t *buffer);

int hash_table_get_dat_size();

int hash_table_init();

void hash_table_deinit();

void export_db_internal(char *file);

void import_db_internal(char *file);

/* EOF */
