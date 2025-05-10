#include "errno.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef DESKTOP_BUILD
#include <nuttx/ioexpander/gpio.h>
#include <nuttx/usb/cdcacm.h>
#include <sys/boardctl.h>
#include <debug.h>

#if defined(CONFIG_NSH_NETINIT) && !defined(CONFIG_SYSTEM_NSH)
#include "netutils/netinit.h"
#endif

#endif

#include "../../packets/packet.h"
#include "../../pad_server/src/actuator.h"
#include "helptext.h"
#include "pad.h"
#include "switch.h"

#define INTERRUPT_SIGNAL SIGUSR1
#define DEBOUNCE_US 10000

#define array_len(arr) (sizeof(arr) / sizeof(arr[0]))

typedef struct {
    uint8_t level;
} arm_t;

typedef struct {
    char key;
    switch_t *sw;
} command_t;

static switch_t switches[] = {

    /* Actuator switches */

    {.act_id = ID_FIRE_VALVE, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV1, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV2, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV3, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV4, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV5, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV6, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV7, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV8, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV9, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV10, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV11, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV12, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_QUICK_DISCONNECT, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_IGNITER, .state = false, .kind = CNTRL_ACT_REQ},

    /* Arming level commands */

    {.act_id = ARMED_PAD, .state = false, .kind = CNTRL_ARM_REQ},
    {.act_id = ARMED_VALVES, .state = false, .kind = CNTRL_ARM_REQ},
    {.act_id = ARMED_IGNITION, .state = false, .kind = CNTRL_ARM_REQ},
    {.act_id = ARMED_DISCONNECTED, .state = false, .kind = CNTRL_ARM_REQ},
    {.act_id = ARMED_LAUNCH, .state = false, .kind = CNTRL_ARM_REQ},
};

#ifndef DESKTOP_BUILD
static const char *ACTUATOR_IN[] = {
    [ID_XV1] = "/dev/gpio2",         [ID_XV2] = "/dev/gpio3",
    [ID_XV3] = "/dev/gpio4",         [ID_XV4] = "/dev/gpio5",
    [ID_XV5] = "/dev/gpio6",         [ID_XV6] = "/dev/gpio7",
    [ID_XV7] = "/dev/gpio8",         [ID_XV8] = "/dev/gpio9",
    [ID_XV9] = "/dev/gpio10",        [ID_XV10] = "/dev/gpio11",
    [ID_XV11] = "/dev/gpio12",       [ID_XV12] = "/dev/gpio13",
    [ID_IGNITER] = "/dev/gpio28",    [ID_QUICK_DISCONNECT] = "/dev/gpio15",
    [ID_FIRE_VALVE] = "/dev/gpio14",
};

static const char *ARM_IN[] = {
    [ARMED_PAD] = NULL,          [ARMED_VALVES] = "/dev/gpio27", [ARMED_IGNITION] = "/dev/gpio26",
    [ARMED_DISCONNECTED] = NULL, [ARMED_LAUNCH] = NULL,
};
#endif

#ifdef DESKTOP_BUILD
static command_t commands[] = {
    {.key = 'q', .sw = &switches[0]},  {.key = 'w', .sw = &switches[1]},  {.key = 'e', .sw = &switches[2]},
    {.key = 'r', .sw = &switches[3]},  {.key = 't', .sw = &switches[4]},  {.key = 'y', .sw = &switches[5]},
    {.key = 'u', .sw = &switches[6]},  {.key = 'i', .sw = &switches[7]},  {.key = 'p', .sw = &switches[8]},
    {.key = 'a', .sw = &switches[9]},  {.key = 's', .sw = &switches[10]}, {.key = 'd', .sw = &switches[11]},
    {.key = 'f', .sw = &switches[12]}, {.key = 'g', .sw = &switches[13]}, {.key = 'h', .sw = &switches[14]},
    {.key = 'z', .sw = &switches[15]}, {.key = 'x', .sw = &switches[16]}, {.key = 'c', .sw = &switches[17]},
    {.key = 'v', .sw = &switches[18]}, {.key = 'b', .sw = &switches[19]},
};
#endif

uint16_t port = 50001; /* Default port */
pad_t pad;
arm_t arm;

static void handle_term(int sig) {
    (void)sig;
    pad_disconnect(&pad);
    // TODO: current limitation is that you cannot CTRL + C on NuttX since we do not unregister the signal events for
    // each GPIO pin on exit.
    exit(EXIT_SUCCESS);
}

