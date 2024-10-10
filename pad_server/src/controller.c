#include <errno.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../../packets/packet.h"
#include "controller.h"

/* Helper function for returning an error code from a thread */
#define thread_return(e) pthread_exit((void *)(unsigned long)((e)))

/*
 * Initializes the controller to be ready to create a TCP connection.
 * @param controller The controller to initialize.
 * @param port The port to use for the connection.
 * @return 0 for success, or the error that occurred.
 */
static int controller_init(controller_t *controller, uint16_t port) {

    /* Initialize the socket connection. */
    controller->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (controller->sock < 0) return errno;

    /* Create address */
    controller->addr.sin_family = AF_INET;
    controller->addr.sin_addr.s_addr = INADDR_ANY;
    controller->addr.sin_port = htons(port);

    /* Make sure client socket fd is marked as invalid until it gets a connection */
    controller->client = -1;

    return 0;
}

/*
 * Connect to the controller client.
 * @param controller The controller to use for the connection.
 * @return 0 for success, or the error that occurred.
 */
static int controller_accept(controller_t *controller) {

    /* Bind the controller socket */
    if (bind(controller->sock, (struct sockaddr *)&controller->addr, sizeof(controller->addr)) < 0) {
        return errno;
    }

    /* Listen for a controller client connection */
    if (listen(controller->sock, MAX_CONTROLLERS) < 0) {
        return errno;
    }

    /* Accept the first incoming connection. */
    socklen_t addrlen = sizeof(controller->addr);
    controller->client = accept(controller->sock, (struct sockaddr *)&controller->addr, &addrlen);
    if (controller->client < 0) {
        return errno;
    }

    return 0;
}

/*
 * Close the connection to the controller.
 * @param The controller to close the connection to.
 * @return 0 for success, the error that occurred otherwise.
 */
static int controller_disconnect(controller_t *controller) {

    if (close(controller->client) < 0) {
        return errno;
    }

    if (close(controller->sock) < 0) {
        return errno;
    }

    return 0;
}

/*
 * pthread cleanup handler for the controller.
 * @param arg A controller to disconnect and clean up.
 */
static void controller_cleanup(void *arg) { controller_disconnect((controller_t *)(arg)); }

/*
 * Receive bytes from the controller client.
 * @param controller The controller to receive from.
 * @param buf The buffer to receive into. Must be at least `n` bytes long.
 * @param n The number of bytes to receive. Must be less than or equal to the length of `buf`.
 * @return The number of bytes read. 0 indicates no more bytes, -1 indicates an error and `errno` will be set.
 */
static ssize_t controller_recv(controller_t *controller, void *buf, size_t n) {
    return recv(controller->client, buf, n, 0);
}

/* Run the controller logic
 * TODO: docs
 */
void *controller_run(void *arg) {
    controller_args_t *args = (controller_args_t *)(arg);
    controller_t controller;
    int err;

    fprintf(stderr, "Waiting for controller...\n");

    pthread_cleanup_push(controller_cleanup, &controller);

    for (;;) {
        /* Initialize the controller (creates a new socket) */
        err = controller_init(&controller, args->port);
        if (err) {
            fprintf(stderr, "Could not initialize controller with error: %s\n", strerror(err));
            sleep(1);
            continue;
        }

        err = controller_accept(&controller);
        if (err) {
            fprintf(stderr, "Could not accept controller connection with error: %s\n", strerror(err));
            controller_disconnect(&controller); // clean up
            sleep(1);
            continue;
        }
        printf("Controller connected!\n");

        /* Receive messages */
        for (;;) {
            /* Get the message header to determine what to handle */
            header_p hdr;
            ssize_t bread = 0;
            size_t total_read = 0;

            while (total_read < sizeof(hdr)) {
                bread = controller_recv(&controller, (char *)&hdr + total_read, sizeof(hdr) - total_read);
                if (bread == -1) {
                    if (errno == EINTR) continue; // Interrupted, try again
                    fprintf(stderr, "Error reading message header: %s\n", strerror(errno));
                    break;
                } else if (bread == 0) {
                    fprintf(stderr, "Control box disconnected.\n");
                    break;
                }
                total_read += bread;
            }

            if (bread <= 0) break; // something that should not have happened happened, try reconnecting just in case

            // TODO: handle recv errors

            switch ((packet_type_e)hdr.type) {
            case TYPE_CNTRL:

                switch ((cntrl_subtype_e)hdr.subtype) {
                case CNTRL_ACT_ACK:
                case CNTRL_ARM_ACK:
                    fprintf(stderr, "Unexpectedly received acknowledgement from sender.\n");
                    thread_return(EXIT_FAILURE);
                    break;
                case CNTRL_ACT_REQ: {
                    act_req_p req;
                    controller_recv(&controller, &req, sizeof(req));
                    printf("Received actuator request for ID #%u and state %s.\n", req.id, req.state ? "on" : "off");
                } break;
                case CNTRL_ARM_REQ: {
                    arm_req_p req;
                    controller_recv(&controller, &req, sizeof(req));
                    printf("Received arming state %u.\n", req.level);
                } break;
                }

                break;
            default:
                fprintf(stderr, "Invalid message type: %u\n", hdr.type);
                thread_return(EXIT_FAILURE);
                break;
            }
        }

        // Disconnect before attempting to reconnect
        controller_disconnect(&controller);
        sleep(1); // Wait before retrying
    }

    thread_return(0); // Normal return
    pthread_cleanup_pop(1);
}
