#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>

#include "state.h"

/* TODO: docs */
void padstate_init(padstate_t *state) {
    pthread_rwlockattr_setkind_np(
        &state->rw_lock_attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP); // Linux only, change if we deploy to
                                                                             // nuttx, default for nuttx is writers pref
    pthread_rwlock_init(&state->rw_lock, &state->rw_lock_attr);

    // TODO: Is this right? Can we assume if the program is running then the pad is armed?
    state->arm_level = ARMED_PAD;
    for (unsigned int i = 0; i < NUM_ACTUATORS; i++) {
        state->actuators[i] = false;
    }
}

//
/* TODO: docs
 *
 */
int padstate_get_level(padstate_t *state, arm_lvl_e *arm_val) {
    int err;
    err = pthread_rwlock_rdlock(&state->rw_lock);
    if (err) return err;

    *arm_val = state->arm_level;

    err = pthread_rwlock_unlock(&state->rw_lock);
    if (err) return err;

    return 0;
}

/* TODO: docs
 * NOTE: this does not check for valid arming state transitions
 */
int padstate_change_level(padstate_t *state, arm_lvl_e new_arm) {
    int err;
    err = pthread_rwlock_wrlock(&state->rw_lock);
    if (err) return err;

    state->arm_level = new_arm;

    err = pthread_rwlock_unlock(&state->rw_lock);
    if (err) return err;

    return 0;
}
/* TODO: docs
 * NOTE: This does not check for valid actuator ID
 */
int padstate_actuate(padstate_t *state, uint8_t id, bool new_state) {
    int err;
    err = pthread_rwlock_wrlock(&state->rw_lock);
    if (err) return err;

    state->actuators[id] = new_state;

    err = pthread_rwlock_unlock(&state->rw_lock);

    return 0;
}