#ifndef DESKTOP_BUILD
static int debounce_read(int fd, unsigned int *final) {
    int err;
    unsigned int prev_value;
    unsigned int value;

    usleep(DEBOUNCE_US); /* Debounce */

    err = ioctl(fd, GPIOC_READ, (unsigned long)((uintptr_t)&prev_value));
    if (err) return err;
    value = !prev_value;

    for (;;) {

        usleep(DEBOUNCE_US); /* Debounce */

        err = ioctl(fd, GPIOC_READ, (unsigned long)((uintptr_t)&value));
        if (err) return err;

        /* If values are the same, then debounce correct */

        if (prev_value == value) {
            *final = value;
            return 0;
        }

        /* Otherwise try again */

        prev_value = value;
    }

    return EIO;
}
#endif

#if !defined(CONFIG_SYSTEM_NSH) && defined(CONFIG_CDCACM_CONSOLE)
/* Starts the NuttX USB serial interface.
 */
static int usb_init(void) {
    struct boardioc_usbdev_ctrl_s ctrl;
    FAR void *handle;
    int ret;
    int usb_fd;

    /* Initialize architecture */

    ret = boardctl(BOARDIOC_INIT, 0);
    if (ret != 0) {
        return ret;
    }

    /* Initialize the USB serial driver */

    ctrl.usbdev = BOARDIOC_USBDEV_CDCACM;
    ctrl.action = BOARDIOC_USBDEV_CONNECT;
    ctrl.instance = 0;
    ctrl.handle = &handle;

    ret = boardctl(BOARDIOC_USBDEV_CONTROL, (uintptr_t)&ctrl);
    if (ret < 0) {
        return ret;
    }

    /* Redirect standard streams to USB console */

    do {
        usb_fd = open("/dev/ttyACM0", O_RDWR);

        /* ENOTCONN means that the USB device is not yet connected, so sleep.
         * Anything else is bad.
         */

        DEBUGASSERT(errno == ENOTCONN);
        usleep(100);
    } while (usb_fd < 0);

    usb_fd = open("/dev/ttyACM0", O_RDWR);

    dup2(usb_fd, 0); /* stdout */
    dup2(usb_fd, 1); /* stdin */
    dup2(usb_fd, 2); /* stderr */

    if (usb_fd > 2) {
        close(usb_fd);
    }
    sleep(1); /* Seems to help ensure first few prints get captured */

    return ret;
}
#endif

