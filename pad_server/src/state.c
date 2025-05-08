#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "actuator.h"
#include "gpio_actuator.h"
#include "state.h"
#include <sys/ioctl.h>

static const char *ACTUATOR_GPIO[NUM_ACTUATORS] = {
    [ID_FIRE_VALVE] = "/dev/gpio27", [ID_XV1] = "/dev/gpio2",
    [ID_XV2] = "/dev/gpio3",         [ID_XV3] = "/dev/gpio4",
    [ID_XV4] = "/dev/gpio5",         [ID_XV5] = "/dev/gpio6",
    [ID_XV6] = "/dev/gpio7",         [ID_XV7] = "/dev/gpio8",
    [ID_XV8] = "/dev/gpio9",         [ID_XV9] = "/dev/gpio10",
    [ID_XV10] = "/dev/gpio11",       [ID_XV11] = "/dev/gpio12",
    [ID_XV12] = "/dev/gpio12",       [ID_QUICK_DISCONNECT] = "/dev/gpio26",
    [ID_IGNITER] = "/dev/gpio28", // TODO double check?
};

/*
 * Initialize the shared pad state. This includes initializing the synchronization objects (rwlock), pad arming state
 * and actuators.
 * @param state The state to initialize.
 */
void padstate_init(padstate_t *state) {
    pthread_rwlock_init(&state->rw_lock, NULL);

    state->arm_level = ARMED_PAD;
    for (unsigned int i = 0; i < NUM_ACTUATORS; i++) {
        gpio_actuator_init(&state->actuators[i], i, ACTUATOR_GPIO[i]);
    }

    pthread_mutex_init(&state->update_mut, NULL);
    pthread_cond_init(&state->update_cond, NULL);
    state->update_recorded = false;
}

/*
 * Gets the current arming level of the pad.
 * @param state The pad state
 * @return The current arming level, or -1 on failure
 */
arm_lvl_e padstate_get_level(padstate_t *state) {
    arm_lvl_e level = -1;

    if (pthread_rwlock_rdlock(&state->rw_lock) != 0) {
        return level;
    }
    level = state->arm_level;
    pthread_rwlock_unlock(&state->rw_lock);
    return level;
}

/*
 * Signal an update of the state.
 * @param state The pad state to signal an update for.
 * @return 0 on success, errno code on failure
 */
int padstate_signal_update(padstate_t *state) {
    int err;

    err = pthread_mutex_lock(&state->update_mut);
    if (err) return err;

    state->update_recorded = true;
    err = pthread_cond_signal(&state->update_cond);
    pthread_mutex_unlock(&state->update_mut);
    return err;
}

/*
 * Attempt to change arming level.
 * @param state The current state of the pad server.
 * @param new_arm The new arming level to attempt to change to.
 * @param cmd_src The source of the command, as a cntrl_subtype_e.
 * @return ARM_OK for success, ARM_INV or ARM_DENIED for invalid or out of order arming state.
 */
int padstate_change_level(padstate_t *state, arm_lvl_e new_arm) {
    int err;

    /* Invalid arming level selected */

    if (new_arm > ARMED_LAUNCH || new_arm < ARMED_PAD) {
        return ARM_INV;
    }

    /* Lock state for computation */

    err = pthread_rwlock_wrlock(&state->rw_lock);
    if (err) return ARM_DENIED; /* Might be a better error to return, but this works */

    /* Check if there is an attempt to increase the arming level */

    bool lvl_increase = (new_arm == state->arm_level + 1 && new_arm <= ARMED_LAUNCH);

    /* Check if we are decreasing from the ARMED_VALVES state */

    bool lvl_decrease_from_armed_valves = (state->arm_level == ARMED_VALVES && new_arm == ARMED_PAD);

    /* Check if we are decreasing the arming level from anywhere in the firing sequence, i.e. ignition, quick disconnect
     * disconnected or armed for launch */

    bool lvl_decrease_from_firing_sequence =
        new_arm == ARMED_VALVES && (state->arm_level == ARMED_IGNITION || state->arm_level == ARMED_DISCONNECTED ||
                                    state->arm_level == ARMED_LAUNCH);

    /* If any of these cases are true, we can perform the change */

    if (lvl_increase || lvl_decrease_from_armed_valves || lvl_decrease_from_firing_sequence) {
        state->arm_level = new_arm;
    } else {
        pthread_rwlock_unlock(&state->rw_lock);
        return ARM_DENIED;
    }

    /* Unlock state now that new arming level has been decided */

    pthread_rwlock_unlock(&state->rw_lock);

    /* Signal an update in state */

    padstate_signal_update(state);

    return ARM_OK;
}

