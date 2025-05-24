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

#include "../../debugging/logging.h"
#include "../../debugging/nxassert.h"
#include "sensors.h"
#include "state.h"
#include "telemetry.h"

/* Helper macro for dereferencing pointers */

#define deref(type, data) *((type *)(data))

#define arr_len(arr) (sizeof(arr) / sizeof(arr[0]))

/* Helper function for returning an error code from a thread */

#define thread_return(e) pthread_exit((void *)(unsigned long)((e)))

/*
 * Set up the telemetry socket for connection.
 * @param sock The telemetry socket to initialize.
 * @param port The port number to use to accept connections.
 * @return 0 for success, error code on failure.
 */
static int telemetry_init(telemetry_sock_t *sock, uint16_t port, char *addr) {

    assert(sock != NULL);
    assert(addr != NULL);

    /* Initialize the socket connection. */

    sock->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock->sock < 0) {
        herr("Failed to create telemetry UDP socket\n");
        nxfail("Failed to create telemetry UDP socket");
        return errno;
    }

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
        herr("Failed to close telemetry socket\n");
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
    herr("Telemetry pad state thread terminated\n");
}

#if defined(DESKTOP_BUILD) || defined(CONFIG_HYSIM_PAD_SERVER_MOCK_DATA)
/* A function to create random data if not put in any file to read from
 * @params telem The telemetry socket to send random data over
 */
static void random_data(telemetry_sock_t *telem) {
    printf("Sorry, no random data for you\n");
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

    assert(args != NULL);
    assert(telem != NULL);

#if defined(CONFIG_SENSORS_NAU7802)
    sensor_mass_t sensor_mass = {
        .known_mass_grams = SENSOR_MASS_KNOWN_WEIGHT,
        .known_mass_point = SENSOR_MASS_KNOWN_POINT,
        .available = true,
    };

    err = sensor_mass_init(&sensor_mass);
    if (err < 0) {
        herr("Could not initialize mass sensor: %d\n", err);
        sensor_mass.available = false;
    }

    if (sensor_mass.available) {
        err = sensor_mass_calibrate(&sensor_mass);
        if (err < 0) {
            herr("Could not calibrate mass sensor: %d\n", err);
            sensor_mass.available = false;
        }
    }

    hinfo("NAU7802 mass sensor inititialized.\n");
#endif

#if defined(CONFIG_ADC_ADS1115)
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
                    {.channel_num = 4, .sensor_id = 0, .type = TELEM_THRUST},
                    {.channel_num = 6, .sensor_id = 1, .type = TELEM_CONT},
                },
        },
    };

    /* Open ADC device file descriptors */

    for (int i = 0; i < arr_len(adc_devices); i++) {
        adc_devices[i].fd = open(adc_devices[i].devpath, O_RDONLY);
        if (adc_devices[i].fd < 0) {
            herr("Could not open ADC device %s: %s\n", adc_devices[i].devpath, strerror(errno));
        }

        hinfo("Initialized ADC device %s\n", adc_devices[i].devpath);
    }
