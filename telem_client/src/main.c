#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../../packets/packet.h"
#include "helptext.h"
#include "stream.h"

#define TELEM_PORT 50002

stream_t telem_stream;

/* End of stream detected */
void stream_over(void) {
    int err;
    printf("End of stream.\n");
    err = stream_disconnect(&telem_stream);
    if (err) {
        fprintf(stderr, "Couldn't close connection: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

/* Handle Ctrl + C (SIGINT) */
void handle_int(int sig) {
    (void)sig;
    int err = stream_disconnect(&telem_stream);
    exit(err);
}

int main(int argc, char **argv) {

    /* Parse command line options. */

    int c;
    while ((c = getopt(argc, argv, ":h")) != -1) {
        switch (c) {
        case 'h':
            puts(HELP_TEXT);
            exit(EXIT_SUCCESS);
            break;
        case '?':
            fprintf(stderr, "Unknown option -%c\n", optopt);
            exit(EXIT_FAILURE);
            break;
        }
    }

    int err;

    err = stream_init(&telem_stream, "224.0.0.10", TELEM_PORT);
    if (err) {
        fprintf(stderr, "Could not initialize telemetry stream: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, handle_int);

    /* Get messages forever */

    header_p hdr;
    ssize_t b_read;
    for (;;) {

        b_read = stream_recv(&telem_stream, &hdr, sizeof(hdr));

        if (b_read == 0) {
            stream_over();
        } else if (b_read == -1) {
            fprintf(stderr, "Stream error: %s\n", strerror(errno));
            stream_disconnect(&telem_stream);
            exit(EXIT_FAILURE);
        }

        /* Quit if we receive something other than telemetry */

        if (hdr.type != TYPE_TELEM) {
            fprintf(stderr, "Received non-telemetry message: %u\n", hdr.type);
            exit(EXIT_FAILURE);
        }

        /* Log the telemetry packet received. */
        // TODO: do something with b_read here
        switch ((telem_subtype_e)hdr.subtype) {
        case TELEM_TEMP: {
            temp_p temp;
            b_read = stream_recv(&telem_stream, &temp, sizeof(temp));
            printf("Thermocouple #%u: %d C @ %u ms\n", temp.id, temp.temperature / 1000, temp.time);
        } break;
        case TELEM_PRESSURE: {
            pressure_p pres;
            b_read = stream_recv(&telem_stream, &pres, sizeof(pres));
            printf("Pressure transducer #%u: %u PSI @ %u ms\n", pres.id, pres.pressure / 1000, pres.time);
        } break;
        case TELEM_MASS: {
            mass_p mass;
            b_read = stream_recv(&telem_stream, &mass, sizeof(mass));
            printf("Load cell #%u: %u kg @ %u ms\n", mass.id, mass.mass / 1000, mass.time);
        } break;
        case TELEM_ACT: {
            act_state_p act;
            b_read = stream_recv(&telem_stream, &act, sizeof(act));
            printf("Actuator #%u: %s @ %u ms\n", act.id, act.state ? "on" : "off", act.time);
        } break;
        case TELEM_ARM: {
            arm_state_p arm;
            b_read = stream_recv(&telem_stream, &arm, sizeof(arm));
            printf("Arming state change to: %s # %u ms\n", arm_state_str(arm.state), arm.time);
        } break;
        case TELEM_WARN: {
            warn_p warn;
            b_read = stream_recv(&telem_stream, &warn, sizeof(warn));
            printf("WARNING: %s # %u ms\n", warning_str(warn.type), warn.time);
        } break;
        }
    }

    return 0;
}