/*
 * Obtains state of actuator given id using
 * @param id The actuator id
 * @param act_state Variable to store actuator state
 * @return 0 for success, -1 for error
 */
int padstate_get_actstate(padstate_t *state, uint8_t act_id, bool *act_state) {
    if (act_id >= NUM_ACTUATORS) return -1;

    *act_state = atomic_load(&state->actuators[act_id].state);
    return 0;
}

/*
 * Set the value of an actuator with the required progression
 * @param id The actuator id
 * @param req_state The new actuator state
 * @return ACT_OK for success, ACT_DNE for invalid id, ACT_INV for invalid req_state, -1 for errors eith errno being
 * set
 */
int pad_actuate(padstate_t *state, uint8_t id, uint8_t req_state) {
    bool is_solenoid_valve;
    arm_lvl_e arm_lvl;
    int err;

    /* Invalid actuator ID */

    if (id >= NUM_ACTUATORS) return ACT_DNE;

    is_solenoid_valve = id >= ID_XV1 && id <= ID_XV12;

    /* Invalid state requested */

    if (req_state != 0 && req_state != 1) return ACT_INV;

    /* Get the current arming level */

    arm_lvl = padstate_get_level(state);

    /* Check if the current arming level permits for the actuator to be commanded */

    switch (arm_lvl) {
    case ARMED_PAD:
        return ACT_DENIED;
        break;

    case ARMED_VALVES:
        if (!is_solenoid_valve) {
            return ACT_DENIED;
        }
        break;

    case ARMED_IGNITION:
        if (!is_solenoid_valve && id != ID_QUICK_DISCONNECT) {
            return ACT_DENIED;
        }
        break;

    case ARMED_DISCONNECTED:
        if (!is_solenoid_valve && id != ID_QUICK_DISCONNECT && id != ID_IGNITER) {
            return ACT_DENIED;
        }
        break;

    case ARMED_LAUNCH:
        // every command is available
        break;
    }

    /* For now just always actuate the actuator */

    err = actuator_set(&state->actuators[id], req_state);
    if (err) {
        errno = err;
        return -1;
    }

    /* Now we made it past the permission being denied *and* the actuator was successfully actuated.
     * Check if this actuator is a special actuator that increases the arming level */

    if (id == ID_QUICK_DISCONNECT) {

        /* If we disconnected and we're in a state less than ARMED_DISCONNECTED, advance the state */

        if (req_state && padstate_get_level(state) < ARMED_DISCONNECTED) {
            padstate_change_level(state, ARMED_DISCONNECTED);
        }

        /* If we re-connected, move back a level prior */

        if (!req_state) {
            padstate_change_level(state, ARMED_IGNITION);
        }

    } else if (id == ID_IGNITER) {

        /* If we ignited and we're in a state less than ARMED_LAUNCH, advance the state */

        if (req_state && padstate_get_level(state) < ARMED_LAUNCH) {
            padstate_change_level(state, ARMED_LAUNCH);
        }

        /* If we un-ignited, move back a level prior */

        if (!req_state) {
            padstate_change_level(state, ARMED_DISCONNECTED);
        }
    }

    padstate_signal_update(state);
    return ACT_OK;
}
