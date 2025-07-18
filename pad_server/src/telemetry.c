#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#if !defined(DESKTOP_BUILD)
#include <nuttx/analog/ads1115.h>
#endif

#if defined(DESKTOP_BUILD) || defined(CONFIG_HYSIM_PAD_SERVER_MOCK_DATA)
#include <stdlib.h>
#endif

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
    if (sendmsg(sock->sock, msg, MSG_NOSIGNAL) < 0) {
        return errno;
    }
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
    struct iovec pkt[2];
    struct msghdr msg = {
        .msg_iov = pkt,
        .msg_iovlen = sizeof(pkt) / sizeof(pkt[0]),
    };
    header_p hdr;
    struct timespec time;
    uint32_t time_ms;

    /* Set up the header info for every message */

    pkt[0].iov_base = &hdr;
    pkt[0].iov_len = sizeof(hdr);

    for (;;) {

        /* Get the current time */

        clock_gettime(CLOCK_MONOTONIC, &time);
        time_ms = time.tv_sec * 1000 + time.tv_nsec / 1000000;

        /* Send pressure transducer data for transducers 0 through 5 */

        hdr.type = TYPE_TELEM;
        hdr.subtype = TELEM_PRESSURE;

        pressure_p pressure;
        for (int i = 0; i < 6; i++) {
            packet_pressure_init(&pressure, i, time_ms, (uint32_t)(1000 * (rand() / RAND_MAX)));
            pkt[1].iov_base = &pressure;
            pkt[1].iov_len = sizeof(pressure);
            telemetry_publish(telem, &msg);
        }

        /* Send single continuity measurement */

        hdr.subtype = TELEM_CONT;
        continuity_state_p cont;
        packet_continuity_state_init(&cont, time_ms, rand() % 2);

        pkt[1].iov_base = &cont;
        pkt[1].iov_len = sizeof(cont);
        telemetry_publish(telem, &msg);

        usleep(100000); /* 100ms sleep */
    }
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
        } else {
            hinfo("NAU7802 mass sensor inititialized.\n");
        }
    }

#endif

#if defined(CONFIG_SENSORS_MCP9600)

    sensor_temp_t sensor_temp[2] = {
        {.available = true, .topic = 2, .sensor_id = 2},
        {.available = true, .topic = 5, .sensor_id = 3},
    };

    for (int i = 0; i < arr_len(sensor_temp); i++) {
        err = sensor_temp_init(&sensor_temp[i]);
        if (err < 0) {
            herr("Coult not initialize temperature topic %d: %d\n", sensor_temp[i].topic, err);
            sensor_temp[i].available = false;
        } else {
            hinfo("Temperature topic %d initialized.\n", sensor_temp[i].topic);
        }
    }
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
                    {.channel_num = 7, .sensor_id = 2, .type = TELEM_PRESSURE},
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
                    {.channel_num = 6, .sensor_id = 5, .type = TELEM_PRESSURE},
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
        } else {
            hinfo("Initialized ADC device %s\n", adc_devices[i].devpath);
        }
    }
