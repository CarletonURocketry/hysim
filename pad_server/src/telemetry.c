#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#include "state.h"
#include "telemetry.h"

/* Helper function for returning an error code from a thread */
#define thread_return(e) pthread_exit((void *)(unsigned long)((e)))

/*
 * Set up the telemetry socket for connection.
 * @param sock The telemetry socket to initialize.
 * @param port The port number to use to accept connections.
 * @return 0 for success, error code on failure.
 */
static int telemetry_init(telemetry_sock_t *sock, uint16_t port, char *addr) {

    /* Initialize the socket connection. */
    sock->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock->sock < 0) return errno;

    /* Create address */
    sock->addr.sin_family = AF_INET;
    sock->addr.sin_addr.s_addr = inet_addr(addr);
    sock->addr.sin_port = htons(port);

    return 0;
}

/*
 * Closes a telemetry socket.
 * @param arg A pointer to a telemetry socket.
 * @return 0 on success, error code on error.
 */
static int telemetry_close(telemetry_sock_t *sock) {
    if (close(sock->sock) < 0) {
        return errno;
    }
    return 0;
}

/*
 * Publish a telemetry message to all listeners.
 * @param sock The telemetry socket on which to publish.
 * @param msg The message to send.
 * @return 0 for success, error code on failure.
 */
static int telemetry_publish(telemetry_sock_t *sock, struct msghdr *msg) {
    msg->msg_name = &sock->addr;
    msg->msg_namelen = sizeof(sock->addr);
    sendmsg(sock->sock, msg, MSG_NOSIGNAL);
    return 0;
}

/*
 * pthread cleanup handler for telemetry socket.
 * @param arg A pointer to a telemetry socket.
 * @return 0 on success, error code on error.
 */
static void telemetry_cleanup(void *arg) { telemetry_close((telemetry_sock_t *)(arg)); }

/*
 * Cleanup function to kill a thread.
 * @param arg A pointer to the pthread_t thread handle.
 */
static void cancel_wrapper(void *arg) { pthread_cancel(*(pthread_t *)(arg)); }

/*
 * A function to publish pressure, tempereature, mass data.
 * @param sock The telemetry socket on which to publish.
 * @param type The telemetry type is being published.
 * @param id The id of that data
 * @param time Time of the data
 * @param press_temp_mass The actual data being sent, could be pressure, temperatur or mass
 */
static void telemetry_publish_data(telemetry_sock_t *sock, telem_subtype_e type, uint8_t id, uint32_t time,
                                   uint32_t press_temp_mass) {
    header_p hdr = {.type = TYPE_TELEM, .subtype = type};
    pressure_p pressureBody = {.id = id, .time = time, .pressure = press_temp_mass};
    temp_p temperatureBody = {.id = id, .time = time, .temperature = press_temp_mass};
    mass_p massBody = {.id = id, .time = time, .mass = press_temp_mass};

    struct iovec pkt[2] = {
        {.iov_base = &hdr, .iov_len = sizeof(hdr)},
    };

    /*Create the appropriate body base on type*/
    struct iovec tmp;
    switch (type) {
    case TELEM_PRESSURE: {
        tmp.iov_base = &pressureBody;
        tmp.iov_len = sizeof(pressureBody);
        pkt[1] = tmp;
        break;
    }
    case TELEM_MASS: {
        tmp.iov_base = &massBody;
        tmp.iov_len = sizeof(massBody);
        pkt[1] = tmp;
        break;
    }
    case TELEM_TEMP: {
        tmp.iov_base = &temperatureBody;
        tmp.iov_len = sizeof(temperatureBody);
        pkt[1] = tmp;
        break;
    }
    default:
        printf("Invalid type");
        thread_return(0);
    }
    struct msghdr msg = {
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = pkt,
        .msg_iovlen = (sizeof(pkt) / sizeof(struct iovec)),
        .msg_control = NULL,
        .msg_controllen = 0,
        .msg_flags = 0,
    };
    telemetry_publish(sock, &msg);
    usleep(1000);
}

/*
 * A function to create random data if not put in any file to read from
 * @params arg The arguent to run the telemetry thread
 */
