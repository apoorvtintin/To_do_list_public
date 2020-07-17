
/*
 * @file state.c
 * @author Mohammed Sameer <sameer.2897@gmail.com>
 * @brief this file has the implementation of states for the server bringup,
 * and FSM Mgmt.
 */
#include "state.h"
#include <stdbool.h>
#include "dbg_assert.h"

volatile run_state_t server_state = {UNKNOWN_REP, UNKNOWN_STATE};

void set_mode(rep_mode_t rep_mode)
{
    // If state was not unknown, then assert, 
    // we dont want to switch b/w active repl and warm passive
    dbg_requires(server_state.rep_mode == UNKNOWN_REP);
    dbg_ensures(((rep_mode > UNKNOWN_REP) && (rep_mode <= PASSIVE_REP)));
    server_state.rep_mode = rep_mode;
}

void set_state(server_states_t s_state)
{
    dbg_requires((server_state.rep_mode != UNKNOWN_REP)); // Make sure rep mode was set
    if(server_state.rep_mode ==  ACTIVE_REP)
    {
        if(s_state != ACTIVE_RUNNING || s_state != ACTIVE_RECOVER)
        {
            dbg_ensures(false);
        }
    }
    else if(server_state.rep_mode == PASSIVE_REP)
    {
        if(s_state != PASSIVE_PRIMARY ||
                s_state != PASSIVE_BACKUP||
                s_state != PASSIVE_RECOVER)
        {
            dbg_ensures(false);
        }
    }
    server_state.state = s_state;
}

rep_mode_t get_mode()
{
    return server_state.rep_mode;
}

server_states_t get_state()
{
    return server_state.state;
}