int main(int argc, char **argv) {

    char *ip = "127.0.0.1";
    pad.sock = -1;
    int err;
#ifdef DESKTOP_BUILD
    char key;
#else
    struct sigevent notify;
    siginfo_t signal_info;
    sigset_t set;
    unsigned int invalue = 2; /* Invalid start value */
    const char *gpio_dev;
    int fd;
    switch_t *signal_sw;
#endif

#if !defined(DESKTOP_BUILD) && !defined(CONFIG_SYSTEM_NSH)
    boardctl(BOARDIOC_INIT, 0);
#endif

#if !defined(DESKTOP_BUILD) && !defined(CONFIG_SYSTEM_NSH) && defined(CONFIG_CDCACM_CONSOLE)
    if (usb_init()) {
        return EXIT_FAILURE;
    }
    printf("Starting controller...\n");
#endif

#if defined(CONFIG_NSH_NETINIT) && !defined(CONFIG_SYSTEM_NSH)
    netinit_bringup();
#endif

    /* Parse command line options. */

    int c;
    while ((c = getopt(argc, argv, ":a:p:h")) != -1) {
        switch (c) {
        case 'h':
            puts(HELP_TEXT);
            exit(EXIT_SUCCESS);
            break;

        case 'a':
            ip = optarg;
            struct in_addr temp_addr;
            if (inet_pton(AF_INET, ip, &temp_addr) != 1) {
                fprintf(stderr, "Invalid pad_server address %s\n", ip);
                exit(EXIT_FAILURE);
            }
            break;

        case 'p':
            port = atoi(optarg);
            break;
        case '?':
            fprintf(stderr, "Unknown option -%c\n", optopt);
            exit(EXIT_FAILURE);
            break;
        }
    }

    /* Handle keyboard interrupt */

    signal(SIGINT, handle_term);

#ifndef DESKTOP_BUILD
    /* Register all control switches as interrupts */

    notify.sigev_notify = SIGEV_SIGNAL;
    notify.sigev_signo = INTERRUPT_SIGNAL;

    for (int i = 0; i < array_len(switches); i++) {

        if (switches[i].kind == CNTRL_ACT_REQ) {
            gpio_dev = ACTUATOR_IN[switches[i].act_id];
        } else {
            gpio_dev = ARM_IN[switches[i].act_id];
        }

        if (gpio_dev == NULL) {
            fprintf(stderr, "Skipping NULL gpio device.\n");
            continue;
        }

        fd = open(gpio_dev, O_RDWR);
        if (fd < 0) {
            syslog(LOG_ERR, "Couldn't open '%s': %d\n", gpio_dev, errno);
            fprintf(stderr, "Couldn't open '%s': %d\n", gpio_dev, errno);
            return EXIT_FAILURE;
        }

        /* Set up to receive signal */

        notify.sigev_value.sival_ptr = &switches[i];
        syslog(LOG_INFO, "ioctl register call");
        err = ioctl(fd, GPIOC_REGISTER, (unsigned long)&notify);
        if (err < 0) {
            syslog(LOG_ERR, "Failed to register interrupt for %s: %d\n", gpio_dev, errno);
            fprintf(stderr, "Failed to register interrupt for %s: %d\n", gpio_dev, errno);
            close(fd);
            return EXIT_FAILURE;
        }

        syslog(LOG_INFO, "Closed device");
        close(fd);
    }
#endif

    /* Connect to pad indefinitely */

    for (;;) {
        fprintf(stderr, "Waiting for pad...\n");
        err = pad_init(&pad, ip, port);
        if (err) {
            fprintf(stderr, "Could not initialize pad server with error: %s\n", strerror(err));
            exit(EXIT_FAILURE);
        }

        err = pad_connect_forever(&pad);
        if (err) {
            fprintf(stderr, "Could not connect to pad server with error: %s\n", strerror(err));
            exit(EXIT_FAILURE);
        }

        printf("Connection established!\n");

        /* Send messages in a loop */

        for (;;) {

#ifdef DESKTOP_BUILD
            printf("Press key and hit enter: ");
            key = getc(stdin);
            if (key != '\n') {
                while (getc(stdin) != '\n')
                    ;
            }

            for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
                if (commands[i].key == key) {
                    err = switch_callback(commands[i].sw, &pad, !commands[i].sw->state);
                } else if (i == (sizeof(commands) / sizeof(commands[0]) - 1)) {
                    fprintf(stderr, "Invalid key: %c\n", key);
                }
            }
#else
            /* Create a signal set containing the GPIO interrupt signal */

            sigemptyset(&set);
            sigaddset(&set, INTERRUPT_SIGNAL);

            /* Wait for a signal to be sent indefinitely */

            err = sigwaitinfo(&set, &signal_info);

            if (err < 0) {
                err = errno;
                if (err == EAGAIN) {
                    /* Timed out */
                    continue;
                } else {
                    fprintf(stderr, "Failed to wait for interrupt: %d\n", err);
                    continue;
                }
            }

            /* Signal was received, call the correct switch handler for the signal */

            signal_sw = signal_info.si_value.sival_ptr;

            if (signal_sw->kind == CNTRL_ACT_REQ) {
                /* GPIO is from actuator set */
                gpio_dev = ACTUATOR_IN[signal_sw->act_id];
            } else {
                /* GPIO is from arming set */
                gpio_dev = ARM_IN[signal_sw->act_id];
            }

            /* Read the GPIO pin for the real value */

            fd = open(gpio_dev, O_RDONLY);
            if (fd < 0) {
                err = errno;
                fprintf(stderr, "Could not open GPIO '%s': %d\n", gpio_dev, errno);
                continue;
            }

            err = debounce_read(fd, &invalue);
            close(fd);

            if (err < 0) {
                err = errno;
                fprintf(stderr, "Failed to re-read value from %s: %d\n", gpio_dev, errno);
                continue;
            }

            /* Call the correct switch callback. `invalue` is negated because the switches use a pull-up resistor. When
             * open circuit (off), the switch is high. When closed circuit (on) the switch is pulled low. */

            err = switch_callback(signal_sw, &pad, !invalue);
#endif

            /* Print a helpful error message */

            switch (err) {
            case 0:
                printf("Switch actuated successfully\n");
                break;
            case EPERM:
                fprintf(stderr, "Permission denied\n");
                break;
            case EINVAL:
                fprintf(stderr, "Invalid actuator/arming level.\n");
            case ENODEV:
                fprintf(stderr, "No such actuator/arming level exists\n");
                break;
            default:
                fprintf(stderr, "Something went wrong: %d\n", err);
                break;
            }

            if (err == ENOTCONN) {
                /* Try to re-connect */
                break;
            }
        }
    }

    pad_disconnect(&pad);
    return 0;
}