static void random_data(telemetry_sock_t *telem) {

    uint32_t time = 0;
    uint32_t pressure = 0;
    uint32_t temperature = 0;
    uint32_t mass = 4000;
    /* Start transmitting telemetry to active clients */
    for (;;) {
        pressure = (pressure + 1) % 255;
        telemetry_publish_data(telem, TELEM_PRESSURE, 1, time, 100 + pressure * 10);
        telemetry_publish_data(telem, TELEM_PRESSURE, 2, time, 200 + pressure * 20);
        telemetry_publish_data(telem, TELEM_PRESSURE, 3, time, 300 + pressure * 30);
        telemetry_publish_data(telem, TELEM_PRESSURE, 4, time, 250 + pressure * 40);

        temperature = (temperature + 1) % 20 + 20;
        telemetry_publish_data(telem, TELEM_TEMP, 1, time, temperature - 1);
        telemetry_publish_data(telem, TELEM_TEMP, 2, time, temperature + 1);
        telemetry_publish_data(telem, TELEM_TEMP, 3, time, temperature - 2);
        telemetry_publish_data(telem, TELEM_TEMP, 4, time, temperature + 2);

        mass = (mass + 10) % 4000 + 3900;
        telemetry_publish_data(telem, TELEM_MASS, 1, time, temperature + 2);

        time = (time + 1) % 1000000;
        usleep(1000000);
    }

    thread_return(0);
}

/*
 * Run the thread responsible for transmitting telemetry data.
 * @param arg The arguments to the telemetry thread, of type `telemetry_args_t`.
 */
void *telemetry_run(void *arg) {

    telemetry_args_t *args = (telemetry_args_t *)(arg);
    char buffer[BUFSIZ];
    int err;

    /* Start telemetry socket */
    telemetry_sock_t telem;
    err = telemetry_init(&telem, args->port, args->addr);
    if (err) {
        fprintf(stderr, "Could not start telemetry socket: %s\n", strerror(err));
        thread_return(err);
    }
    pthread_cleanup_push(telemetry_cleanup, &telem);

    pthread_t telemetry_padstate_thread;
    telemetry_padstate_args_t telemetry_padstate_args = {.sock = &telem, .state = args->state};
    err = pthread_create(&telemetry_padstate_thread, NULL, telemetry_send_padstate, &telemetry_padstate_args);
    if (err) {
        fprintf(stderr, "Could not start telemetry padstate update thread: %s\n", strerror(err));
        // pthread_cleanup_pop(1);
    }

    /* Null telemetry file means nothing to do */
    if (args->data_file == NULL) {
        random_data(&telem);
    }

    /* Open telemetry file */
    FILE *data = fopen(args->data_file, "r");
    if (data == NULL) {
        fprintf(stderr, "Could not open telemetry file \"%s\" with error: %s\n", args->data_file, strerror(errno));
        thread_return(err);
    }

    /* Start transmitting telemetry to active clients */
    for (;;) {

        /* Handle file errors */
        if (ferror(data)) {
            fprintf(stderr, "Error reading telemetry file: %s\n", strerror(errno));
            thread_return(err);
        }

        /* Read from file in a loop */
        if (feof(data)) {
            rewind(data);
        }

        /* Read next line */
        if (fgets(buffer, sizeof(buffer), data) == NULL) {
            continue;
        }

        /* TODO: parse all data */

        char *rest = buffer;
        char *time_str = strtok_r(rest, ",", &rest);
        uint32_t time = strtoul(time_str, NULL, 10);
        char *mass_str = strtok_r(rest, ",", &rest);
        uint32_t mass = strtoul(mass_str, NULL, 10);
        char *pstr = strtok_r(rest, ",", &rest);
        uint32_t pressure = strtoul(pstr, NULL, 10);

        header_p hdr = {.type = TYPE_TELEM, .subtype = TELEM_PRESSURE};
        pressure_p body = {.id = 1, .time = time, .pressure = pressure};

        struct iovec pkt[2] = {
            {.iov_base = &hdr, .iov_len = sizeof(hdr)},
            {.iov_base = &body, .iov_len = sizeof(body)},
        };
        struct msghdr msg = {
            .msg_iov = pkt,
            .msg_iovlen = (sizeof(pkt) / sizeof(struct iovec)),
        };
        telemetry_publish(&telem, &msg);
        usleep(1000000);
    }

    thread_return(0); // Normal return

    pthread_cleanup_pop(1);
}

