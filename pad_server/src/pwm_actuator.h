#ifndef _PWM_ACTUATOR_H_
#define _PWM_ACTUATOR_H_

#include "actuator.h"

typedef struct {
    char *dev;           /* The PWM character device path */
    uint8_t channel;     /* The PWM channel of the device */
    uint16_t close_duty; /* The duty cycle to close the device out of 0xffff being 100% */
    uint16_t open_duty;  /* The duty cycle to open the device out of 0xffff being 100% */
} pwm_actinfo_t;

/*
 * Initialize a PWM actuator.
 * @param act The actuator structure to initialize.
 * @param id The actuator ID
 * @param dev The path to the PWM character device
 */
void pwm_actuator_init(actuator_t *act, uint8_t id, const pwm_actinfo_t *info);

#endif // _PWM_ACTUATOR_H_
