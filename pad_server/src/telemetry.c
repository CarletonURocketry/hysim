#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#include "sensors.h"
#include "state.h"
#include "telemetry.h"

/* Helper macro for dereferencing pointers */

#define deref(type, data) *((type *)(data))

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
    // TODO: handle error from sendmsg
    return 0;
}

/*
 * pthread cleanup handler for telemetry socket.
 * @param arg A pointer to a telemetry socket.
 * @return 0 on success, error code on error.
 */
static void telemetry_cleanup(void *arg) { telemetry_close((telemetry_sock_t *)(arg)); }

static void telemetry_cancel_padstate_thread(void *arg) {
    pthread_t telemetry_padstate_thread = *(pthread_t *)arg;

    pthread_cancel(telemetry_padstate_thread);
    pthread_join(telemetry_padstate_thread, NULL);
    fprintf(stderr, "Telemetry pad state thread terminated\n");
}

/*
 * A function to publish pressure, temperature, mass data.
 * @param sock The telemetry socket on which to publish.
 * @param type The telemetry type is being published.
 * @param id The id of that data
 * @param time Time of the data
 * @param data The actual data being sent, could be pressure, temperature or mass
 */
static void telemetry_publish_data(telemetry_sock_t *sock, telem_subtype_e type, uint8_t id, uint32_t time,
                                   void *data) {

    header_p hdr = {.type = TYPE_TELEM, .subtype = type};
    pressure_p pressure_body;
    mass_p mass_body;
    temp_p temperature_body;
    continuity_state_p continuity_body;

    struct iovec pkt[2] = {
        {.iov_base = &hdr, .iov_len = sizeof(hdr)},
    };

    switch (type) {

    case TELEM_PRESSURE:
        pressure_body.id = id;
        pressure_body.time = time;
        pressure_body.pressure = deref(int32_t, data);
        pkt[1].iov_base = &pressure_body;
        pkt[1].iov_len = sizeof(pressure_body);
        break;

    case TELEM_MASS:
        mass_body.id = id;
        mass_body.time = time;
        mass_body.mass = deref(uint32_t, data);
        pkt[1].iov_base = &mass_body;
        pkt[1].iov_len = sizeof(mass_body);
        break;

    case TELEM_TEMP:
        temperature_body.id = id;
        temperature_body.time = time;
        temperature_body.temperature = deref(int32_t, data);
        pkt[1].iov_base = &temperature_body;
        pkt[1].iov_len = sizeof(temperature_body);
        break;

    case TELEM_CONT:
        continuity_body.time = time;
        continuity_body.state = deref(continuity_state_e, data);
        pkt[1].iov_base = &continuity_body;
        pkt[1].iov_len = sizeof(continuity_body);
        break;

    default:
        fprintf(stderr, "Invalid telemetry data type: %u\n", type);
        return;
    }

    struct msghdr msg = {
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = pkt,
        .msg_iovlen = (sizeof(pkt) / sizeof(pkt[0])),
        .msg_control = NULL,
        .msg_controllen = 0,
        .msg_flags = 0,
    };
    telemetry_publish(sock, &msg);
}

#if defined(DESKTOP_BUILD) || defined(CONFIG_HYSIM_PAD_SERVER_MOCK_DATA)
/* A function to create random data if not put in any file to read from
 * @params telem The telemetry socket to send random data over
 */
