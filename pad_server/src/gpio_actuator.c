// BEWARE, this file is expluded from Linux compilation
// because the of the missing nuttx header file

#include "gpio_actuator.h"
#include "stdio.h"
#include <errno.h>
#include <fcntl.h>
#include <nuttx/ioexpander/gpio.h>
#include <sys/ioctl.h>
#include <unistd.h>

int gpio_actuator_on(actuator_t *act) {
    // TODO: change the device to be actuator specific
    int fd = open("/dev/gpio12", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Failed to open gpio with err %d\n", errno);
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

int gpio_actuator_off(actuator_t *act) {
    // TODO: change the device to be actuator specific
    int fd = open("/dev/gpio12", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Failed to open gpio with err %d\n", errno);
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
