#include "gpio_actuator.h"
#include "stdio.h"

/*
 * Turn on a GPIO actuator.
 * @param act The actuator to turn on.
 * @return 0 on success, an error code on failure.
 */
static int gpio_actuator_on(actuator_t *act) {
    printf("Dummy actuator #%d turned on\n", act->id);
    return 0;
}

/*
 * Turn off a GPIO actuator.
 * @param act The actuator to turn off.
 * @return 0 on success, an error code on failure.
 */
static int gpio_actuator_off(actuator_t *act) {
    printf("Dummy actuator #%d turned off\n", act->id);
    return 0;
}

/*
 * Initialize a GPIO actuator.
 * @param act The actuator structure to initialize.
 * @param id The actuator ID
 * @param dev The path to the GPIO character device (unused)
 */
void gpio_actuator_init(actuator_t *act, uint8_t id, const char *dev) {
    (void)(dev);
    actuator_init(act, id, gpio_actuator_on, gpio_actuator_off, NULL);
}
