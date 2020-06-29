
/*
 * @file worker.c
 * @author Mohammed Sameer <sameer.2897@gmail.com>
 * @brief this file has the implementation of woker thread arch for
 * log execution.
 */
#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "worker.h"
static server_log_t *gs_server_log = NULL;


static hdl_nrl_t hdl_nrl;
static hdl_ctrl_t hdl_ctrl;



void *run(void *argp)
{
    pthread_detach(pthread_self());
    server_log_t *svr = (server_log_t *) gs_server_log;
    int prio = (int) argp;
    while(1)
    {
        size_t n_count = (prio == NORMAL) ? get_count(svr, NORMAL) :0, 
               c_count = (prio == CONTROL) ? get_count(svr, CONTROL):0;

        if((n_count == 0) && (c_count == 0))
        {
            // sleep for 1 sec;
            sleep(1);
            continue;
        }
        else
        {
            if(c_count != 0)
            {
                // handle control message.
                log_node_t* node =  dequeue(svr, CONTROL);
                if((node != NULL) && (node->val != NULL))
                {
                    hdl_ctrl(node->val);
                }
                else
                {
                    fprintf(stderr, "ctrl dequeue error\n");
                    continue;
                }
                free(node);
            }
            if(n_count != 0)
            {
                //handle normal message.
                log_node_t* node =  dequeue(svr, NORMAL);
                if((node != NULL) && (node->val != NULL))
                {
                    hdl_nrl(node->val);
                }
                else
                {
                    fprintf(stderr, "nrl dequeue error\n");
                    continue;
                }
                free(node);
            }
            else
            {
                fprintf(stderr, "Error!, we should never have reached here!\n");
            }
        }
    }
}

int start_worker_threads(server_log_t *svr, hdl_nrl_t f1, 
        hdl_ctrl_t f2)
{
    if(svr == NULL)
    {
        return -1;
    }
    gs_server_log = svr;
    hdl_nrl = f1;
    hdl_ctrl = f2;
    pthread_t th_id[2];
    if (pthread_create(&th_id[0], NULL, run,
                       (void *) NORMAL) != 0) {
        fprintf(stderr, "NORMAL worker theread creation failed!!!");
        exit(0);
    }
    if (pthread_create(&th_id[1], NULL, run,
                       (void *)CONTROL) != 0) {
        fprintf(stderr, "NORMAL worker theread creation failed!!!");
        exit(0);
    }
    fprintf(stderr, "Started worker thread with TH_ID = %x, %x\n", 
            th_id[0], th_id[1]);
    return 0;
}
