/*
 * @file chkpt.h
 * @author Mohammed Sameer <sameer.2897@gmail.com>
 * @brief this file has the interface fucntion defnitions for
 * checkpointing functionality.
 */
#ifndef _CHKPT_H_
#define _CHKPT_H_
#include <stdio.h>

#include "c_s_iface.h"

void start_ckhpt_thread();

void kill_chkpt_thrd_if_running();

void set_bckup_servers(bsvr_ctx svrs[2]);

void set_checkpoint_freq(int checkpoint_freq);

#endif
