#include "actuator.h"

/*
 * Initialize the fields of an actuator.
 * @param act The actuator to initialize.
 * @param on The function to turn the actuator on.
 * @param off The function to turn the actuator off.
 * @param priv The private data the on/off functions need to control the actuator.
 */
void actuator_init(actuator_t *act, uint8_t id, actuate_f on, actuate_f off, void *priv) {
    act->id = id;
    act->on = on;
    act->off = off;
    act->priv = priv;
}

/*
 * TODO: docs
 */
int actuator_on(actuator_t *act) { return act->on(act); }

/*
 * TODO: docs
 */
int actuator_off(actuator_t *act) { return act->off(act); }
