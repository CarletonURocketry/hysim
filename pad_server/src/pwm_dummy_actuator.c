/* NOTE: used for desktop builds to mock a PWM actuator. */

#include <stdio.h>

#include "pwm_actuator.h"

/*
 * Turn on a PWM actuator.
 * @param act The actuator to turn on.
 * @return 0 on success, an error code on failure.
 */
static int pwm_actuator_on(actuator_t *act) {
    printf("Dummy PWM actuator #%d turned on\n", act->id);
    return 0;
}

/*
 * Turn off a GPIO actuator.
 * @param act The actuator to turn off.
 * @return 0 on success, an error code on failure.
 */
static int pwm_actuator_off(actuator_t *act) {
    printf("Dummy PWM actuator #%d turned off\n", act->id);
    return 0;
}

/*
 * Initialize a PWM actuator.
 * @param act The actuator structure to initialize.
 * @param id The actuator ID
 * @param dev The path to the PWM character device (unused)
 */
void pwm_actuator_init(actuator_t *act, uint8_t id, const pwm_actinfo_t *info) {
    (void)(info);
    actuator_init(act, id, pwm_actuator_on, pwm_actuator_off, NULL);
}