#endif

    for (;;) {
        /* We hopefully won't go beyond 32 sensors */
        struct iovec pkt[(32) * 2];
        int sensor_count = 0;

        struct timespec time_t;
        clock_gettime(CLOCK_MONOTONIC, &time_t);
        uint32_t time_ms = time_t.tv_sec * 1000 + time_t.tv_nsec / 1000000;

        header_p headers[32];
        union {
            pressure_p pressure;
            mass_p mass;
            temp_p temp;
            thrust_p thrust;
            continuity_state_p continuity;
        } bodies[32];

#if defined(CONFIG_ADC_ADS1115)
        for (int i = 0; i < arr_len(adc_devices); i++) {

            if (adc_devices[i].fd < 0) {
                continue;
            }

            err = adc_trigger_conversion(&adc_devices[i]);
            if (err < 0) {
                herr("Failed to trigger ADC conversion on id %d: %d\n", adc_devices[i].id, err);
                continue;
            }

            err = adc_read_value(&adc_devices[i]);
            for (int j = 0; j < adc_devices[i].n_channels; j++) {
                adc_channel_t channel = adc_devices[i].channels[j];

                int32_t adc_val = adc_devices[i].sample[channel.channel_num].am_data;

                /* For each channel of data we find the corresponding information */

                int32_t sensor_val = 0;
                err = adc_sensor_val_conversion(&channel, adc_val, &sensor_val);

                headers[sensor_count] = (header_p){.type = TYPE_TELEM, .subtype = channel.type};
                pkt[sensor_count * 2] =
                    (struct iovec){.iov_base = &headers[sensor_count], .iov_len = sizeof(headers[sensor_count])};

                switch (channel.type) {
                case TELEM_PRESSURE:
                    bodies[sensor_count].pressure = (pressure_p){.time = time_ms, .id = i, .pressure = sensor_val};
                    pkt[sensor_count * 2 + 1] =
                        (struct iovec){.iov_base = &bodies[sensor_count].pressure, .iov_len = sizeof(pressure_p)};
                    sensor_count++;
                    break;
                case TELEM_TEMP:
                    bodies[sensor_count].temp = (temp_p){.time = time_ms, .id = i, .temperature = sensor_val};
                    pkt[sensor_count * 2 + 1] =
                        (struct iovec){.iov_base = &bodies[sensor_count].temp, .iov_len = sizeof(temp_p)};
                    sensor_count++;
                    break;
                case TELEM_THRUST:
                    bodies[sensor_count].thrust = (thrust_p){.time = time_ms, .id = i, .thrust = sensor_val};
                    pkt[sensor_count * 2 + 1] =
                        (struct iovec){.iov_base = &bodies[sensor_count].thrust, .iov_len = sizeof(thrust_p)};
                    sensor_count++;
                    break;
                case TELEM_CONT:
                    bodies[sensor_count].continuity = (continuity_state_p){.time = time_ms, .state = sensor_val};
                    pkt[sensor_count * 2 + 1] = (struct iovec){.iov_base = &bodies[sensor_count].continuity,
                                                               .iov_len = sizeof(continuity_state_p)};
                    sensor_count++;
                    break;
                default:
                    herr("Invalid telemetry data type: %u\n", channel.type);
                    break;
                }
            }
        }
#endif

#if defined(CONFIG_SENSORS_NAU7802)
        if (sensor_mass.available) {
            err = sensor_mass_fetch(&sensor_mass);
            if (err < 0) {
                herr("Error fetching mass data: %d\n", err);
            } else {
                headers[sensor_count] = (header_p){.type = TYPE_TELEM, .subtype = TELEM_MASS};
                bodies[sensor_count].mass = (mass_p){.time = time_ms, .id = 0, .mass = sensor_mass.data.force};

                pkt[sensor_count * 2] =
                    (struct iovec){.iov_base = &headers[sensor_count], .iov_len = sizeof(headers[sensor_count])};
                pkt[sensor_count * 2 + 1] =
                    (struct iovec){.iov_base = &bodies[sensor_count].mass, .iov_len = sizeof(mass_p)};

                sensor_count++;
            }
        }
#endif

        if (sensor_count > 0) {
            struct msghdr msg = {
                .msg_iov = pkt,
                .msg_iovlen = sensor_count * 2,
            };
            telemetry_publish(telem, &msg);
        } else {
            herr("No sensor data to send\n");
            usleep(1000000);
        }
    }
}
#endif

/*
 * Run the thread responsible for transmitting telemetry data.
 * @param arg The arguments to the telemetry thread, of type `telemetry_args_t`.
 */
