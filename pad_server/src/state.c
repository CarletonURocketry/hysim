#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/ioctl.h>

#include "../../logging/logging.h"
#include "actuator.h"
#include "gpio_actuator.h"
#include "pwm_actuator.h"
#include "state.h"

struct actuator_info {
    uint8_t id;
    bool gpio;
    union {
        char *dev;
        pwm_actinfo_t pwm;
    } priv;
};

static const struct actuator_info ACTUATORS[] = {
    {.id = ID_XV1, .gpio = true, .priv = {.dev = "/dev/gpio6"}},
    {.id = ID_XV2, .gpio = true, .priv = {.dev = "/dev/gpio7"}},
    {.id = ID_XV3, .gpio = true, .priv = {.dev = "/dev/gpio8"}},
    {.id = ID_XV4, .gpio = true, .priv = {.dev = "/dev/gpio9"}},
    {.id = ID_XV5, .gpio = true, .priv = {.dev = "/dev/gpio10"}},
    {.id = ID_XV6, .gpio = true, .priv = {.dev = "/dev/gpio11"}},
    {.id = ID_XV7, .gpio = true, .priv = {.dev = "/dev/gpio12"}},
    {.id = ID_XV8, .gpio = true, .priv = {.dev = "/dev/gpio13"}},
    {.id = ID_XV9, .gpio = true, .priv = {.dev = "/dev/gpio2"}},
    {.id = ID_XV10, .gpio = true, .priv = {.dev = "/dev/gpio3"}},
    {.id = ID_XV11, .gpio = true, .priv = {.dev = "/dev/gpio4"}},
    {.id = ID_XV12, .gpio = true, .priv = {.dev = "/dev/gpio5"}},
    {.id = ID_IGNITER, .gpio = true, .priv = {.dev = "/dev/gpio28"}},
    {.id = ID_DUMP,
     .gpio = false,
     .priv = {.pwm =
                  {
                      .dev = "/dev/pwm5",
                      .channel = 1 /* B for RP2040 */,
                      .close_duty = 0x2666,
                      .open_duty = 0x8ccc,
                  }}},
    {.id = ID_QUICK_DISCONNECT,
     .gpio = false,
     .priv = {.pwm =
                  {
                      .dev = "/dev/pwm5",
                      .channel = 0 /* A for RP2040 */,
                      .close_duty = 0x2666,
                      .open_duty = 0x8ccc,
                  }}},
};

/*
 * Initialize the shared pad state. This includes initializing the synchronization objects (rwlock), pad arming state
 * and actuators.
 * @param state The state to initialize.
 */
void padstate_init(padstate_t *state) {
    pthread_rwlock_init(&state->rw_lock, NULL);

    state->arm_level = ARMED_PAD;

    /* Initialize all actuators */

    for (unsigned int i = 0; i < NUM_ACTUATORS; i++) {
        if (ACTUATORS[i].gpio) {
            gpio_actuator_init(&state->actuators[ACTUATORS[i].id], ACTUATORS[i].id, ACTUATORS[i].priv.dev);
            hinfo("Initialized GPIO actuator %d\n", ACTUATORS[i].id);
        } else {
            pwm_actuator_init(&state->actuators[ACTUATORS[i].id], ACTUATORS[i].id, &ACTUATORS[i].priv.pwm);
            hinfo("Initialized PWM actuator %d\n", ACTUATORS[i].id);
        }
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
    hinfo("Signalled padstate update.\n");
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
        hinfo("Updated pad state to arming level %s.\n", arm_state_str(new_arm));
        state->arm_level = new_arm;
    } else {
        hwarn("Rejected arming level %s.\n", arm_state_str(new_arm));
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
    actuator_t *act;

    /* Actuator is the dump valve, which can always be actuated */

    if (id == ID_DUMP) goto actuate_actuator;

    /* Invalid actuator ID */

    if (id >= NUM_ACTUATORS) {
        hwarn("Invalid actuator ID: %u\n", id);
        return ACT_DNE;
    }
    act = &state->actuators[id];

    is_solenoid_valve = (id >= ID_XV1 && id <= ID_XV12) && id != ID_FIRE_VALVE;

    /* Invalid state requested */

    if (req_state != 0 && req_state != 1) {
        hwarn("Request invalid actuator state: %u\n", req_state);
        return ACT_INV;
    }

    /* Get the current arming level */

    arm_lvl = padstate_get_level(state);

    /* Check if the current arming level permits for the actuator to be commanded */

    switch (arm_lvl) {
    case ARMED_PAD:
        hwarn("Denied actuation of %s\n", actuator_get_name(act));
        return ACT_DENIED;
        break;

    case ARMED_VALVES:
        if (!is_solenoid_valve) {
            hwarn("Denied actuation of %s\n", actuator_get_name(act));
            return ACT_DENIED;
        }
        break;

    case ARMED_IGNITION:
        if (!is_solenoid_valve && id != ID_QUICK_DISCONNECT) {
            hwarn("Denied actuation of %s\n", actuator_get_name(act));
            return ACT_DENIED;
        }
        break;

    case ARMED_DISCONNECTED:
        if (!is_solenoid_valve && id != ID_QUICK_DISCONNECT && id != ID_IGNITER) {
            hwarn("Denied actuation of %s\n", actuator_get_name(act));
            return ACT_DENIED;
        }
        break;

    case ARMED_LAUNCH:
        /* Every command is available */
        break;
    }

actuate_actuator:

    /* For now just always actuate the actuator */

    err = actuator_set(act, req_state);
    if (err) {
        hwarn("Failed to set actuator %s -> %u\n", actuator_get_name(act), req_state);
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
