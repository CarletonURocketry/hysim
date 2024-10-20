#ifndef _PAD_H_
#define _PAD_H_

#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>

/* Represents the pad control system server */
typedef struct {
    int sock;                /* Connection to server */
    struct sockaddr_in addr; /* Address of server */
} pad_t;

int pad_init(pad_t *pad, const char *ip, uint16_t port);
int pad_connect(pad_t *pad);
int pad_connect_forever(pad_t *pad);
int pad_disconnect(pad_t *pad);
ssize_t pad_send(pad_t *pad, struct iovec *iov, ssize_t iovlen);
ssize_t pad_recv(pad_t *pad, void *buf, ssize_t n);

#endif // _PAD_H_
