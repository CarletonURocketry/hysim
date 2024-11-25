#include "state.h"
#include <stdint.h>

/* String names of the actuators. */
static const char *ACTUATOR_STR[] = {
    [ID_FIRE_VALVE] = "Fire valve",
    [ID_XV1] = "XV-1",
    [ID_XV2] = "XV-2",
    [ID_XV3] = "XV-3",
    [ID_XV4] = "XV-4",
    [ID_XV5] = "XV-5",
    [ID_XV6] = "XV-6",
    [ID_XV7] = "XV-7",
    [ID_XV8] = "XV-8",
    [ID_XV9] = "XV-9",
    [ID_XV10] = "XV-10",
    [ID_XV11] = "XV-11",
    [ID_XV12] = "XV-12",
    [ID_QUICK_DISCONNECT] = "Quick disconnect",
    [ID_IGNITER] = "Igniter",
};

/*
 * Initialize the fields of an actuator.
 * @param act The actuator to initialize.
 * @param on The function to turn the actuator on.
 * @param off The function to turn the actuator off.
 * @param priv The private data the on/off functions need to control the actuator.
 */
void actuator_init(actuator_t *act, uint8_t id, actuate_f on, actuate_f off, void *priv) {
    act->id = id;
    act->state = false;
    act->on = on;
    act->off = off;
    act->priv = priv;
}

/*
 * Turn the actuator on.
 * @param act The actuator to turn on.
 * @return 0 for success, an error code on failure.
 */
int actuator_on(actuator_t *act) { return act->on(act); }

/*
 * Turn the actuator off.
 * @param act The actuator to turn off.
 * @return 0 for an error code on failure.
 */
int actuator_off(actuator_t *act) { return act->off(act); }

/*
 * Get the string name of the actuator.
 * @param act The actuator to get the string name of.
 * @return The string name of the actuator.
 */
const char *actuator_get_name(actuator_t *act) { return ACTUATOR_STR[act->id]; }
