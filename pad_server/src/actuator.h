#ifndef _ACTUATOR_H_
#define _ACTUATOR_H_

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

/* Agreed upon actuator IDs. */
typedef enum {
    ID_FIRE_VALVE = 0,
    ID_XV1 = 1,
    ID_XV2 = 2,
    ID_XV3 = 3,
    ID_XV4 = 4,
    ID_XV5 = 5,
    ID_XV6 = 6,
    ID_XV7 = 7,
    ID_XV8 = 8,
    ID_XV9 = 9,
    ID_XV10 = 10,
    ID_XV11 = 11,
    ID_XV12 = 12,
    ID_QUICK_DISCONNECT = 13,
    ID_IGNITER = 14,
} act_id_e;

/* Forward reference */
typedef struct actuator actuator_t;

/* Function for controlling the actuator. */
typedef int (*actuate_f)(struct actuator *act);

/*
 * Represents an actuator in the control system.
 * Could be a valve, servo, etc.
 */
typedef struct actuator {
    act_id_e id;       /* The unique numeric ID of the actuator */
    atomic_bool state; /* The actuator state, true being on and false being off*/
    actuate_f on;      /* Function to turn the actuator on. */
    actuate_f off;     /* Function to turn the actuator off. */
    void *priv;        /* Any private information needed by the actuator control functions */
} actuator_t;

int actuator_on(actuator_t *act);
int actuator_off(actuator_t *act);
void actuator_init(actuator_t *act, uint8_t id, actuate_f on, actuate_f off, void *priv);
int actuator_set(actuator_t *act, bool new_state);
// const char *actuator_name(actuator_t *act); Conflicts with state.h, commenting for now. - Tony

#endif // _ACTUATOR_H_
