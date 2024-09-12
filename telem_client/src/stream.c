#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

#include "stream.h"

/*
 * Initialize a stream in preparation for connection.
 * TODO: docs
 */
int stream_init(stream_t *stream, const char *ip, uint16_t port) {

    /* Create TCP socket */
    stream->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (stream->sock < 0) {
        return errno;
    }

    /* Create address for socket */
    stream->addr.sin_family = AF_INET;
    stream->addr.sin_port = htons(port);

    /* Convert IPv4 address to integer */
    int ret = inet_pton(AF_INET, ip, &stream->addr.sin_addr);
    if (ret == 0) {
        return EINVAL;
    } else if (ret < 0) {
        return errno;
    }

    return 0;
}

/*
 * Connect the stream to the its telemetry server.
 * TODO: docs
 */
int stream_connect(stream_t *stream) {
    if (connect(stream->sock, (struct sockaddr *)&stream->addr, sizeof(stream->addr)) < 0) {
        return errno;
    }
    return 0;
}

/*
 * Disconnect from the upstream telemetry server.
 * TODO: docs
 */
int stream_disconnect(stream_t *stream) {
    if (close(stream->sock) < 0) {
        return errno;
    }
    return 0;
}

/*
 * Receive bytes from the telemetry upstream.
 * TODO: docs
 */
ssize_t stream_recv(stream_t *stream, void *buf, size_t n) { return recv(stream->sock, buf, n, 0); }
