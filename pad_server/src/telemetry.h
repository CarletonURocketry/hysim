#ifndef _TELEMETRY_H_
#define _TELEMETRY_H_

#include "state.h"
#include <netinet/in.h>
#include <semaphore.h>
#include <sys/socket.h>

#define MAX_TELEMETRY 5
#define PADSTATE_UPDATE_TIMEOUT_SEC 5

/* The main telemetry socket */
typedef struct {
    int sock;
    struct sockaddr_in addr;
} telemetry_sock_t;

typedef struct {
    telemetry_sock_t *sock;
    padstate_t *state;
} telemetry_padstate_args_t;

typedef struct {
    padstate_t *state;
    uint16_t port;
    char *addr;
    char *data_file;
} telemetry_args_t;

typedef struct {
    const struct orb_metadata *imu_meta;
    int imu;
    long zero_point;       /* Zero point of the sensor */
    long known_mass_grams; /* Calibration weight in grams */
    long known_mass_point; /* Calibration weight value */
} sensor_mass_t;

void *telemetry_run(void *arg);
void *telemetry_update_padstate(void *arg);
void telemetry_send_padstate(padstate_t *state, telemetry_sock_t *sock);

#endif // _TELEMETRY_H_
