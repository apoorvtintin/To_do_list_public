/*
 * @file log.c
 * @author Mohammed Sameer <sameer.2897@gmail.com>
 * @brief This file implements the distributed log for distributed replication
 * framework.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"


void init_log_queue(server_log_t *svr, size_t nrl_limit, size_t ctrl_limit)
{
    if(svr == NULL)
    {
        return;
    }
    memset(svr, 0, sizeof(server_log_t));
    svr->NRL_Q_LIMIT = nrl_limit;
    svr->CTRL_Q_LIMIT = ctrl_limit;
#ifdef M_THRD_LQ
    pthread_mutex_init(&svr->q_mutex, NULL);
#endif
}
#ifdef M_THRD_LQ
void take_lq_lock(server_log_t *svr)
{
    int ret = pthread_mutex_lock(&svr->q_mutex);
    if(ret != 0)
    {
        fprintf(stderr, "taking lock on LQ failed!!!\n");
        exit(0);
    }
}

void release_lq_lock(server_log_t *svr)
{
    int ret = pthread_mutex_unlock(&svr->q_mutex);
    if(ret != 0)
    {
        fprintf(stderr, "releasing lq mutex encountered error !!!\n");
        exit(0);
    }
}
#endif

size_t get_count(server_log_t* svr, log_msg_type m_type)
{
    size_t size = 0;
    if(m_type == NORMAL)
    {
#ifdef M_THRD_LQ
        take_lq_lock(svr);
#endif
        size = svr->nrl_count;
#ifdef M_THRD_LQ
        release_lq_lock(svr);
#endif
    }
    else if(m_type == CONTROL)
    {
    
#ifdef M_THRD_LQ
        take_lq_lock(svr);
#endif
        size = svr->ctrl_count;
#ifdef M_THRD_LQ
        release_lq_lock(svr);
#endif
    }
    else
    {
        fprintf(stderr, "we're not supposed to reach here.\n");
        exit(0);
    }
    return size;
}

log_node_t * dequeue(server_log_t *svr, log_msg_type m_type)
{
    log_node_t *ret_val = NULL;
    if(m_type == INVALID_LOG)
    {
        return NULL;
    }    

#ifdef M_THRD_LQ
    take_lq_lock(svr);
#endif
    if(m_type == NORMAL)
    {
        if(svr->nrl_count > 0)
        {
            if(svr->nrl_count == 1)
            {
                ret_val = svr->nrl_head;
                svr->nrl_head = NULL;
                svr->nrl_tail = NULL;
                svr->nrl_count--;
                goto _OUT;
            }
            svr->nrl_head->next->prev = svr->nrl_tail;
            ret_val = svr->nrl_head;
            svr->nrl_head = svr->nrl_head->next;
            svr->nrl_count--;
            goto _OUT;

        }
    }
    else if(m_type == CONTROL)
    {
        if(svr->ctrl_count > 0)
        {
            if(svr->ctrl_count == 1)
            {
                ret_val = svr->ctrl_head;
                svr->ctrl_head = NULL;
                svr->ctrl_tail = NULL;
                svr->ctrl_count--;
                goto _OUT;
            }
            svr->ctrl_head->next->prev = svr->ctrl_tail;
            ret_val = svr->ctrl_head;
            svr->ctrl_head = svr->ctrl_head->next;
            svr->ctrl_count--;
            goto _OUT;

        }
    }
_OUT:
#ifdef M_THRD_LQ
    release_lq_lock(svr);
#endif
        return ret_val;
}


int enqueue(server_log_t *svr, log_node_t *node, log_msg_type m_type)
{
    int ret_val = 0;
    if(m_type == INVALID_LOG)
    {
        return -1;
    }

#ifdef M_THRD_LQ
    take_lq_lock(svr);
#endif

    if(m_type == NORMAL)
    {
        if(svr->nrl_count >= svr->NRL_Q_LIMIT)
        {
            ret_val = -1;
            goto _OUT;
        }
        if(svr->nrl_count == 0)
        {
            svr->nrl_head = node;
            svr->nrl_tail = node;
        }            
        node->next = svr->nrl_head;
        node->prev = svr->nrl_tail;
        svr->nrl_tail = node;
        svr->nrl_count++;
    }
    else if(m_type == CONTROL)
    {
        if(svr->ctrl_count >= svr->CTRL_Q_LIMIT)
        {
            ret_val = -1;
            goto _OUT;
        }
        if(svr->ctrl_count == 0)
        {
            svr->ctrl_head = node;
            svr->ctrl_tail = node;
        }            
        node->next = svr->ctrl_head;
        node->prev = svr->ctrl_tail;
        svr->ctrl_tail = node;
        svr->ctrl_count++;
    
    }
_OUT:
#ifdef M_THRD_LQ
    release_lq_lock(svr);
#endif
        return ret_val;
}

