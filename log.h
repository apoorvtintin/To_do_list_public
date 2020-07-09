/*
 * @file log.h
 * @author Mohammed Sameer <sameer.2897@gmail.com>
 * @brief this is header file for message log for distributed replication
 * app.
 */
#ifndef _LOG_H_
#define _LOG_H_

#include <pthread.h>

typedef enum _log_msg_type { INVALID_LOG, NORMAL, CONTROL } log_msg_type;

typedef struct _log_node log_node_t;

typedef struct _log_node {
    void *val;
    log_node_t *next;
    log_node_t *prev;

} log_node_t;

typedef struct _server_log {
    log_node_t *nrl_head;
    log_node_t *nrl_tail;
    size_t nrl_count;
    size_t NRL_Q_LIMIT;

    log_node_t *ctrl_head;
    log_node_t *ctrl_tail;
    size_t ctrl_count;
    size_t CTRL_Q_LIMIT;
#ifdef M_THRD_LQ
    pthread_mutex_t q_mutex;
#endif
} server_log_t;

// Function definitions
//

void init_log_queue(server_log_t *svr, size_t nrl_limit, size_t ctrl_limit);

log_node_t *dequeue(server_log_t *svr, log_msg_type m_type);

int enqueue(server_log_t *svr, log_node_t *node, log_msg_type m_type);

#endif
