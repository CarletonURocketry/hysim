#include <arpa/inet.h>
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
#define MULTICAST_ADDR "239.100.110.210"

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
    char *multicast_addr = "224.0.0.10";

    /* Parse command line options. */

    int c;
    while ((c = getopt(argc, argv, ":ha:")) != -1) {
        switch (c) {
        case 'h':
            puts(HELP_TEXT);
            exit(EXIT_SUCCESS);
            break;
        case 'a':
            multicast_addr = optarg;
            struct in_addr temp_addr;
            if (inet_pton(AF_INET, multicast_addr, &temp_addr) != 1) {
                fprintf(stderr, "Invalid multicast address %s\n", multicast_addr);
                exit(EXIT_FAILURE);
            }
            break;

        case '?':
            fprintf(stderr, "Unknown option -%c\n", optopt);
            exit(EXIT_FAILURE);
            break;
        }
    }

    int err;

    err = stream_init(&telem_stream, multicast_addr, TELEM_PORT);
    if (err) {
        fprintf(stderr, "Could not initialize telemetry stream: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, handle_int);

    /* Get messages forever */

    header_p hdr;
    ssize_t b_read;
    char buffer[50]; /* Large enough for any UDP packet */
    for (;;) {

        /* Necessary to peek at header because the 'stream' is a UDP datagram stream. Any `stream_recv` call will read
         * `n` bytes, but it will discard any remainder of the datagram. We peek at the header to make sure that on
         * subsequent recv calls we read *exactly* the number of bytes we're supposed to.
         */

        b_read = stream_peek(&telem_stream, &hdr, sizeof(hdr));

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
            b_read = stream_recv(&telem_stream, &buffer, sizeof(hdr) + sizeof(temp_p));
            temp_p *temp = (temp_p *)&buffer[sizeof(hdr)];
            printf("Thermocouple #%u: %d C @ %u ms\n", temp->id, temp->temperature / 1000, temp->time);
        } break;
        case TELEM_PRESSURE: {
            b_read = stream_recv(&telem_stream, &buffer, sizeof(pressure_p) + sizeof(hdr));
            pressure_p *pres = (pressure_p *)&buffer[sizeof(hdr)];
            printf("Pressure transducer #%u: %u PSI @ %u ms\n", pres->id, pres->pressure / 1000, pres->time);
        } break;
        case TELEM_MASS: {
            b_read = stream_recv(&telem_stream, &buffer, sizeof(mass_p) + sizeof(hdr));
            mass_p *mass = (mass_p *)&buffer[sizeof(hdr)];
            printf("Load cell #%u: %d kg @ %u ms\n", mass->id, mass->mass / 1000, mass->time);
        } break;
        case TELEM_ACT: {
            b_read = stream_recv(&telem_stream, &buffer, sizeof(act_state_p) + sizeof(hdr));
            act_state_p *act = (act_state_p *)&buffer[sizeof(hdr)];
            printf("Actuator #%u: %s @ %u ms\n", act->id, act->state ? "on" : "off", act->time);
        } break;
        case TELEM_ARM: {
            b_read = stream_recv(&telem_stream, &buffer, sizeof(arm_state_p) + sizeof(hdr));
            arm_state_p *arm = (arm_state_p *)&buffer[sizeof(hdr)];
            printf("Arming state: %s # %u ms\n", arm_state_str(arm->state), arm->time);
        } break;
        case TELEM_WARN: {
            b_read = stream_recv(&telem_stream, &buffer, sizeof(hdr) + sizeof(warn_p));
            warn_p *warn = (warn_p *)&buffer[sizeof(hdr)];
            printf("WARNING: %s # %u ms\n", warning_str(warn->type), warn->time);
        } break;
        }
    }

    return 0;
}
