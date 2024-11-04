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
 * @return ARM_OK for success, ARM_INV or ARM_DENIED for invalid or out of order arming state, and -1 on error, setting
 * ERRNO.
 */
int change_arm_level(padstate_t *state, arm_lvl_e new_arm, cntrl_subtype_e cmd_src) {
    if (new_arm > ARMED_LAUNCH || new_arm < ARMED_PAD) {
        return ARM_INV;
    }

    int err;

    arm_lvl_e current_state;
    err = padstate_get_level(state, &current_state);
    if (err) {
        errno = err;
        return -1;
    }

    if ((current_state + 1 == new_arm && new_arm >= ARMED_DISCONNECTED && cmd_src == CNTRL_ACT_REQ) ||
        (current_state + 1 == new_arm && new_arm < ARMED_DISCONNECTED && cmd_src == CNTRL_ARM_REQ)) {
        err = padstate_change_level(state, new_arm);
        if (err) {
            errno = err;
            return -1;
        }
    } else {
        return ARM_DENIED;
    }

    return ARM_OK;
}
