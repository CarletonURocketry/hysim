#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "telemetry.h"

#define MULTICAST_ADDR "224.0.0.10"

/* Helper function for returning an error code from a thread */
#define thread_return(e) pthread_exit((void *)(unsigned long)((e)))

/* The main telemetry socket */
typedef struct {
    int sock;
    struct sockaddr_in addr;
} telemetry_sock_t;

/*
 * Set up the telemetry socket for connection.
 * @param sock The telemetry socket to initialize.
 * @param port The port number to use to accept connections.
 * @return 0 for success, error code on failure.
 */
static int telemetry_init(telemetry_sock_t *sock, uint16_t port) {

    /* Initialize the socket connection. */
    sock->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock->sock < 0) return errno;

    /* Create address */
    sock->addr.sin_family = AF_INET;
    sock->addr.sin_addr.s_addr = inet_addr(MULTICAST_ADDR);
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
 * Run the thread responsible for transmitting telemetry data.
 * @param arg The arguments to the telemetry thread, of type `telemetry_args_t`.
 */
void *telemetry_run(void *arg) {

    telemetry_args_t *args = (telemetry_args_t *)(arg);
    char buffer[BUFSIZ];
    int err;

    /* Null telemetry file means nothing to do */
    if (args->data_file == NULL) {
        printf("No telemetry data to send.\n");
        thread_return(0);
    }

    /* Start telemetry socket */
    telemetry_sock_t telem;
    err = telemetry_init(&telem, args->port);
    if (err) {
        fprintf(stderr, "Could not start telemetry socket: %s\n", strerror(err));
        thread_return(err);
    }
    pthread_cleanup_push(telemetry_cleanup, &telem);

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
        char *time_str = strtok_r(buffer, ",", &rest);
        uint32_t time = strtoul(time_str, NULL, 10);
        char *pstr = strtok_r(buffer, ",", &rest);
        uint32_t pressure = strtoul(pstr, NULL, 10);

        header_p hdr = {.type = TYPE_TELEM, .subtype = TELEM_PRESSURE};
        pressure_p body = {.id = 0, .time = time, .pressure = pressure};

        /* Send data to all clients. */
        struct iovec pkt[2] = {
            {.iov_base = &hdr, .iov_len = sizeof(hdr)},
            {.iov_base = &body, .iov_len = sizeof(body)},
        };
        struct msghdr msg = {
            .msg_name = NULL,
            .msg_namelen = 0,
            .msg_iov = pkt,
            .msg_iovlen = (sizeof(pkt) / sizeof(struct iovec)),
            .msg_control = NULL,
            .msg_controllen = 0,
            .msg_flags = 0,
        };
        telemetry_publish(&telem, &msg);

        usleep(1000);
    }

    thread_return(0); // Normal return

    pthread_cleanup_pop(1);
}
