
/*
 * @file worker.c
 * @author Mohammed Sameer <sameer.2897@gmail.com>
 * @brief this file has the implementation of woker thread arch for
 * log execution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include "log.h"
#include "worker.h"
#include "c_s_iface.h"
#include "server.h"
#include "state.h"

// Global Static Data
static server_log_t *gs_server_log = NULL;
static volatile int is_prune = 0;
static volatile int in_quiesence = 0;
static hdl_nrl_t hdl_nrl;
static hdl_ctrl_t hdl_ctrl;

void put_in_quiesence() { in_quiesence = 1; }

void remove_from_quiesence() { in_quiesence = 0; }

void set_worker_prune() { is_prune = 1; }

bool should_nrl_thrd_sleep() {
    if ((get_mode() == PASSIVE_REP) && (get_state() == PASSIVE_BACKUP)) {
        return true;
    }
    return false;
}
void free_msg(client_ctx_t *client_ctx) {
    if (client_ctx->fd >= 0) {
        close(client_ctx->fd);
        client_ctx->fd = -1;
    }
    if (client_ctx) {
        free(client_ctx);
        client_ctx = NULL;
    }
}
void *run(void *argp) {
    pthread_detach(pthread_self());
    server_log_t *svr = (server_log_t *)gs_server_log;
    int prio;
    int64_t prio1 = (int64_t)argp;
    prio = (int)prio1;
    while (1) {
        if ((prio == NORMAL) && (is_prune == 1)) {
            log_node_t *node1;
            while ((node1 = dequeue(svr, NORMAL)) != NULL) {
                client_ctx_t *ctx = node1->val;
                if (ctx->req.msg_type != MSG_CHK_PT) {
                    free_msg(ctx);
                    free(node1);
                } else {
                    is_prune = 0;
                    hdl_nrl(node1->val);
                    free(node1);
                    continue;
                }
            }
        }
        if ((!is_prune && should_nrl_thrd_sleep()) && (prio == NORMAL)) {
            sleep(1);
            continue;
        }
        size_t n_count = (prio == NORMAL) ? get_count(svr, NORMAL) : 0,
               c_count = (prio == CONTROL) ? get_count(svr, CONTROL) : 0;
        if ((prio == NORMAL) && (get_mode() == PASSIVE_REP) &&
            (get_state() == PASSIVE_PREPRIMARY) && (n_count == 0)) {
            set_state(PASSIVE_PRIMARY);
        }
        if ((n_count == 0) && (c_count == 0)) {
            // sleep for 1 sec;
            sleep(1);
            continue;
        } else {
            if (c_count != 0) {
                // handle control message.
                log_node_t *node = dequeue(svr, CONTROL);
                if ((node != NULL) && (node->val != NULL)) {
                    hdl_ctrl(node->val);
                } else {
                    fprintf(stderr, "ctrl dequeue error\n");
                    continue;
                }
                free(node);
            }
            if (n_count != 0 && in_quiesence == 0) {
                // handle normal message.
                log_node_t *node = dequeue(svr, NORMAL);
                if ((node != NULL) && (node->val != NULL)) {
                    hdl_nrl(node->val);
                } else {
                    fprintf(stderr, "nrl dequeue error\n");
                    continue;
                }
                free(node);
            }
            if (c_count && n_count) {
                fprintf(stderr,
                        "Error!, we should never have reached here! %d\n",
                        prio);
            }
        }
    }
}

int start_worker_threads(server_log_t *svr, hdl_nrl_t f1, hdl_ctrl_t f2) {
    if (svr == NULL) {
        return -1;
    }
    gs_server_log = svr;
    hdl_nrl = f1;
    hdl_ctrl = f2;
    pthread_t th_id[2];
    if (pthread_create(&th_id[0], NULL, run, (void *)NORMAL) != 0) {
        fprintf(stderr, "NORMAL worker theread creation failed!!!");
        exit(0);
    }
    if (pthread_create(&th_id[1], NULL, run, (void *)CONTROL) != 0) {
        fprintf(stderr, "NORMAL worker theread creation failed!!!");
        exit(0);
    }
    fprintf(stderr, "Started worker thread with TH_ID = %x, %x\n",
            (uint32_t)th_id[0], (uint32_t)th_id[1]);
    return 0;
}
