/* WARNING: This file is expluded from Linux compilation because the of the missing NuttX header file */

#include "gpio_actuator.h"
#include "actuator.h"
#include "stdio.h"
#include <errno.h>
#include <fcntl.h>
#include <nuttx/ioexpander/gpio.h>
#include <sys/ioctl.h>
#include <unistd.h>

/*
 * Turn on a GPIO actuator.
 * @param act The actuator to turn on.
 * @return 0 on success, an error code on failure.
 */
static int gpio_actuator_on(actuator_t *act) {
    int fd = open((char *)act->priv, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Failed to open gpio '%s' with err %d\n", (char *)act->priv, errno);
        return -1;
    }

    int err = ioctl(fd, GPIOC_WRITE, true);
    if (err < 0) {
        fprintf(stderr, "Failed to communicate via ioctl with err %d\n", errno);
        return -1;
    }

    if (close(fd) == -1) {
        fprintf(stderr, "Failed to close gpio with err %d\n", errno);
        return -1;
    }

    fprintf(stdout, "Actuator #%d turned on\n", act->id);
    return 0;
}

/*
 * Turn off a GPIO actuator.
 * @param act The actuator to turn off.
 * @return 0 on success, an error code on failure.
 */
static int gpio_actuator_off(actuator_t *act) {

    int fd = open((char *)act->priv, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Failed to open gpio '%s' with err %d\n", (char *)act->priv, errno);
        return -1;
    }

    int err = ioctl(fd, GPIOC_WRITE, false);
    if (err < 0) {
        fprintf(stderr, "Failed to communicate via ioctl with err %d\n", errno);
        return -1;
    }

    if (close(fd) == -1) {
        fprintf(stderr, "Failed to close gpio with err %d\n", errno);
        return -1;
    }

    fprintf(stdout, "Actuator #%d turned off\n", act->id);
    return 0;
}

/*
 * Initialize a GPIO actuator.
 * @param act The actuator structure to initialize.
 * @param id The actuator ID
 * @param dev The path to the GPIO character device
 */
void gpio_actuator_init(actuator_t *act, uint8_t id, const char *dev) {
    actuator_init(act, id, gpio_actuator_on, gpio_actuator_off, (char *)dev);
}
