#include "controller.h"
#include "../../packets/packet.h"
#include <errno.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * Initializes the controller to be ready to create a TCP connection.
 * @param controller The controller to initialize.
 * @param port The port to use for the connection.
 * @return 0 for success, or the error that occurred.
 */
int controller_init(controller_t *controller, uint16_t port) {

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
int controller_accept(controller_t *controller) {

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
int controller_disconnect(controller_t *controller) {

    if (close(controller->client) < 0) {
        return errno;
    }

    if (close(controller->sock) < 0) {
        return errno;
    }

    return 0;
}

/*
 * Receive bytes from the controller client.
 * @param controller The controller to receive from.
 * @param buf The buffer to receive into. Must be at least `n` bytes long.
 * @param n The number of bytes to receive. Must be less than or equal to the length of `buf`.
 * @return The number of bytes read. 0 indicates no more bytes, -1 indicates an error and `errno` will be set.
 */
ssize_t controller_recv(controller_t *controller, void *buf, size_t n) { return recv(controller->client, buf, n, 0); }

/* Run the controller logic
 * TODO: docs
 */
void *controller_run(void *arg) {

    padstate_t *state = ((controller_args_t *)(arg))->state;
    uint16_t port = ((controller_args_t *)(arg))->port;
    sem_t die = ((controller_args_t *)(arg))->die;

    controller_t controller;

    /* Connect to the controller */
    int err;

    err = controller_init(&controller, port);
    if (err) {
        fprintf(stderr, "Could not initialize controller with error: %s\n", strerror(err));
        pthread_exit((void *)(long int)err);
    }

    err = controller_accept(&controller);
    if (err) {
        fprintf(stderr, "Could not connect to controller with error: %s\n", strerror(err));
        pthread_exit((void *)(long int)err);
    }
    printf("Controller connected!\n");

    /* Receive messages */

    for (;;) {

        /* Get the message header to determine what to handle */
        header_p hdr;
        ssize_t bread;
        bread = controller_recv(&controller, &hdr, sizeof(hdr));

        if (bread == -1) {
            fprintf(stderr, "Error reading message header: %s\n", strerror(errno));
            controller_disconnect(&controller);
            pthread_exit((void *)(long int)errno);
        } else if (bread == 0) {
            fprintf(stderr, "Control box disconnected.\n");
            controller_disconnect(&controller);
            pthread_exit((void *)EXIT_FAILURE);
        }

        // TODO: handle recv errors

        switch ((packet_type_e)hdr.type) {
        case TYPE_CNTRL:

            switch ((cntrl_subtype_e)hdr.subtype) {
            case CNTRL_ACT_ACK:
            case CNTRL_ARM_ACK:
                fprintf(stderr, "Unexpectedly received acknowledgement from sender.\n");
                controller_disconnect(&controller);
                pthread_exit((void *)EXIT_FAILURE);
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
            controller_disconnect(&controller);
            pthread_exit((void *)EXIT_FAILURE);
            break;
        }

        /* Check for interrupt */
        int num_threads;
        int value = sem_getvalue(&die, &num_threads);
        if (value == -1) pthread_exit((void *)(long int)errno);
        if (value > 0) {
            printf("Controller terminating...\n");
            controller_disconnect(&controller);
            pthread_exit(0);
        }
    }

    controller_disconnect(&controller);
    return 0;
}
