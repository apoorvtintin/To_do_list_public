
/*
 * @file state.c
 * @author Mohammed Sameer <sameer.2897@gmail.com>
 * @brief this file has the implementation of states for the server bringup,
 * and FSM Mgmt.
 */

#include <stdbool.h>

#include "state.h"
#include "chkpt.h"
#include "dbg_assert.h"

extern volatile server_states_t prev_state;

// Global variables
static volatile run_state_t server_state = {UNKNOWN_REP, UNKNOWN_STATE};

void set_mode(rep_mode_t rep_mode) {
    // If state was not unknown, then assert,
    // we dont want to switch b/w active repl and warm passive
    //dbg_requires(server_state.rep_mode == UNKNOWN_REP);
    //dbg_ensures(((rep_mode > UNKNOWN_REP) && (rep_mode <= PASSIVE_REP)));
    server_state.rep_mode = rep_mode;
    dbg_printf("mode set to %s\n", rep_mode_str(server_state.rep_mode));
}

void set_state(server_states_t s_state) {
    dbg_printf("\nTrying to set state to %s; cur mode = %s, cur state = %s\n", server_states_str(s_state),
            rep_mode_str(server_state.rep_mode), server_states_str(server_state.state));
    /*dbg_requires(
        (server_state.rep_mode != UNKNOWN_REP)); // Make sure rep mode was set
    if (server_state.rep_mode == ACTIVE_REP) {
        if (s_state != ACTIVE_RUNNING && s_state != ACTIVE_RECOVER && s_state != QUIESCE) {
            dbg_ensures(false);
        }
    } else if (server_state.rep_mode == PASSIVE_REP) {
        if (s_state != PASSIVE_PRIMARY && s_state != PASSIVE_BACKUP &&
            s_state != PASSIVE_PREPRIMARY) {
            dbg_ensures(false);
        }
    }*/
    // server_states_t prev_state = server_state.state;
	if(server_state.rep_mode == ACTIVE_REP && server_state.state == UNKNOWN_STATE) {
		if(s_state == ACTIVE_RUNNING) {
			dbg_printf("\ndirect state change not allowed set state to %s; cur mode = %s, cur state = %s\n", server_states_str(s_state),
            rep_mode_str(server_state.rep_mode), server_states_str(server_state.state));
		}
	}
	if(server_state.state == QUIESCE)
		prev_state = s_state;
    if (server_state.rep_mode == PASSIVE_REP) {
        if (s_state == PASSIVE_PRIMARY /*|| s_state == PASSIVE_PREPRIMARY*/) {
			dbg_printf("checkpoint thread started\n\n");
            start_ckhpt_thread();
        } else if (s_state == PASSIVE_BACKUP) {
            kill_chkpt_thrd_if_running();
        }
    } else if(server_state.rep_mode == ACTIVE_REP) {
		kill_chkpt_thrd_if_running();
	}
    server_state.state = s_state;
    dbg_printf("Server State set to %s\n",
               server_states_str(server_state.state));
}

rep_mode_t get_mode() { return server_state.rep_mode; }

server_states_t get_state() { return server_state.state; }

char *rep_mode_str(rep_mode_t rep_mode) {
    switch (rep_mode) {
    case UNKNOWN_REP:
        return "UNKNOWN_REP";
    case ACTIVE_REP:
        return "ACTIVE_REP";
    case PASSIVE_REP:
        return "PASSIVE_REP";
    }
    return "UPDATE_STR_FUNC";
}

char *server_states_str(server_states_t server_state) {
    switch (server_state) {
    case UNKNOWN_STATE:
        return "UNKNOWN_STATE";
    case QUIESCE:
        return "QUIESCE";
    case ACTIVE_RUNNING:
        return "ACTIVE_RUNNING";
    case ACTIVE_RECOVER:
        return "ACTIVE_RECOVER";
    case PASSIVE_PRIMARY:
        return "PASSIVE_PRIMARY";
    case PASSIVE_BACKUP:
        return "PASSIVE_BACKUP";
    case PASSIVE_PREPRIMARY:
        return "PASSIVE_PREPRIMARY";
    }
    return "UPDATE_STR_FUNC";
}