static void random_data(telemetry_sock_t *telem) {

    uint32_t time = 0;
    uint32_t pressure = 0;
    uint32_t temperature = 0;
    uint32_t mass = 4000;

    /* Start transmitting telemetry to active clients */
    for (;;) {

        pressure = (pressure + 1) % 255;
        uint32_t pressure_i = 100 + pressure * 10;
        telemetry_publish_data(telem, TELEM_PRESSURE, 1, time, &pressure_i);
        pressure_i = 200 + pressure * 20;
        telemetry_publish_data(telem, TELEM_PRESSURE, 2, time, &pressure_i);
        pressure_i = 300 + pressure * 30;
        telemetry_publish_data(telem, TELEM_PRESSURE, 3, time, &pressure_i);
        pressure_i = 250 + pressure * 40;
        telemetry_publish_data(telem, TELEM_PRESSURE, 4, time, &pressure_i);

        temperature = (temperature + 78) % 20000 + 20000;
        int32_t temp_i = temperature - 1;
        telemetry_publish_data(telem, TELEM_TEMP, 1, time, &temp_i);
        temp_i = 2000 + temperature * 2;
        telemetry_publish_data(telem, TELEM_TEMP, 2, time, &temp_i);
        temp_i = 3000 + temperature * 3;
        telemetry_publish_data(telem, TELEM_TEMP, 3, time, &temp_i);
        temp_i = 2500 + temperature * 4;
        telemetry_publish_data(telem, TELEM_TEMP, 4, time, &temp_i);

        mass = (mass + 10) % 4000 + 3900;
        uint32_t mass_i = mass + 2;
        telemetry_publish_data(telem, TELEM_MASS, 1, time, &mass_i);
        mass_i = mass + 4;
        telemetry_publish_data(telem, TELEM_MASS, 2, time, &mass_i);

        time = (time + 1) % 1000000;
        usleep(100000);
    }

    thread_return(0);
}

/*
 * Generate mock telemetry data from either the provided file or randomly generated data.
 * @param args The telemetry thread arguments
 * @param telem The telemetry socket to output data on
 */
static void mock_telemetry(telemetry_args_t *args, telemetry_sock_t *telem) {
    int err = 0;
    char buffer[BUFSIZ];

    /* NULL telemetry file means generate random data */

    if (args->data_file == NULL) {
        random_data(telem);
    } else {

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
            telemetry_publish(telem, &msg);
            usleep(1000000);
        }
    }
}
#endif

#ifndef DESKTOP_BUILD
/*
 * Thread logic responsible for reading data from sensors and publishing the data as telemetry.
 * NOTE: only works on NuttX builds
 * @param args The telemetry thread arguments
 * @param telem The telemetry socket to output data on
 */
