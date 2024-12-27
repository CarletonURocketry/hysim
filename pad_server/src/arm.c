#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../packets/packet.h"
#include "arm.h"
#include "state.h"

/*
 * Attempt to change arming level.
 * @param state The current state of the pad server.
 * @param new_arm The new arming level to attempt to change to.
 * @param cmd_src The source of the command, as a cntrl_subtype_e.
 * @return ARM_OK for success, ARM_INV or ARM_DENIED for invalid or out of order arming state.
 */
int change_arm_level(padstate_t *state, arm_lvl_e new_arm) {
    if (new_arm > ARMED_LAUNCH || new_arm < ARMED_PAD) {
        return ARM_INV;
    }

    int err;
    /* Arming state read before */
    arm_lvl_e current_arm = padstate_get_level(state);

    do {
        bool lvl_increase = new_arm == current_arm + 1 && new_arm <= ARMED_LAUNCH;
        bool lvl_decrease_from_armed_valves = current_arm == ARMED_VALVES && new_arm == ARMED_PAD;
        bool lvl_decrease_from_firing_sequence =
            new_arm == ARMED_VALVES &&
            (current_arm == ARMED_IGNITION || current_arm == ARMED_DISCONNECTED || current_arm == ARMED_LAUNCH);

        if (lvl_increase || lvl_decrease_from_armed_valves || lvl_decrease_from_firing_sequence) {
            /* Returns 1 on success, 0 on failure */
            err = padstate_change_level(state, &current_arm, new_arm);
            /* In case of failure, it means some other thread changed the arming level without us knowing or the CAS
             * failed spuriously, try to rerun all the checks and set it again. */
        } else {
            return ARM_DENIED;
        }
    } while (!err);

    pthread_mutex_lock(&state->update_mut);
    state->update_recorded = true;
    pthread_cond_signal(&state->update_cond);
    pthread_mutex_unlock(&state->update_mut);
    return ARM_OK;
}