void *telemetry_send_padstate(void *arg) {
    telemetry_padstate_args_t *args = (telemetry_padstate_args_t *)arg;

    for (;;) {
        struct timespec time;
        clock_gettime(CLOCK_MONOTONIC, &time);
        uint32_t time_ms = time.tv_sec * 1000 + time.tv_nsec / 1000000;

        // sending arming update
        header_p hdr = {.type = TYPE_TELEM, .subtype = TELEM_ARM};
        arm_lvl_e arm_lvl;
        padstate_get_level(args->state, &arm_lvl);
        arm_state_p body = {.time = time_ms, .state = arm_lvl};

        struct iovec pkt[2] = {
            {.iov_base = &hdr, .iov_len = sizeof(hdr)},
            {.iov_base = &body, .iov_len = sizeof(body)},
        };
        struct msghdr msg = {
            .msg_iov = pkt,
            .msg_iovlen = (sizeof(pkt) / sizeof(struct iovec)),
        };
        telemetry_publish(args->sock, &msg);

        // sending actuator update
        for (int i = 0; i < NUM_ACTUATORS; i++) {
            header_p hdr = {.type = TYPE_TELEM, .subtype = TELEM_ACT};
            bool act_state;
            padstate_get_actstate(args->state, i, &act_state);
            act_state_p body = {.time = time_ms, .id = i, .state = act_state};

            struct iovec pkt[2] = {
                {.iov_base = &hdr, .iov_len = sizeof(hdr)},
                {.iov_base = &body, .iov_len = sizeof(body)},
            };
            struct msghdr msg = {
                .msg_iov = pkt,
                .msg_iovlen = (sizeof(pkt) / sizeof(struct iovec)),
            };
            telemetry_publish(args->sock, &msg);
        }

        pthread_mutex_lock(&padstate_last_updated.mut);

        struct timespec cond_timeout;
        clock_gettime(CLOCK_REALTIME, &cond_timeout);
        cond_timeout.tv_sec += PADSTATE_UPDATE_TIMEOUT_SEC;

        int err;
        // waiting until cond_timedwait times out
        while ((err = pthread_cond_timedwait(&padstate_last_updated.cond, &padstate_last_updated.mut, &cond_timeout)) ==
               0) {

            clock_gettime(CLOCK_MONOTONIC, &time);
            time_ms = time.tv_sec * 1000 + time.tv_nsec / 1000000;

            if (padstate_last_updated.target == ACT) {
                header_p hdr = {.type = TYPE_TELEM, .subtype = TELEM_ACT};
                act_state_p body = {
                    .time = time_ms, .id = padstate_last_updated.act_id, .state = padstate_last_updated.act_val};

                struct iovec pkt[2] = {
                    {.iov_base = &hdr, .iov_len = sizeof(hdr)},
                    {.iov_base = &body, .iov_len = sizeof(body)},
                };
                struct msghdr msg = {
                    .msg_iov = pkt,
                    .msg_iovlen = (sizeof(pkt) / sizeof(struct iovec)),
                };
                telemetry_publish(args->sock, &msg);

            } else if (padstate_last_updated.target == ARM) {
                header_p hdr = {.type = TYPE_TELEM, .subtype = TELEM_ARM};
                arm_state_p body = {.time = time_ms, .state = padstate_last_updated.arm_lvl};

                struct iovec pkt[2] = {
                    {.iov_base = &hdr, .iov_len = sizeof(hdr)},
                    {.iov_base = &body, .iov_len = sizeof(body)},
                };
                struct msghdr msg = {
                    .msg_iov = pkt,
                    .msg_iovlen = (sizeof(pkt) / sizeof(struct iovec)),
                };
                telemetry_publish(args->sock, &msg);
            }
        }
        pthread_mutex_unlock(&padstate_last_updated.mut);
    }

    thread_return(0);
}