static void sensor_telemetry(telemetry_args_t *args, telemetry_sock_t *telem) {
    int err;
#if defined(CONFIG_SENSORS_NAU7802) && defined(CONFIG_ADC_ADS1115)

    sensor_mass_t sensor_mass = {
        .known_mass_grams = SENSOR_MASS_KNOWN_WEIGHT,
        .known_mass_point = SENSOR_MASS_KNOWN_POINT,
        .available = true,
    };

    err = sensor_mass_init(&sensor_mass);
    if (err < 0) {
        fprintf(stderr, "Could not initialize mass sensor: %d\n", err);
        sensor_mass.available = false;
    }

    if (sensor_mass.available) {
        err = sensor_mass_calibrate(&sensor_mass);
        if (err < 0) {
            fprintf(stderr, "Could not calibrate mass sensor: %d\n", err);
            sensor_mass.available = false;
        }
    }

    /* Channel numbers go from 0 to 8, 0-7 are differential between the pins, 4-7 are single ended */

    adc_device_t adc_devices[] = {
        {
            .id = 0,
            .fd = -1,
            .devpath = "/dev/adc0", /* 0x48 */
            .n_channels = 4,
            .channels =
                {
                    {.channel_num = 4, .sensor_id = 0, .type = TELEM_TEMP},
                    {.channel_num = 5, .sensor_id = 1, .type = TELEM_TEMP},
                    {.channel_num = 6, .sensor_id = 4, .type = TELEM_PRESSURE},
                    {.channel_num = 7, .sensor_id = 5, .type = TELEM_PRESSURE},
                },
        },
        {
            .id = 1,
            .fd = -1,
            .devpath = "/dev/adc1", /* 0x49 */
            .n_channels = 4,
            .channels =
                {
                    {.channel_num = 4, .sensor_id = 0, .type = TELEM_PRESSURE},
                    {.channel_num = 5, .sensor_id = 1, .type = TELEM_PRESSURE},
                    {.channel_num = 6, .sensor_id = 2, .type = TELEM_PRESSURE},
                    {.channel_num = 7, .sensor_id = 3, .type = TELEM_PRESSURE},
                },
        },
        {
            .id = 2,
            .fd = -1,
            .devpath = "/dev/adc2", /* 0x4A */
            .n_channels = 2,
            .channels =
                {
                    {.channel_num = 4, .sensor_id = 0, .type = TELEM_MASS},
                    {.channel_num = 6, .sensor_id = 1, .type = TELEM_CONT},
                },
        },
    };

    for (int i = 0; i < sizeof(adc_devices) / sizeof(adc_devices[0]); i++) {
        adc_devices[i].fd = open(adc_devices[i].devpath, O_RDONLY);
        if (adc_devices[i].fd < 0) {
            fprintf(stderr, "Could not open ADC device %s: %s\n", adc_devices[i].devpath, strerror(errno));
        }
    }

    for (;;) {
        struct timespec time_t;
        clock_gettime(CLOCK_MONOTONIC, &time_t);
        uint32_t time_ms = time_t.tv_sec * 1000 + time_t.tv_nsec / 1000000;

        for (int i = 0; i < sizeof(adc_devices) / sizeof(adc_devices[0]); i++) {

            if (adc_devices[i].fd < 0) {
                continue;
            }

            err = adc_trigger_conversion(&adc_devices[i]);
            if (err < 0) {
                fprintf(stderr, "Failed to trigger ADC conversion on id %d: %d\n", adc_devices[i].id, err);
                continue;
            }

            err = adc_read_value(&adc_devices[i]);
#ifdef CONFIG_SYSTEM_NSH
            if (err < 0) {
                fprintf(stderr, "Failed to read ADC value from id %d: %d\n", adc_devices[i].id, err);
                continue;
            }
#endif

            for (int j = 0; j < adc_devices[i].n_channels; j++) {
                adc_channel_t channel = adc_devices[i].channels[j];

                int32_t adc_val = adc_devices[i].sample[channel.channel_num].am_data;
                /* For each channel of data we find the corresponding information */
                int32_t sensor_val = 0;
                err = adc_sensor_val_conversion(&channel, adc_val, &sensor_val);
#ifdef CONFIG_SYSTEM_NSH
                if (err < 0) {
                    fprintf(stderr, "ADC %d pin %d: failed to convert value\n", i, channel.channel_num - 4);
                    continue;
                }
                fprintf(stderr, "ADC %d pin %d value %ld\n", i, channel.channel_num - 4, adc_val);
#endif

                telemetry_publish_data(telem, channel.type, channel.sensor_id, time_ms, (void *)&sensor_val);
            }
#ifdef CONFIG_SYSTEM_NSH
            printf("\n");
#endif
        }

        if (sensor_mass.available) {
            err = sensor_mass_fetch(&sensor_mass);
            if (err < 0) {
                fprintf(stderr, "Error fetching mass data: %d\n", err);
            } else {
                /* I made the id of this one 3, check on this*/
                telemetry_publish_data(telem, TELEM_MASS, 3, time_ms, (void *)&sensor_mass.data.force);
            }
        }
    }

#endif
}
#endif

/*
 * Run the thread responsible for transmitting telemetry data.
 * @param arg The arguments to the telemetry thread, of type `telemetry_args_t`.
 */
