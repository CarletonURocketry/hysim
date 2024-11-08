#ifndef _TELEMETRY_H_
#define _TELEMETRY_H_

#include "state.h"
#include <netinet/in.h>
#include <semaphore.h>
#include <sys/socket.h>

#define MAX_TELEMETRY 5

typedef struct {
    padstate_t *state;
    uint16_t port;
    char *addr;
    char *data_file;
} telemetry_args_t;

void *telemetry_run(void *arg);

#endif // _TELEMETRY_H_
