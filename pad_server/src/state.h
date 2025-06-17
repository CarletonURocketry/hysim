#ifndef _STATE_H_
#define _STATE_H_
#include "actuator.h"
#define MAX_READERS 255 // random number, feel free to change

#include "../../packets/packet.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

/* Number of actuators in the system:
 * - 12 solenoid valves
 * - 1 quick disconnect
 * - 1 dump valve
 * - 1 igniter
 */

#define NUM_ACTUATORS (12 + 1 + 1 + 1)

/* State of the entire pad control system */
typedef struct {
    actuator_t actuators[NUM_ACTUATORS];
    arm_lvl_e arm_level;
    conn_status_e conn_status;
    pthread_rwlock_t rw_lock;
    pthread_mutex_t update_mut;
    pthread_cond_t update_cond;
    bool update_recorded;
} padstate_t;

void padstate_init(padstate_t *state);
arm_lvl_e padstate_get_level(padstate_t *state);
conn_status_e padstate_get_connstatus(padstate_t *state);
int padstate_set_connstatus(padstate_t *state, conn_status_e new_status);
int padstate_get_actstate(padstate_t *state, uint8_t act_id, bool *act_val);
int padstate_signal_update(padstate_t *state);
int padstate_change_level(padstate_t *state, arm_lvl_e new_arm);
int pad_actuate(padstate_t *state, uint8_t id, uint8_t req_state);

#endif // _STATE_H_
