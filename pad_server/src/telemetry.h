#ifndef _TELEMETRY_H_
#define _TELEMETRY_H_

#include "state.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <semaphore.h>

#define MAX_TELEMETRY 5

typedef struct {
    int sock;
    int clients[MAX_TELEMETRY]; /* The control client connection. */
    struct sockaddr_in addr;    /* The address of the controller client */
} telemetry_t;

typedef struct {
    padstate_t *state;
    uint16_t port;
    sem_t die;
    char *data_file;
} telemetry_args_t;

int telemetry_init(telemetry_t *telem, uint16_t port);
int telemetry_disconnect_all(telemetry_t *telem);
int telemetry_accept(telemetry_t *telem);
ssize_t telemetry_send(telemetry_t *telem, unsigned int id, void *buf, size_t n);

void *telemetry_run(void *arg);

#endif // _TELEMETRY_H_
