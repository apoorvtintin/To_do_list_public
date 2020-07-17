/*
 * @file state.h
 * @author Mohammed Sameer <sameer.2897@gmail.com>
 * @brief this file includes all the common utility functions and definitions
 * for maintaining the server state.
 */

#ifndef _STATE_H_
#define _STATE_H_
#include <stdio.h>

typedef enum _rep_mode \
{ \
    UNKNOWN_REP, \
    ACTIVE_REP, \
    PASSIVE_REP \
} rep_mode_t;

typedef enum _server_states
{
     UNKNOWN_STATE,
     // Common state
     QUIESCE,
     // Active rep states
     ACTIVE_RUNNING, 
     ACTIVE_RECOVER,
     // Warm Passive states
     PASSIVE_PRIMARY,
     PASSIVE_BACKUP,
     PASSIVE_RECOVER
     
} server_states_t;

typedef struct _server_state
{
    rep_mode_t rep_mode;
    server_states_t state;
} run_state_t;

extern volatile run_state_t server_state;

void set_mode(rep_mode_t rep_mode);
void set_state(server_states_t s_state);
rep_mode_t get_mode();
server_states_t get_state();


#endif
