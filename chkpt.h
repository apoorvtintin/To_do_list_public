/*
 * @file chkpt.h
 * @author Mohammed Sameer <sameer.2897@gmail.com>
 * @brief this file has the interface fucntion defnitions for
 * checkpointing functionality.
 */
#ifndef _CHKPT_H_
#define _CHKPT_H_
#include <stdio.h>

void start_ckhpt_thread();

void kill_chkpt_thrd_if_running();

#endif