#endif

    for (;;) {
        struct iovec pkt[(32) * 2]; /* 32 possible measurements, headers and bodies */
        int sensor_count = 0;
        struct timespec time_t;
        uint32_t time_ms;
        header_p headers[32];

        /* All possible sensor bodies */

        union {
            pressure_p pressure;
            mass_p mass;
            temp_p temp;
            thrust_p thrust;
            continuity_state_p continuity;
        } bodies[32];

#if defined(CONFIG_ADC_ADS1115)
        struct adc_msg_s sample;
        adc_channel_t *channel;

        /* For each of the possible 4 channels an ADS1115 ADC can have, we loop */

        for (int j = 0; j < 4; j++) {

            /* For each ADC, we loop and we trigger a conversion on their `j`th channel */

            for (int i = 0; i < arr_len(adc_devices); i++) {

                /* If this ADC has an invalid file descriptor, skip it.
                 * If the ADC does not have a `j`th channel, skip it.
                 */

                if (adc_devices[i].fd < 0 || j >= adc_devices[i].n_channels) {
                    continue;
                }

                /* We have a valid ADC with a channel `j`. Trigger a conversion on this channel for this ADC */

                channel = &adc_devices[i].channels[j];
                sample.am_channel = channel->channel_num;

                err = ioctl(adc_devices[i].fd, ANIOC_ADS1115_TRIGGER_CONVERSION, &sample);
                if (err) {
                    herr("Couldn't trigger ADC channel %d: %d\n", channel->channel_num, errno);
                    continue;
                }
            }

            /* Now, for each ADC again, get the completed conversion result from channel `j` and put it in the packet.
             */

            for (int i = 0; i < arr_len(adc_devices); i++) {

                /* If this ADC has an invalid file descriptor, skip it.
                 * If the ADC does not have a `j`th channel, skip it.
                 */

                if (adc_devices[i].fd < 0 || j >= adc_devices[i].n_channels) {
                    continue;
                }

                channel = &adc_devices[i].channels[j];
                sample.am_channel = channel->channel_num;
                err = ioctl(adc_devices[i].fd, ANIOC_ADS1115_READ_CHANNEL_NO_CONVERSION, &sample);

                if (err) {
                    herr("Couldn't read ADC channel %d: %d\n", channel->channel_num, errno);
                    continue;
                }

                /* Get an exact time stamp for the data body */

                clock_gettime(CLOCK_MONOTONIC, &time_t);
                time_ms = time_t.tv_sec * 1000 + time_t.tv_nsec / 1000000;

                /* Now that we have the voltage from the ADC, convert it to a real measurement. */

                int32_t sensor_val = 0;
                err = adc_sensor_val_conversion(channel, sample.am_data, &sensor_val);
                if (err) {
                    herr("Couldn't convert value from ADC channel %d: %d\n", i, err);
                    continue;
                }

                /* Put the converted value in the packet */

                headers[sensor_count] = (header_p){.type = TYPE_TELEM, .subtype = channel->type};
                pkt[sensor_count * 2] = (struct iovec){
                    .iov_base = &headers[sensor_count],
                    .iov_len = sizeof(headers[sensor_count]),
                };

                switch (channel->type) {
                case TELEM_PRESSURE:
                    bodies[sensor_count].pressure =
                        (pressure_p){.time = time_ms, .id = channel->sensor_id, .pressure = sensor_val};
                    pkt[sensor_count * 2 + 1] =
                        (struct iovec){.iov_base = &bodies[sensor_count].pressure, .iov_len = sizeof(pressure_p)};
                    sensor_count++;
                    break;
                case TELEM_TEMP:
                    bodies[sensor_count].temp =
                        (temp_p){.time = time_ms, .id = channel->sensor_id, .temperature = sensor_val};
                    pkt[sensor_count * 2 + 1] =
                        (struct iovec){.iov_base = &bodies[sensor_count].temp, .iov_len = sizeof(temp_p)};
                    sensor_count++;
                    break;
                case TELEM_THRUST:
                    bodies[sensor_count].thrust =
                        (thrust_p){.time = time_ms, .id = channel->sensor_id, .thrust = sensor_val};
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
                    herr("Invalid telemetry data type: %u\n", channel->type);
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

                float slope = (float)sensor_mass.known_mass_grams /
                              (float)(sensor_mass.known_mass_point - sensor_mass.zero_point);
                int32_t output = (int32_t)((sensor_mass.data.force - sensor_mass.zero_point) * slope);
                time_ms = sensor_mass.data.timestamp / 1000;
                bodies[sensor_count].mass = (mass_p){.time = time_ms, .id = 0, .mass = output};
                pkt[sensor_count * 2] =
                    (struct iovec){.iov_base = &headers[sensor_count], .iov_len = sizeof(headers[sensor_count])};
                pkt[sensor_count * 2 + 1] =
                    (struct iovec){.iov_base = &bodies[sensor_count].mass, .iov_len = sizeof(mass_p)};

                sensor_count++;
            }
        }
#endif

#if defined(CONFIG_SENSORS_MCP9600)

        for (int i = 0; i < arr_len(sensor_temp); i++) {
            if (sensor_temp[i].available) {

                err = sensor_temp_fetch(&sensor_temp[i]);
                if (err < 0) {
                    herr("Error fetching temperature topic %d: %d\n", sensor_temp[i].topic, err);
                    continue;
                }

                time_ms = sensor_temp[i].data.timestamp / 1000;
                headers[sensor_count] = (header_p){.type = TYPE_TELEM, .subtype = TELEM_TEMP};
                bodies[sensor_count].temp = (temp_p){
                    .time = time_ms,
                    .id = sensor_temp[i].sensor_id,
                    .temperature = sensor_temp[i].data.temperature * 1000,
                };
                pkt[sensor_count * 2] = (struct iovec){
                    .iov_base = &headers[sensor_count],
                    .iov_len = sizeof(headers[sensor_count]),
                };
                pkt[sensor_count * 2 + 1] = (struct iovec){
                    .iov_base = &bodies[sensor_count].temp,
                    .iov_len = sizeof(temp_p),
                };
                sensor_count++;
            }
        }
#endif

        /* Send the telemetry that was collected */

        if (sensor_count > 0) {
            struct msghdr msg = {.msg_iov = pkt, .msg_iovlen = sensor_count * 2};
            telemetry_publish(telem, &msg);
        } else {
            herr("No sensor data to send\n");
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
    uint32_t time_ms;
    header_p arm_hdr;
    header_p conn_hdr;
    arm_state_p arm_body;
    conn_status_p conn_body;
    header_p headers[NUM_ACTUATORS];
    act_state_p bodies[NUM_ACTUATORS];
    bool act_state;

    /* One packet per actuator
     * + one packet for the arming state
     * + one packet for the connection state.
     *
     * All multiplied by 2 because there is a header and a body for each.
     */

    struct iovec pkt[(NUM_ACTUATORS + 2) * 2];
    struct msghdr msg = {
        .msg_iov = pkt,
        .msg_iovlen = (sizeof(pkt) / sizeof(struct iovec)),
    };

    /* Get the current time and convert it to milliseconds */

    clock_gettime(CLOCK_MONOTONIC, &time);
    time_ms = time.tv_sec * 1000 + time.tv_nsec / 1000000;

    /* Construct packets for arming level and for connection status */

    packet_header_init(&arm_hdr, TYPE_TELEM, TELEM_ARM);
    packet_arm_state_init(&arm_body, time_ms, padstate_get_level(state));
    packet_header_init(&conn_hdr, TYPE_TELEM, TELEM_CONN);
    packet_conn_init(&conn_body, time_ms, padstate_get_connstatus(state));

    /* Arming state packet */

    pkt[0] = (struct iovec){.iov_base = &arm_hdr, .iov_len = sizeof(arm_hdr)};
    pkt[1] = (struct iovec){.iov_base = &arm_body, .iov_len = sizeof(arm_body)};

    /* Connection state packet */

    pkt[2] = (struct iovec){.iov_base = &conn_hdr, .iov_len = sizeof(conn_hdr)};
    pkt[3] = (struct iovec){.iov_base = &conn_body, .iov_len = sizeof(conn_body)};

    /* Send actuator updates */

    for (int i = 0; i < NUM_ACTUATORS; i++) {

        /* Store the data in the arrays */

        packet_header_init(&headers[i], TYPE_TELEM, TELEM_ACT);
        padstate_get_actstate(state, i, &act_state);
        packet_act_state_init(&bodies[i], i, time_ms, act_state);

        /* Point to the stored data in the iovecs */

        pkt[4 + (i * 2)] = (struct iovec){.iov_base = &headers[i], .iov_len = sizeof(headers[i])};
        pkt[4 + (i * 2) + 1] = (struct iovec){.iov_base = &bodies[i], .iov_len = sizeof(bodies[i])};
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
        assert(err == 0 && "Failed to lock mutex");

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
