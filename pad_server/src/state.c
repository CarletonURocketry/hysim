#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "actuator.h"
#include "state.h"

static int dummy_on(actuator_t *act) {
    printf("Actuator #%d turned on\n", act->id);
    return 0;
}
static int dummy_off(actuator_t *act) {
    printf("Actuator #%d turned off\n", act->id);
    return 0;
}

/* TODO: docs */
void padstate_init(padstate_t *state) {
    pthread_rwlock_init(&state->rw_lock, NULL);
    // TODO: Is this right? Can we assume if the program is running then the pad is armed?
    state->arm_level = ARMED_PAD;
    for (unsigned int i = 0; i < NUM_ACTUATORS; i++) {
        actuator_init(&state->actuators[i], i, dummy_on, dummy_off, NULL);
    }
}

/* TODO: docs
 *
 */
int padstate_get_level(padstate_t *state, arm_lvl_e *arm_val) {
    int err;
    err = pthread_rwlock_rdlock(&state->rw_lock);
    if (err) return err;

    *arm_val = state->arm_level;

    return pthread_rwlock_unlock(&state->rw_lock);
}

/* TODO: docs
 * NOTE: this does not check for valid arming state transitions
 */
int padstate_change_level(padstate_t *state, arm_lvl_e new_arm) {
    int err;
    err = pthread_rwlock_wrlock(&state->rw_lock);
    if (err) return err;

    state->arm_level = new_arm;

    return pthread_rwlock_unlock(&state->rw_lock);
}

/*
 * Changes the state of the actuator, no checks are done
 * @param id The actuator id
 * @param new_state The new actuator state
 * @return 0 for success, -1 for error
 */
int padstate_actuate(padstate_t *state, uint8_t id, bool new_state) {
    state->actuators[id].state = new_state;
    return 0;
}

/*
 * Changes the state of the actuator, no checks are done
 * @param id The actuator id
 * @param new_state The new actuator state
 * @return 0 for success, -1 for error
 */
int padstate_get_actstate(padstate_t *state, uint8_t act_id, bool *act_state) {
    if (act_id >= NUM_ACTUATORS) return -1;

    *act_state = state->actuators[act_id].state;
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

    arm_lvl_e arm_lvl;
    int err = padstate_get_level(state, &arm_lvl);
    if (err) {
        errno = err;
        return -1;
    }

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
    err = padstate_get_actstate(state, id, &current_state);
    if (err) {
        errno = err;
        return -1;
    }

    if (new_state == current_state) {
        return ACT_OK;
    }

    err = actuator_set(&state->actuators[id], new_state);

    if (err) {
        return -1;
    }

    return ACT_OK;
}
