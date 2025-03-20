#include "actuator.h"

/*
 * Initialize a GPIO actuator.
 * @param act The actuator structure to initialize.
 * @param id The actuator ID
 * @param dev The path to the GPIO character device
 */
void gpio_actuator_init(actuator_t *act, uint8_t id, const char *dev);
