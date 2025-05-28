#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "pad.h"

#define RCVTIMEO_SEC 3;

#ifndef DESKTOP_BUILD /* NuttX build */

#ifdef CONFIG_HYSIM_CONTROL_CLIENT_INTERVAL
#define KEEPALIVE_INTERVAL_SECS CONFIG_HYSIM_CONTROL_CLIENT_INTERVAL
#endif

#ifdef CONFIG_HYSIM_CONTROL_CLIENT_NPROBES
#define KEEPALIVE_N_PROBES CONFIG_HYSIM_CONTROL_CLIENT_NPROBES
#endif

#else /* is a DESKTOP_BUILD */

#ifndef KEEPALIVE_N_PROBES
#define KEEPALIVE_N_PROBES 2
#endif

#ifndef KEEPALIVE_INTERVAL_SECS
#define KEEPALIVE_INTERVAL_SECS 10
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
        return err;
    }

#ifndef __APPLE__
    tv.tv_sec = KEEPALIVE_INTERVAL_SECS;
    tv.tv_usec = 0;

    err = setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &tv, sizeof(struct timeval));
    if (err < 0) {
        err = errno;
        return err;
    }
#endif

    tv.tv_sec = KEEPALIVE_INTERVAL_SECS;
    tv.tv_usec = 0;

    err = setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &tv, sizeof(struct timeval));
    if (err < 0) {
        err = errno;
        return err;
    }

    value = KEEPALIVE_N_PROBES;
    err = setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &value, sizeof(int));
    if (err < 0) {
        err = errno;
        return err;
    }
    return err;
}

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

#if defined(CONFIG_CLOCK_TIMEKEEPING)
    struct timeval tv;
    tv.tv_sec = RCVTIMEO_SEC;
    tv.tv_usec = 0;
    if (setsockopt(pad->sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv) < 0) {
        return errno;
    }
#endif /* defined(CONFIG_CLOCK_TIMEKEEPING) */

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

    return setsock_keepalive(pad->sock);
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
        if (status != 0) {
            fprintf(stderr, "Connect failed (error %d), trying again.\n", status);
            sleep(1);
        }
    } while (status == ECONNREFUSED || status == ETIMEDOUT || status == ENOTCONN || status == ENETUNREACH);

    return status;
}

/*
 * Send a message to the control port of the pad server.
 * @param pad The pad server to send a message to.
 * @param buf The buffer containing `n` bytes of message to be sent.
 * @param n The number of bytes in `buf` to be sent.
 * @return The number of bytes that were sent, or -1 on failure (errno indicates the error).
 */
ssize_t pad_send(pad_t *pad, struct iovec *iov, ssize_t iovlen) {
    struct msghdr msg = {0};
    msg.msg_iov = iov;
    msg.msg_iovlen = iovlen;
    return sendmsg(pad->sock, &msg, 0);
}

/*
 * Close the connection to the pad server.
 * @param pad The pad server to close the connection to.
 * @return 0 on success, the error that occurred on failure.
 */
int pad_disconnect(pad_t *pad) {
    if (pad->sock >= 0) {
        if (close(pad->sock) < 0) {
            return errno;
        }
        pad->sock = -1;
    }
    return 0;
}

/*
 * Receive a message from the pad server.
 * @param pad The pad server to send a message to.
 * @param buf The destination buffer.
 * @param n The size of the destination buffer.
 * @return The number of bytes that were received, or -1 on failure (errno indicates the error).
 */
ssize_t pad_recv(pad_t *pad, void *buf, ssize_t n) { return recv(pad->sock, buf, n, 0); }
