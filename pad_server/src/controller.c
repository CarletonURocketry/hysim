#include <errno.h>
#include <netinet/tcp.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "../../debugging/logging.h"
#include "../../debugging/nxassert.h"
#include "../../packets/packet.h"
#include "controller.h"
#include "state.h"

/* Helper function for returning an error code from a thread */
#define thread_return(e) pthread_exit((void *)(unsigned long)((e)))

#ifndef DESKTOP_BUILD /* NuttX build */

#ifdef CONFIG_HYSIM_PAD_SERVER_INTERVAL
#define KEEPALIVE_INTERVAL_SECS CONFIG_HYSIM_PAD_SERVER_INTERVAL
#endif

#ifdef CONFIG_HYSIM_PAD_SERVER_NPROBES
#define KEEPALIVE_N_PROBES CONFIG_HYSIM_PAD_SERVER_NPROBES
#endif

#ifdef CONFIG_HYSIM_PAD_SERVER_ABORT_TIME
#define ABORT_TIMEOUT CONFIG_HYSIM_PAD_SERVER_ABORT_TIME
#endif

#else /* is a DESKTOP_BUILD */

#ifndef KEEPALIVE_N_PROBES
#define KEEPALIVE_N_PROBES 2
#endif

#ifndef KEEPALIVE_INTERVAL_SECS
#define KEEPALIVE_INTERVAL_SECS 10
#endif

#ifndef ABORT_TIMEOUT
#define ABORT_TIMEOUT 20
#endif

#endif /* DESKTOP_BUILD */

/*
 * Enables TCP keep-alive on the socket. Based on examples/netloop implementation.
 * @param sockfd The socket to enable TCP keep-alive for
 * @return 0 on success, error code on failure.
 */
static int setsock_keepalive(int sockfd) {
    int err;
    int value;
    struct timeval tv;

    value = 1;
    err = setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(int));
    if (err < 0) {
        err = errno;
        herr("setsockopt(SO_KEEPALIVE) failed: %d\n", err);
        return err;
    }

#ifndef __APPLE__
    tv.tv_sec = KEEPALIVE_INTERVAL_SECS;
    tv.tv_usec = 0;

    err = setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &tv, sizeof(struct timeval));
    if (err < 0) {
        err = errno;
        herr("setsockopt(TCP_KEEPIDLE) failed: %d\n", err);
        return err;
    }
#endif

    tv.tv_sec = KEEPALIVE_INTERVAL_SECS;
    tv.tv_usec = 0;

    err = setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &tv, sizeof(struct timeval));
    if (err < 0) {
        err = errno;
        herr("setsockopt(TCP_KEEPIDLE) failed: %d\n", err);
        return err;
    }

    value = KEEPALIVE_N_PROBES;
    err = setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &value, sizeof(int));
    if (err < 0) {
        err = errno;
        herr("setsockopt(SO_KEEPALIVE) failed: %d\n", err);
        return err;
    }
    return err;
}

/*
 * Initializes the controller to be ready to create a TCP connection.
 * @param controller The controller to initialize.
 * @param port The port to use for the connection.
 * @return 0 for success, or the error that occurred.
 */
