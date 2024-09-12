#ifndef _CONTROL_H_
#define _CONTROL_H_

#include <netinet/in.h>
#include <sys/socket.h>
#include "state.h"
#include <semaphore.h>

/* The maximum number of controllers allowed to connect to the pad control system. */
#define MAX_CONTROLLERS 1

/* Represents the controller client */
typedef struct {
    int sock;                /* The pad socket accepting connections. */
    int client;              /* The control client connection. */
    struct sockaddr_in addr; /* The address of the controller client */
} controller_t;

typedef struct {
    padstate_t *state;
    uint16_t port;
    sem_t die;
} controller_args_t;

/* Represents the arguments passed to the controller. */

int controller_init(controller_t *controller, uint16_t port);
int controller_accept(controller_t *controller);
int controller_disconnect(controller_t *controller);
ssize_t controller_recv(controller_t *controller, void *buf, size_t n);

void *controller_run(void *arg);

#endif // _CONTROL_H_
