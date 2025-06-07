/* WARNING: This file is excluded from Linux compilation because the of the missing NuttX header file
 *
 * WARNING: The implementation for this actuator is fairly specific to the servo hardware and RP2040 hardware. The
 * multichannel option for the RP2040 must be enabled, there is no conditional compilation to handle single channel if
 * that has been configured. The frequency of 4ms was chosen since most servos require a pulse between 1ms and 2ms, with
 * 1ms being closed (0 degrees) and 2ms being open (180 degrees or 360 or whatever). However, with hardware testing,
 * sometimes there has been variance (i.e. 0.6ms and 2.2ms), so 4 was chosen to give more flexibility.
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <nuttx/timers/pwm.h>

#include "../../debugging/logging.h"
#include "actuator.h"
#include "pwm_actuator.h"

/* Period of 4ms */
#define DEFAULT_PERIOD 0.004                   /* Seconds */
#define DEFAULT_PERIOD_US 2000                 /* Seconds */
#define DEFAULT_FREQUENCY (1 / DEFAULT_PERIOD) /* Hz */

/*
 * Sends a PWM signal to the device based on the open or close pulse duration.
 * @param act The PWM actuator to send the signal to
 * @param open True to send an open pulse, false to send a close pulse
 * @return 0 on success, error code on failure
 */
static int pwm_send_signal(actuator_t *act, bool open_act) {
    int fd;
    int err;
    pwm_actinfo_t *priv = (pwm_actinfo_t *)act->priv;
    struct pwm_info_s pwm_config;

    /* Open the PWM actuator file */

    fd = open(priv->dev, O_RDWR);
    if (fd < 0) {
        return errno;
    }

    /* Get the configuration */

    hinfo("Got characteristics\n");
    err = ioctl(fd, PWMIOC_GETCHARACTERISTICS, &pwm_config);
    if (err < 0) {
        return errno;
    }

    hinfo("Freq: %lu\n", pwm_config.frequency);
    hinfo("Duty 0: %lu\n", pwm_config.channels[0].duty);
    hinfo("Duty 1: %lu\n", pwm_config.channels[1].duty);

    pwm_config.frequency = DEFAULT_FREQUENCY;

    /* Configure only the active channel */

    pwm_config.channels[priv->channel].channel = priv->channel;
    pwm_config.channels[priv->channel].cpol = 1;
    pwm_config.channels[priv->channel].dcpol = 0;

    /* Choose open or close pulse duration */

    if (open_act) {
        pwm_config.channels[priv->channel].duty = priv->open_duty;
    } else {
        pwm_config.channels[priv->channel].duty = priv->close_duty;
    }

    hinfo("Freq: %lu\n", pwm_config.frequency);
    hinfo("duty 0: %lu\n", pwm_config.channels[0].duty);
    hinfo("duty 1: %lu\n", pwm_config.channels[1].duty);

    /* Set the configuration */

    err = ioctl(fd, PWMIOC_SETCHARACTERISTICS, &pwm_config);
    if (err < 0) {
        return errno;
    }
    hinfo("Set characteristics\n");

    /* Turn on the PWM signal */

    err = ioctl(fd, PWMIOC_START, NULL);
    if (err < 0) {
        return errno;
    }
    hinfo("Started PWM\n");

    return err;
}

/*
 * Turn on the PWM actuator.
 * @param act The PWM actuator to turn on
 * @return 0 on success, error code on failure
 */
static int pwm_actuator_on(actuator_t *act) { return pwm_send_signal(act, false); }

/*
 * Turn off the PWM actuator.
 * @param act The PWM actuator to turn off
 * @return 0 on success, error code on failure
 */
static int pwm_actuator_off(actuator_t *act) { return pwm_send_signal(act, true); }

/*
 * Initialize a PWM actuator.
 * @param act The actuator structure to initialize.
 * @param id The actuator ID
 * @param info The information describing the PWM device settings
 */
void pwm_actuator_init(actuator_t *act, uint8_t id, const pwm_actinfo_t *info) {
    actuator_init(act, id, pwm_actuator_on, pwm_actuator_off, (void *)info);
}
