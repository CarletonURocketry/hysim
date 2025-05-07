#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
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

    // TODO: Is this right? Can we assume if the program is running then the pad is armed?
    // TODO: make GPIO device path change

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
 * @return The current arming level
 */
arm_lvl_e padstate_get_level(padstate_t *state) {
    /* Something has gone terribly wrong if reading a variable doesn't work, so no errors are returned from here */
    return atomic_load_explicit(&state->arm_level, __ATOMIC_ACQUIRE);
}

/* TODO: docs
 * NOTE: this does not check for valid arming state transitions
 * NOTE: use in a loop, see arm.c for an example and why
 * @return 1 for success, 0 for error
 */
int padstate_change_level(padstate_t *state, arm_lvl_e *old_arm, arm_lvl_e new_arm) {
    return atomic_compare_exchange_weak_explicit(&state->arm_level, old_arm, new_arm, memory_order_release,
                                                 memory_order_acquire);
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
    if (id >= NUM_ACTUATORS) return ACT_DNE;

    if (req_state != 0 && req_state != 1) return ACT_INV;
    bool new_state = (bool)req_state;

    arm_lvl_e arm_lvl = padstate_get_level(state);

    bool is_solenoid_valve = id >= ID_XV1 && id <= ID_XV12;

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

    bool current_state;
    int err = padstate_get_actstate(state, id, &current_state);
    if (err) {
        errno = err;
        return -1;
    }

    if (new_state != current_state) {
        err = actuator_set(&state->actuators[id], new_state);
        if (err) {
            return -1;
        }
    }

    pthread_mutex_lock(&state->update_mut);
    state->update_recorded = true;
    pthread_cond_signal(&state->update_cond);
    pthread_mutex_unlock(&state->update_mut);

    return ACT_OK;
}
