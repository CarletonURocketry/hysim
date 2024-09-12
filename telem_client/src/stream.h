#ifndef _STREAM_H_
#define _STREAM_H_

#include <sys/socket.h>
#include <netinet/in.h>

typedef struct {
    int sock;
    struct sockaddr_in addr;
} stream_t;

int stream_init(stream_t *stream, const char *ip, uint16_t port);
int stream_connect(stream_t *stream);
int stream_disconnect(stream_t *stream);
ssize_t stream_recv(stream_t *stream, void *buf, size_t n);

#endif // _STREAM_H_
