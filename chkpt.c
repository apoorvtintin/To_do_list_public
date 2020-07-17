
/*
 * @file chkpt.c
 * @author Mohammed Sameer <sameer.2897@gmail.com>
 * @brief this file has the implementation for checkpointing both for 
 * Active replication mode, as well as passive replication mode.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>

#include "util.h"
#include "storage.h"
#include "dbg_assert.h"

// Temp remove this
extern int msg_count;
extern pthread_mutex_t storage_lock;
extern int server_id;
extern uint64_t chk_point_num;

// Global Static valiables.
static volatile int signal_exit_checkpt_thread = 0;
static volatile int is_chkpt_thrd_running = 0;
static bsvr_ctx bckup_svr[2];

void set_bckup_servers(bsvr_ctx svrs[2])
{
    memcpy(bckup_svr, svrs, 2 * sizeof(bsvr_ctx));
}

void open_ports_secondary()
{
    init_bsvr_ctx(&bckup_svr[0]);
    init_bsvr_ctx(&bckup_svr[1]);
    // Hard coded for now;
    // take this info dynamically from somewhere else later
    memcpy(bckup_svr[0].info.server_ip, "127.0.0.1", strlen("127.0.0.1")+1);
    bckup_svr[0].info.port = 23457;
    bckup_svr[0].server_id = 1;
    if( (bckup_svr[0].fd = connect_to_server(&bckup_svr[0].info))
            < 0)
    {
        fprintf(stderr, "failed open channel to backup server %d !!!\n", bckup_svr[0].server_id);
        exit(0);
    }

    memcpy(bckup_svr[1].info.server_ip, "127.0.0.1", strlen("127.0.0.1")+1);
    bckup_svr[1].info.port = 23458;
    bckup_svr[1].server_id = 2;
    if( (bckup_svr[1].fd = connect_to_server(&bckup_svr[1].info))
            < 0)
    {
        fprintf(stderr, "failed open channel to backup server %d!!\n", bckup_svr[1].server_id);
        exit(0);
    }
}


int write_check_point(bsvr_ctx *ctx, char *chk_file_name)
{
    char buffer[MAX_LENGTH];
        int fd = -1, n;
    fd = open(chk_file_name, O_RDONLY);
    if(fd < 0)
    {
        fprintf(stderr, "open of check point failed.\n");
        return -1;
    }
    char resp_buf[MAX_LENGTH];
    int resp_len =
        snprintf(resp_buf, sizeof(resp_buf),
                "Client ID: %d\r\n"
                "Request No: %lu\r\n"
                "Message Type: %d\r\n"
                "Data Length: %lu\r\n"
                "\r\n",
                server_id, chk_point_num, MSG_CHK_PT,
                get_file_size(chk_file_name));
    fprintf(stderr, "sending checkpint to replica %d\n", ctx->server_id);
    fprintf(stderr, "\n-------------------------------------------------\n");
    write(2, resp_buf, resp_len);
    print_state();
    fprintf(stderr, "\n-------------------------------------------------\n");
    if(write(ctx->fd, resp_buf, resp_len) < 0)
    {
            fprintf(stderr, "failed writing checkpoint\n");
        goto _END;
    }
    int sent = 0;
    while((n = read(fd, buffer, MAX_LENGTH)) > 0)
    {
        sent += n;
        if(write(ctx->fd, buffer, n) < 0)
        {
            fprintf(stderr, "failed writing checkpoint\n");
            goto _END;
        }
    }
_END:
    close(fd);
    return 0;


}

void * send_checkpoint(void *argvp)
{
    while(1)
    {
        if(signal_exit_checkpt_thread == 1)
        {
            signal_exit_checkpt_thread = 0;
            is_chkpt_thrd_running = 0;
            return (void *)0;
        }
        sleep(10); // TODO: checge this, run every 10 seconds
#if 0
        if(msg_count < 2)
        {
            sleep(1);
            continue;
        }
        msg_count = 0;
#endif
        open_ports_secondary();

        // Add code to get the checkpoint
        pthread_mutex_lock(&storage_lock);
        char checkpoint_file_name[MAX_LENGTH];
        export_db(checkpoint_file_name);
        pthread_mutex_unlock(&storage_lock);
        for(unsigned int i = 0; i < sizeof(bckup_svr)/sizeof(bsvr_ctx); i++)
        {
            if(bckup_svr[i].fd >= 0)
            {
                write_check_point(&bckup_svr[i], checkpoint_file_name);
                close(bckup_svr[i].fd);
            }
        }
        chk_point_num++;
    }
    return (void *)0;
}
void start_ckhpt_thread()
{
    if(is_chkpt_thrd_running == 1)
    {
        dbg_printf("The chkpt thread is already running!!!\n");
        return;
    }
    pthread_t id;
    pthread_create(&id, NULL, send_checkpoint, NULL);
    is_chkpt_thrd_running = 1;
}

void stop_chk_pt_thread()
{
    signal_exit_checkpt_thread = 1;
    while(signal_exit_checkpt_thread == 1);
    return;
}

void kill_chkpt_thrd_if_running()
{
    if(is_chkpt_thrd_running == 1)
    {
        stop_chk_pt_thread();
    }
    return;
}
