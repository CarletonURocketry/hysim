#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
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
    stream->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (stream->sock < 0) {
        return errno;
    }

    /* Create address for socket */
    stream->addr.sin_family = AF_INET;
    stream->addr.sin_addr.s_addr = htonl(INADDR_ANY);
    stream->addr.sin_port = htons(port);

    /* Register for multicast */
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(ip);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    /* Register with multicast group */
    if (setsockopt(stream->sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        return errno;
    }

    /* Re-use port so testing can be done on the same machine. */
    int reuse = 1;
    if (setsockopt(stream->sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) == -1) {
        return errno;
    }

    /* Bind the socket for use. */
    if (bind(stream->sock, (struct sockaddr *)&stream->addr, sizeof(stream->addr)) < 0) {
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
ssize_t stream_recv(stream_t *stream, void *buf, size_t n, int flag) {
    socklen_t size = sizeof(stream->addr);
    return recvfrom(stream->sock, buf, n, flag, (struct sockaddr *)&stream->addr, &size);
}