void *telemetry_run(void *arg) {
    telemetry_args_t *args = (telemetry_args_t *)(arg);
    int err;

    assert(arg != NULL);

    /* Start telemetry socket */

    telemetry_sock_t telem;
    err = telemetry_init(&telem, args->port, args->addr);
    if (err) {
        herr("Could not start telemetry socket: %s\n", strerror(err));
        thread_return(err);
    }
    pthread_cleanup_push(telemetry_cleanup, &telem);

    /* Start thread to periodically update telemetry stream with the pad state */

    pthread_t telemetry_padstate_thread;
    telemetry_padstate_args_t telemetry_padstate_args = {.sock = &telem, .state = args->state};
    err = pthread_create(&telemetry_padstate_thread, NULL, telemetry_update_padstate, &telemetry_padstate_args);
    if (err) {
        herr("Could not start telemetry padstate sending thread: %s\n", strerror(err));
        thread_return(err);
    }
    pthread_cleanup_push(telemetry_cancel_padstate_thread, &telemetry_padstate_thread);

#ifndef DESKTOP_BUILD
    /* Give the telemetry pad-state thread a higher priority TODO: make this constant between CONTROL and TELEM
     * priorities */

    err = pthread_setschedprio(telemetry_padstate_thread, 200);
    if (err) {
        herr("Could not set controller thread priority: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }
#endif

#if defined(DESKTOP_BUILD) || defined(CONFIG_HYSIM_PAD_SERVER_MOCK_DATA)
    /* Start mock telemetry if on desktop build or if we want mock data during */

    hinfo("Starting mock telemetry\n");
    mock_telemetry(args, &telem);
#elif !defined(CONFIG_HYSIM_PAD_SERVER_MOCK_DATA) && !defined(DESKTOP_BUILD)

    /* Start real telemetry if on NuttX and not mocking. */

    hinfo("Starting real telemetry\n");
    sensor_telemetry(args, &telem);
#endif

    nxfail("Telemetry thread exited");
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
    assert(state != NULL);
    assert(sock != NULL);

    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    uint32_t time_ms = time.tv_sec * 1000 + time.tv_nsec / 1000000;

    struct iovec pkt[(1 + NUM_ACTUATORS) * 2];

    struct msghdr msg = {
        .msg_iov = pkt,
        .msg_iovlen = (sizeof(pkt) / sizeof(struct iovec)),
    };

    /* Send arming update */

    arm_lvl_e arm_lvl = padstate_get_level(state);
    arm_state_p arm_body = {.time = time_ms, .state = arm_lvl};
    header_p arm_hdr = (header_p){.type = TYPE_TELEM, .subtype = TELEM_ARM};

    pkt[0] = (struct iovec){.iov_base = &arm_hdr, .iov_len = sizeof(arm_hdr)};
    pkt[1] = (struct iovec){.iov_base = &arm_body, .iov_len = sizeof(arm_body)};

    /* Send actuator updates */

    header_p headers[NUM_ACTUATORS];
    act_state_p bodies[NUM_ACTUATORS];

    for (int i = 0; i < NUM_ACTUATORS; i++) {
        // Store the data in the arrays
        headers[i] = (header_p){.type = TYPE_TELEM, .subtype = TELEM_ACT};
        bool act_state;
        padstate_get_actstate(state, i, &act_state);
        bodies[i] = (act_state_p){.time = time_ms, .id = i, .state = act_state};

        // Point to the stored data
        pkt[(1 + i) * 2] = (struct iovec){.iov_base = &headers[i], .iov_len = sizeof(header_p)};
        pkt[(1 + i) * 2 + 1] = (struct iovec){.iov_base = &bodies[i], .iov_len = sizeof(act_state_p)};
    }

    telemetry_publish(sock, &msg);
}

/*
 * Thread which periodically sends information about the pad's state.
 * @param arg Arguments of type `telemetry_padstate_args_t`
 * @return 0 on success, error code on failure (thread dies)
 */
void *telemetry_update_padstate(void *arg) {
    telemetry_padstate_args_t *args = (telemetry_padstate_args_t *)arg;
    assert(arg != NULL);
    assert(args->state != NULL);
    padstate_t *state = args->state;
    int err = -1;

    for (;;) {
        struct timespec cond_timeout;
        clock_gettime(CLOCK_REALTIME, &cond_timeout);
        cond_timeout.tv_sec += PADSTATE_UPDATE_TIMEOUT_SEC;

        err = pthread_mutex_lock(&state->update_mut);
        assert(err == 0);

        // waiting until either the cond times out or an update is received
        // and we confirmed it was not a spurious wakeup
        while (err != ETIMEDOUT && !state->update_recorded) {
            err = pthread_cond_timedwait(&state->update_cond, &state->update_mut, &cond_timeout);
        }

        if (state->update_recorded) {
            hinfo("Sent updated padstate.\n");
        } else {
            hinfo("Sent padstate as heartbeat.\n");
        }

        telemetry_send_padstate(state, args->sock);
        state->update_recorded = false;
        err = pthread_mutex_unlock(&state->update_mut);
        assert(err == 0);
    }

    nxfail("telemetry_update_padstate exited");
    thread_return(0);
}
