#ifndef _ACTUATOR_H_
#define _ACTUATOR_H_

#include <stdint.h>

/* Forward reference */
typedef struct actuator actuator_t;

/* Function for controlling the actuator. */
typedef int (*actuate_f)(struct actuator *act);

/*
 * Represents an actuator in the control system.
 * Could be a valve, servo, etc.
 */
typedef struct actuator {
    uint8_t id;    /* The unique numeric ID of the actuator */
    actuate_f on;  /* Function to turn the actuator on. */
    actuate_f off; /* Function to turn the actuator off. */
    void *priv;    /* Any private information needed by the actuator control functions */
} actuator_t;

int actuator_on(actuator_t *act);
int actuator_off(actuator_t *act);
void actuator_init(actuator_t *act, uint8_t id, actuate_f on, actuate_f off, void *priv);

#endif // _ACTUATOR_H_
