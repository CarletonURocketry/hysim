#include "state.h"
#include <pthread.h>

/* TODO: docs */
void padstate_init(padstate_t *state) {
    pthread_mutex_init(&state->lock, NULL);
    // TODO: Is this right? Can we assume if the program is running then the pad is armed?
    state->arm_level = ARMED_PAD;
    for (unsigned int i = 0; i < NUM_ACTUATORS; i++) {
        state->actuators[i] = false;
    }
}

/* TODO: docs
 * NOTE: this does not check for valid arming state transitions
 */
int padstate_change_level(padstate_t *state, arm_lvl_e new_arm) {
    int err;
    err = pthread_mutex_lock(&state->lock);
    if (err) return err;

    state->arm_level = new_arm;

    err = pthread_mutex_unlock(&state->lock);
    if (err) return err;

    return 0;
}

/* TODO: docs
 * NOTE: This does not check for valid actuator ID
 */
int padstate_actuate(padstate_t *state, uint8_t id, bool new_state) {
    int err;
    err = pthread_mutex_lock(&state->lock);
    if (err) return err;

    state->actuators[id] = new_state;

    err = pthread_mutex_unlock(&state->lock);
    if (err) return err;

    return 0;
}