void *telemetry_run(void *arg) {
    telemetry_args_t *args = (telemetry_args_t *)(arg);
    int err;

    /* Start telemetry socket */

    telemetry_sock_t telem;
    err = telemetry_init(&telem, args->port, args->addr);
    if (err) {
        fprintf(stderr, "Could not start telemetry socket: %s\n", strerror(err));
        thread_return(err);
    }
    pthread_cleanup_push(telemetry_cleanup, &telem);

    /* Start thread to periodically update telemetry stream with the pad state */

    pthread_t telemetry_padstate_thread;
    telemetry_padstate_args_t telemetry_padstate_args = {.sock = &telem, .state = args->state};
    err = pthread_create(&telemetry_padstate_thread, NULL, telemetry_update_padstate, &telemetry_padstate_args);
    if (err) {
        fprintf(stderr, "Could not start telemetry padstate sending thread: %s\n", strerror(err));
        thread_return(err);
    }
    pthread_cleanup_push(telemetry_cancel_padstate_thread, &telemetry_padstate_thread);

#if defined(DESKTOP_BUILD) || defined(CONFIG_HYSIM_PAD_SERVER_MOCK_DATA)
    /* Start mock telemetry if on desktop build or if we want mock data during */

    mock_telemetry(args, &telem);
#elif !defined(CONFIG_HYSIM_PAD_SERVER_MOCK_DATA) && !defined(DESKTOP_BUILD)

    /* Start real telemetry if on NuttX and not mocking. */
    sensor_telemetry(args, &telem);
#endif

    thread_return(0); // Normal return

    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
}

/*
 * Helper function to send the entire padstate over telemetry
 * @param state the pad state
 * @param sock the telemetry socket
 */
void telemetry_send_padstate(padstate_t *state, telemetry_sock_t *sock) {
    struct timespec time;
    header_p hdr;
    struct iovec pkt[2];
    struct msghdr msg = {
        .msg_iov = pkt,
        .msg_iovlen = (sizeof(pkt) / sizeof(struct iovec)),
    };

    /* Find the current time in milliseconds */

    clock_gettime(CLOCK_MONOTONIC, &time);
    uint32_t time_ms = time.tv_sec * 1000 + time.tv_nsec / 1000000;

    /* Send arming update */

    arm_lvl_e arm_lvl = padstate_get_level(state);
    arm_state_p arm_body = {.time = time_ms, .state = arm_lvl};
    hdr = (header_p){.type = TYPE_TELEM, .subtype = TELEM_ARM};

    pkt[0] = (struct iovec){.iov_base = &hdr, .iov_len = sizeof(hdr)};
    pkt[1] = (struct iovec){.iov_base = &arm_body, .iov_len = sizeof(arm_body)};

    telemetry_publish(sock, &msg);

    /* Send actuator updates */

    for (int i = 0; i < NUM_ACTUATORS; i++) {
        hdr = (header_p){.type = TYPE_TELEM, .subtype = TELEM_ACT};
        bool act_state;
        padstate_get_actstate(state, i, &act_state);
        act_state_p act_body = {.time = time_ms, .id = i, .state = act_state};

        pkt[0] = (struct iovec){.iov_base = &hdr, .iov_len = sizeof(hdr)};
        pkt[1] = (struct iovec){.iov_base = &act_body, .iov_len = sizeof(act_body)};

        telemetry_publish(sock, &msg);
    }
}

/*
 * Thread which periodically sends information about the pad's state.
 * @param arg Arguments of type `telemetry_padstate_args_t`
 * @return 0 on success, error code on failure (thread dies)
 */
void *telemetry_update_padstate(void *arg) {
    telemetry_padstate_args_t *args = (telemetry_padstate_args_t *)arg;
    padstate_t *state = args->state;

    for (;;) {
        struct timespec cond_timeout;
        clock_gettime(CLOCK_REALTIME, &cond_timeout);
        cond_timeout.tv_sec += PADSTATE_UPDATE_TIMEOUT_SEC;

        int err = -1;
        pthread_mutex_lock(&state->update_mut);
        // waiting until either the cond times out or an update is received
        // and we confirmed it was not a spurious wakeup
        while (err != ETIMEDOUT && !state->update_recorded) {
            err = pthread_cond_timedwait(&state->update_cond, &state->update_mut, &cond_timeout);
        }

        telemetry_send_padstate(state, args->sock);
        state->update_recorded = false;
        pthread_mutex_unlock(&state->update_mut);
    }

    thread_return(0);
}
