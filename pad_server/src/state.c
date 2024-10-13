#include "state.h"
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>

/* TODO: docs */
void padstate_init(padstate_t *state) {
    pthread_mutex_init(&state->r_mutex, NULL);
    pthread_mutex_init(&state->w_mutex, NULL);

    sem_init(&state->read_try, 0, 1);
    sem_init(&state->resource, 0, 1);

    state->write_count = 0;
    state->read_count = 0;

    // TODO: Is this right? Can we assume if the program is running then the pad is armed?
    state->arm_level = ARMED_PAD;
    for (unsigned int i = 0; i < NUM_ACTUATORS; i++) {
        state->actuators[i] = false;
    }
}

// based on https://en.wikipedia.org/wiki/Readers%E2%80%93writers_problem#Second_readers%E2%80%93writers_problem
//
/* TODO: docs
 *
 */
int padstate_get_level(padstate_t *state, arm_lvl_e *arm_val) {
    int err;
    // wait until we can read, this can be set by writers
    err = sem_wait(&state->read_try);
    if (err) return errno;
    // lock for read_count race condition
    err = pthread_mutex_lock(&state->r_mutex);
    if (err) return err;

    state->read_count++;

    if (state->read_count == 1) {
        err = sem_wait(&state->resource); // don't let writers modify the resource
    }

    err = pthread_mutex_unlock(&state->r_mutex);
    if (err) return err;

    // allow other readers to do their actions; doesn't change anything for us since we're just reading
    err = sem_post(&state->read_try);

    *arm_val = state->arm_level;

    // lock for read_count race condition
    err = pthread_mutex_lock(&state->r_mutex);
    if (err) return err;

    state->read_count--;

    if (state->read_count == 0) {
        err = sem_post(&state->resource); // let writers modify the resource
    }

    err = pthread_mutex_unlock(&state->r_mutex);
    if (err) return err;

    return 0;
}

/* TODO: docs
 * NOTE: this does not check for valid arming state transitions
 */
int padstate_change_level(padstate_t *state, arm_lvl_e new_arm) {
    int err;
    err = pthread_mutex_lock(&state->w_mutex);
    if (err) return err;

    state->write_count++;

    // we're the first writer
    if (state->write_count == 1) {
        // stop readers in queue from reading
        err = sem_wait(&state->read_try);
        if (err) return err;
    }
    // let other writers join queue
    err = pthread_mutex_unlock(&state->w_mutex);
    if (err) return err;

    // stop other writers from writing at the same time
    err = sem_wait(&state->resource);
    if (err) return err;

    state->arm_level = new_arm;

    err = sem_post(&state->resource);
    if (err) return err;

    // lock write_count
    err = pthread_mutex_lock(&state->w_mutex);
    if (err) return err;

    state->write_count--;
    // we were the last writer, let the readers start reading
    if (state->write_count == 0) {
        err = sem_post(&state->read_try);
        if (err) return err;
    }
    err = pthread_mutex_unlock(&state->w_mutex);
    if (err) return err;

    return 0;
}
/* TODO: docs
 * NOTE: This does not check for valid actuator ID
 */
int padstate_actuate(padstate_t *state, uint8_t id, bool new_state) {
    int err;
    err = pthread_mutex_lock(&state->w_mutex);
    if (err) return err;

    state->write_count++;

    // we're the first writer
    if (state->write_count == 1) {
        // stop readers from reading
        err = sem_wait(&state->read_try);
        if (err) return err;
    }
    // let other writers join queue
    err = pthread_mutex_unlock(&state->w_mutex);
    if (err) return err;

    // stop other writers from writing at the same time
    err = sem_wait(&state->resource);
    if (err) return err;

    state->actuators[id] = new_state;

    err = sem_post(&state->resource);
    if (err) return err;

    // lock write_count
    err = pthread_mutex_lock(&state->w_mutex);
    if (err) return err;

    state->write_count--;
    // we were the last writer, let the readers start reading
    if (state->write_count == 0) {
        err = sem_post(&state->read_try);
        if (err) return err;
    }

    err = pthread_mutex_unlock(&state->w_mutex);
    if (err) return err;

    return 0;
}
