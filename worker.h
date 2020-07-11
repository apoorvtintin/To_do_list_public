/*
 * @file worker.h
 * @author Mohammed Sameer <sameer.2897@gmail.com>
 * @brief this file includes interface functions for worker thread arch
 * implementation.
 */
#ifndef _WORKER_H_
#define _WORKER_H_

typedef void *(*hdl_nrl_t)(void *);
typedef void *(*hdl_ctrl_t)(void *);

int start_worker_threads(server_log_t *svr, hdl_nrl_t f1, hdl_ctrl_t f2, 
        int passive);

#endif
