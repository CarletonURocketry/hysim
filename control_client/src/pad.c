#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

#include "pad.h"

/*
 * Initialize a pad structure.
 * @param pad The pad structure to initialize.
 * @param ip The IPv4 address of the pad server.
 * @param port The control port of the pad server.
 * @return 0 for success, the error that occurred otherwise.
 */
int pad_init(pad_t *pad, const char *ip, uint16_t port) {

    /* Create TCP socket */
    pad->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (pad->sock < 0) {
        return errno;
    }

    /* Create address */
    pad->addr.sin_family = AF_INET;
    pad->addr.sin_port = htons(port);

    /* Convert IPv4 addresses from text to binary form */
    int ret = inet_pton(AF_INET, ip, &pad->addr.sin_addr);
    if (ret == 0) {
        return EINVAL;
    } else if (ret < 0) {
        return errno;
    }

    return 0;
}

/*
 * Connect to the control port of the pad server.
 * @param pad The pad server to connect to.
 * @return 0 for success, the error that occurred otherwise.
 */
int pad_connect(pad_t *pad) {
    if (connect(pad->sock, (struct sockaddr *)&pad->addr, sizeof(pad->addr)) < 0) {
        return errno;
    }
    return 0;
}

/*
 * Connect to the control port of the pad server with infinite retries when there is no port open.
 * @param pad The pad server to connect to.
 * @return 0 on success, the error that occurred if a non-timeout related error occurred.
 */
int pad_connect_forever(pad_t *pad) {

    int status = 0;
    do {
        status = pad_connect(pad);
    } while (status == ECONNREFUSED);

    return status;
}

/*
 * Send a message to the control port of the pad server.
 * @param pad The pad server to send a message to.
 * @param buf The buffer containing `n` bytes of message to be sent.
 * @param n The number of bytes in `buf` to be sent.
 * @return The number of bytes that were sent, or -1 on failure (errno indicates the error).
 */
ssize_t pad_send(pad_t *pad, const void *buf, size_t n) { return send(pad->sock, buf, n, 0); }

/*
 * Close the connection to the pad server.
 * @param pad The pad server to close the connection to.
 * @return 0 on success, the error that occurred on failure.
 */
int pad_disconnect(pad_t *pad) {
    if (close(pad->sock) < 0) {
        return errno;
    }
    return 0;
}
