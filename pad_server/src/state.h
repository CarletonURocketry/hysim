#ifndef _STATE_H_
#define _STATE_H_

#include "../../packets/packet.h"
#include <pthread.h>
#include <stdbool.h>

/* Number of actuators in the system: 12 solenoid valves, 1 fire valve, 1 quick disconnect */
#define NUM_ACTUATORS (12 + 1 + 1)

/* State of the entire pad control system */
typedef struct {
    bool actuators[NUM_ACTUATORS];
    arm_lvl_e arm_level;
    pthread_mutex_t lock;
} padstate_t;

void padstate_init(padstate_t *state);
int padstate_change_level(padstate_t *state, arm_lvl_e new_arm);
int padstate_actuate(padstate_t *state, uint8_t id, bool new_state);

const char *actuator_name(uint8_t id);

#endif // _STATE_H_