static int controller_init(controller_t *controller, uint16_t port) {
    int err;

    /* Initialize the socket connection. */
    controller->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (controller->sock < 0) return errno;

    int opt = 1;
    err = setsockopt(controller->sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (err < 0) {
        herr("Failed to set option SO_REUSEADDR: %d\n", errno);
        return errno;
    }

    /* Create address */
    controller->addr.sin_family = AF_INET;
    controller->addr.sin_addr.s_addr = INADDR_ANY;
    controller->addr.sin_port = htons(port);

    /* Make sure client socket fd is marked as invalid until it gets a connection */
    controller->client = -1;

    if (bind(controller->sock, (struct sockaddr *)&controller->addr, sizeof(controller->addr)) < 0) {
        herr("Failed to bind: %d\n", errno);
        return errno;
    }

    return 0;
}

/*
 * Accept a new connection on the controller client.
 * @param controller The controller to use for the connection.
 * @return 0 for success, or the error that occurred.
 */
static int controller_accept(controller_t *controller) {
    bool was_connected = false; /* Already had a connection before */
    struct timeval timeout = {.tv_sec = ABORT_TIMEOUT, .tv_usec = 0};
    fd_set rfds;
    int err;

    /* Listen for a controller client connection */

    if (listen(controller->sock, MAX_CONTROLLERS) < 0) {
        if (errno == EADDRINUSE) {
            was_connected = true;
            hwarn("Listening more than once...\n");
        } else {
            herr("listen failed: %d\n", errno);
            return errno;
        }
    }

    /* If this is our second or more connection, we want to set a time-out on the accept call so that we can abort if no
     * connection is re-established */

    if (was_connected) {
        hwarn("Setting timeout of %lu seconds for re-connect.\n", timeout.tv_sec);

        FD_ZERO(&rfds);
        FD_SET(controller->sock, &rfds);

        err = select(controller->sock + 1, &rfds, NULL, NULL, &timeout);
        if (err < 0) {
            herr("select failed: %d\n", errno);
            return errno;
        } else if (err == 0) {
            /* This indicates a time-out, since `select()` will return the number of bits set in the bit-map on success.
             */
            herr("Timed out waiting for new connection, ABORT!\n");
            nxfail("Timed out waiting for new connection, ABORT!\n");
            return ETIMEDOUT; /* Just return the error on desktop build */
        }

        /* If we are here, then the `select()` call returned a 1 and it means we can accept a new connection */
    }

    /* Accept the first incoming connection. */

    socklen_t addrlen = sizeof(controller->addr);
    controller->client = accept(controller->sock, (struct sockaddr *)&controller->addr, &addrlen);
    if (controller->client < 0) {
        herr("accept failed: %d\n", errno);
        return errno;
    }

    return setsock_keepalive(controller->client);
}

/*
 * Close the connection to the controller listening socket
 * @param The controller to close the connection to.
 * @return 0 for success, the error that occurred otherwise.
 */
static int controller_sock_disconnect(controller_t *controller) {

    /* Only close the connection if it is valid */

    if (controller->sock >= 0) {
        if (close(controller->sock) < 0) {
            herr("close failed: %d\n", errno);
            return errno;
        }
        controller->sock = -1;
    }
    return 0;
}

/*
 * Close the connection to the controller client
 * @param The controller to close the connection to.
 * @return 0 for success, the error that occurred otherwise.
 */
static int controller_client_disconnect(controller_t *controller) {
    /* Only close the connection if it is valid */

    if (controller->client >= 0) {
        if (close(controller->client) < 0) {
            herr("close failed: %d\n", errno);
            return errno;
        }
        controller->client = -1;
    }

    return 0;
}

/*
 * pthread cleanup handler for the controller.
 * @param arg A controller to disconnect and clean up.
 */
static void controller_cleanup(void *arg) {
    controller_client_disconnect((controller_t *)(arg));
    controller_sock_disconnect((controller_t *)(arg));
}

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

/*
 * Send buf to controller client
 * @param controller The controller to send to.
 * @param buf The buffer with the payload.
 * @param n The size of buf
 * @return The number of bytes written. -1 indicates an error.
 */
static ssize_t controller_send(controller_t *controller, void *buf, size_t n) {
    return send(controller->client, buf, n, 0);
}

/* The controller logic thread
 * @param arg Argument containing `controller_args_t`
 */
void *controller_run(void *arg) {
    controller_args_t *args = (controller_args_t *)(arg);
    controller_t controller;
    int err;
    controller.sock = -1;
    controller.client = -1;

    assert(arg != NULL);

    pthread_cleanup_push(controller_cleanup, &controller);

    /* Initialize the controller (creates a new socket) */

    err = controller_init(&controller, args->port);
    if (err) {
        herr("Could not initialize controller with error: %s\n", strerror(err));
        nxfail("Could not initialize controller");
        exit(EXIT_FAILURE);
    }

    for (;;) {

        /* Initialize the controller (creates a new socket) */

        printf("Waiting for controller...\n");

        err = controller_accept(&controller);
        if (err) {
            herr("Could not accept controller connection with error: %s\n", strerror(err));
            controller_client_disconnect(&controller);
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
                    hinfo("Error reading message header: %s\n", strerror(errno));
                    fprintf(stderr, "Error code: %s\n", strerror(errno));

                    if (errno == ECONNRESET || errno == ENOTCONN || errno == ECONNABORTED) {
                        // TODO: this should trigger an abort because it happens when TCP keep-alive is done
                        herr("Lost connection with controller!\n");
                    }

                    break;
                } else if (bread == 0) {
                    herr("Control box disconnected.\n");
                    break;
                }
                total_read += bread;
            }

            /* Error happened, do a cleanup and re-initialize connection */

            if (bread <= 0) {
                hinfo("Re-initializing connection.\n");
                controller_client_disconnect(&controller);
                break;
            }

            switch ((packet_type_e)hdr.type) {

            case TYPE_CNTRL:

                switch ((cntrl_subtype_e)hdr.subtype) {

                case CNTRL_ACT_ACK:
                    /* Deliberate fall-through */
                case CNTRL_ARM_ACK:
                    herr("Unexpectedly received acknowledgement from sender.\n");
                    break;

                case CNTRL_ACT_REQ: {
                    act_req_p req;

                    controller_recv(&controller, &req, sizeof(req)); // TODO: handle recv errors

                    hinfo("Received actuator request for ID #%u and state %s.\n", req.id, req.state ? "on" : "off");

                    err = pad_actuate(args->state, req.id, req.state);
                    if (err == -1) {
                        herr("Could not modify the actuator with error: %s\n", strerror(errno));
                        break;
                    } else {
                        switch (err) {
                        case ACT_OK:
                            hinfo("Actuator with id %d was put in state %d\n", req.id, req.state);
                            break;

                        case ACT_DNE:
                            hwarn("%d is not a valid actuator id\n", req.id);
                            break;

                        case ACT_INV:
                            hwarn("%d is not a valid state for actuator with id %d\n", req.state, req.id);
                            break;

                        case ACT_DENIED:
                            hwarn("The current arming level is too low to operate actuator with id %d\n", req.id);
                            break;
                        }

                        act_ack_p ack = {.id = req.id, .status = err};
                        controller_send(&controller, &ack, sizeof(ack));
                    }

                } break;

                case CNTRL_ARM_REQ: {
                    arm_req_p req;
                    controller_recv(&controller, &req, sizeof(req)); // TODO: handle recv errors

                    hinfo("Received arming state %u.\n", req.level);

                    err = padstate_change_level(args->state, req.level);
                    arm_ack_p ack;

                    switch (err) {
                    case ARM_OK:
                        hinfo("Arming level changed succesfully to %d\n", req.level);
                        ack.status = ARM_OK;
                        controller_send(&controller, &ack, sizeof(ack));
                        break;
                    case ARM_DENIED:
                        hwarn("Could not change arming level with error: %d, arming denied\n", err);
                        ack.status = ARM_DENIED;
                        controller_send(&controller, &ack, sizeof(ack));
                        break;
                    case ARM_INV:
                        hwarn("Could not change arming level with error: %d, arming invalid\n", err);
                        ack.status = ARM_INV;
                        controller_send(&controller, &ack, sizeof(ack));
                        break;
                    }

                } break;

                default:
                    herr("Invalid control message type: %u\n", hdr.subtype);
                    break;
                }
                break;

            case TYPE_TELEM:
                herr("Unexpectedly received telemetry packet.\n");
                break;

            default:
                herr("Invalid message type: %u\n", hdr.type);
                break;
            }
        }
    }

    nxfail("Reached the end of control thread");
    thread_return(0); // Normal return
    pthread_cleanup_pop(1);
}
