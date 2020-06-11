/*
 * @file util.c
 * @brief This file implemts the msg utility functions defined in msg_util.h
 * @author Mohammed Sameer (mohammes@andrew.cmu.edu)
 */

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "c_s_iface.h"
#include "msg_util.h"

void print_user_msg_add(void *msg) {
    struct message_add *msg_ptr = (struct message_add *)msg;
    printf("----------------------------------------------------------\n");
    printf("| Message Type : Add                                     |\n");
    printf("| Task         : %s                                      |\n",
           msg_ptr->task);
    printf("| Task Date    : %s                                      |\n",
           msg_ptr->task_date);
    printf("| Task Status  : %d                                      |\n",
           msg_ptr->task_status);
    printf("----------------------------------------------------------\n");
}

void print_msg(void *msg) {
    struct message_storage *msg_ptr = (struct message_storage *)msg;

    switch (msg_ptr->header.msg_type) {
    case MSG_ADD:
        print_user_msg_add((msg + offsetof(struct message_storage, task)));
    }
}
