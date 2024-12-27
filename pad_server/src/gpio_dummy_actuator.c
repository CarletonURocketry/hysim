#include "gpio_actuator.h"
#include "stdio.h"

int gpio_actuator_on(actuator_t *act) {
    printf("Dummy actuator #%d turned on\n", act->id);
    return 0;
}
int gpio_actuator_off(actuator_t *act) {
    printf("Dummy actuator #%d turned off\n", act->id);
    return 0;
}
