#include "actuator.h"
#include "state.h"
#include <errno.h>
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
 * @return 0 for success, an error code on failure.
 */
int actuator_off(actuator_t *act) { return act->off(act); }

/*
 * Get the string name of the actuator.
 * @param act The actuator to get the string name of.
 * @return The string name of the actuator.
 */
const char *actuator_get_name(actuator_t *act) { return ACTUATOR_STR[act->id]; }

/*
 * Set the value of an actuator
 * @param id The actuator id
 * @param req_state The new actuator state
 * @return ACT_OK for success, ACT_DNE for invalid id, ACT_INV for invalid req_state, -1 for errors eith errno being set
 */
int actuator_set(padstate_t *state, uint8_t id, uint8_t req_state) {
    if (id >= NUM_ACTUATORS) return ACT_DNE;

    if (req_state != 0 && req_state != 1) return ACT_INV;
    bool new_state = (bool)req_state;

    arm_lvl_e arm_lvl;
    int err = padstate_get_level(state, &arm_lvl);
    if (err) {
        errno = err;
        return -1;
    }

    bool is_solanoid_valve = id >= ID_XV1 && id <= ID_XV12;

    switch (arm_lvl) {
    case ARMED_PAD:
        return ACT_DENIED;
        break;

    case ARMED_VALVES:
        if (!is_solanoid_valve) {
            return ACT_DENIED;
        }
        break;

    case ARMED_IGNITION:
        if (!is_solanoid_valve && id != ID_QUICK_DISCONNECT) {
            return ACT_DENIED;
        }
        break;

    case ARMED_DISCONNECTED:
        if (!is_solanoid_valve && id != ID_QUICK_DISCONNECT && id != ID_IGNITER) {
            return ACT_DENIED;
        }
        break;

    case ARMED_LAUNCH:
        // every command is available
        break;
    }

    bool current_state;
    err = padstate_get_actuator(state, id, &current_state);
    if (err) {
        errno = err;
        return -1;
    }

    if (new_state == current_state) {
        return ACT_OK;
    }

    err = padstate_actuate(state, id, new_state);
    if (err) {
        errno = err;
        return -1;
    }

    return ACT_OK;
}
