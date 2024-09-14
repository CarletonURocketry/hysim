#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "telemetry.h"

static char buffer[BUFSIZ];

/*
 * Initialize the telemetry structure for accepting connections.
 */
int telemetry_init(telemetry_t *telem, uint16_t port) {

    /* Initialize the socket connection. */
    telem->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (telem->sock < 0) return errno;

    /* Create address */

    telem->addr.sin_family = AF_INET;
    telem->addr.sin_addr.s_addr = INADDR_ANY;
    telem->addr.sin_port = htons(port);

    /* Make sure client socket fd is marked as invalid until it gets a connection */
    for (unsigned int i = 0; i < MAX_TELEMETRY; i++) {
        telem->clients[i] = -1;
    }

    return 0;
}

/* TODO: docs */
int telemetry_accept(telemetry_t *telem) {

    /* Bind the controller socket */
    if (bind(telem->sock, (struct sockaddr *)&telem->addr, sizeof(telem->addr)) < 0) {
        return errno;
    }

    /* Listen for a controller client connection */
    if (listen(telem->sock, MAX_TELEMETRY) < 0) {
        return errno;
    }

    /* Accept the first incoming connection. */
    socklen_t addrlen = sizeof(telem->addr);
    for (unsigned int i = 0; i < MAX_TELEMETRY; i++) {

        /* Found a free spot for a client */
        if (telem->clients[i] == -1) {
            /* Try to connect the client */
            telem->clients[i] = accept(telem->sock, (struct sockaddr *)&telem->addr, &addrlen);
            if (telem->clients[i] < 0) {
                return errno;
            }
            break; /* Connection successful, exit loop */
        }
    }

    return 0;
}

/* TODO: docs */
int telemetry_disconnect_all(telemetry_t *telem) {
    int res;
    int err;
    for (unsigned int i = 0; i < MAX_TELEMETRY; i++) {
        if (telem->clients[i] != -1) {
            res = close(telem->clients[i]);
            if (res == -1) err = errno;
            telem->clients[i] = -1; // Mark as invalid
        }
    }

    /* Close primary socket */
    res = close(telem->sock);
    if (res == -1) return errno;
    return err;
}

/* TODO: docs */
ssize_t telemetry_send(telemetry_t *telem, unsigned int id, void *buf, size_t n) {
    if (telem->clients[id] == -1) {
        return n;
    }
    return send(telem->clients[id], buf, n, 0);
}

/* TODO: docs */
void *telemetry_run(void *arg) {

    int err;
    telemetry_t telem;
    padstate_t *state = ((telemetry_args_t *)(arg))->state;
    uint16_t port = ((telemetry_args_t *)(arg))->port;
    sem_t die = ((telemetry_args_t *)(arg))->die;
    const char *data_file = ((telemetry_args_t *)(arg))->data_file;

    err = telemetry_init(&telem, port);
    if (err) {
        fprintf(stderr, "Could not initialize telemetry stream with error: %s\n", strerror(err));
        pthread_exit((void *)(long int)err);
    }

    err = telemetry_accept(&telem);
    if (err) {
        fprintf(stderr, "Error connecting telemetry client: %s\n", strerror(err));
        pthread_exit((void *)(long int)(err));
    }
    printf("Telemetry client connected!\n");

    /* Open dummy data file */

    // TODO: remove abspath
    FILE *data = fopen(data_file, "r");
    if (data == NULL) {
        fprintf(stderr, "Could not open dummy data: %s\n", strerror(errno));
        telemetry_disconnect_all(&telem);
        pthread_exit((void *)(long int)errno);
    }
    // Discard labels
    fgets(buffer, BUFSIZ, data);

    /* Send telemetry */

    header_p hdr = {.type = TYPE_TELEM};
    ssize_t sent;

    for (;;) {

        /* Continually loop over file */
        if (feof(data)) {
            if (fseek(data, 0, SEEK_SET) == -1) {
                fprintf(stderr, "Couldn't rewind data with error: %s\n", strerror(errno));
                telemetry_disconnect_all(&telem);
                pthread_exit((void *)(long int)errno);
            }
            fgets(buffer, BUFSIZ, data); // Discard labels
        }

        // Get next line
        if (fgets(buffer, BUFSIZ, data) == NULL) {
            continue; // Handle EOF
        }

        /* Time */
        char *timestr = strtok(buffer, ",");
        uint32_t time = strtoul(timestr, NULL, 10);

        /* Mass */
        char *massstr = strtok(NULL, ",");
        uint32_t mass = strtoul(massstr, NULL, 10) * 1000;

        /* Pressure 1 */
        char *p1str = strtok(NULL, ",");
        uint32_t p1 = strtoul(p1str, NULL, 10) * 1000;

        /* Pressure 2 */
        char *p2str = strtok(NULL, ",");
        uint32_t p2 = strtoul(p2str, NULL, 10) * 1000;

        for (unsigned int i = 0; i < MAX_TELEMETRY; i++) {

            hdr.subtype = TELEM_PRESSURE;
            pressure_p pkt = {.time = time, .pressure = p1, .id = 1};

            sent = telemetry_send(&telem, i, &hdr, sizeof(hdr));
            if (sent == -1) {
                fprintf(stderr, "Could not send packet header to client %u: %s\n", i, strerror(errno));
                telem.clients[i] = -1;
            } else if (sent == 0) {
                fprintf(stderr, "Client %u disconnected.\n", i);
                telem.clients[i] = -1;
            }

            sent = telemetry_send(&telem, i, &pkt, sizeof(pkt));
            if (sent == -1) {
                fprintf(stderr, "Could not send packet to client %u: %s\n", i, strerror(errno));
                telem.clients[i] = -1;
            } else if (sent == 0) {
                fprintf(stderr, "Client %u disconnected.\n", i);
                telem.clients[i] = -1;
            }
        }

        /* Check for interrupt */
        int num_threads;
        int value = sem_getvalue(&die, &num_threads);
        if (value == -1) pthread_exit((void *)(long int)errno);
        if (value > 0) {
            printf("Telemetry terminating...\n");
            telemetry_disconnect_all(&telem);
            pthread_exit(0);
        }
        usleep(10);
    }

    telemetry_disconnect_all(&telem);
    return 0;
}
