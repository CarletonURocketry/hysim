#include "errno.h"
#include <arpa/inet.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../packets/packet.h"
#include "../../pad_server/src/actuator.h"
#include "helptext.h"
#include "pad.h"
#include "switch.h"

typedef struct {
    uint8_t level;
} arm_t;

typedef struct {
    char key;
    switch_t *sw;
} command_t;

static switch_t switches[] = {

    /* Actuator switches */

    {.act_id = ID_FIRE_VALVE, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV1, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV2, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV3, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV4, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV5, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV6, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV7, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV8, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV9, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV10, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV11, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_XV12, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_QUICK_DISCONNECT, .state = false, .kind = CNTRL_ACT_REQ},
    {.act_id = ID_IGNITER, .state = false, .kind = CNTRL_ACT_REQ},

    /* Arming level commands */

    {.kind = CNTRL_ARM_REQ, .state = false, .act_id = ARMED_PAD},
    {.kind = CNTRL_ARM_REQ, .state = false, .act_id = ARMED_VALVES},
    {.kind = CNTRL_ARM_REQ, .state = false, .act_id = ARMED_IGNITION},
    {.kind = CNTRL_ARM_REQ, .state = false, .act_id = ARMED_DISCONNECTED},
    {.kind = CNTRL_ARM_REQ, .state = false, .act_id = ARMED_LAUNCH},
    {.kind = CNTRL_ARM_REQ, .state = false, .act_id = 5},
    {.kind = CNTRL_ARM_REQ, .state = false, .act_id = 6},
    {.kind = CNTRL_ARM_REQ, .state = false, .act_id = 7},
};

static command_t commands[] = {
    {.key = 'q', .sw = &switches[0]},  {.key = 'w', .sw = &switches[1]},  {.key = 'e', .sw = &switches[2]},
    {.key = 'r', .sw = &switches[3]},  {.key = 't', .sw = &switches[4]},  {.key = 'y', .sw = &switches[5]},
    {.key = 'u', .sw = &switches[6]},  {.key = 'i', .sw = &switches[7]},  {.key = 'p', .sw = &switches[8]},
    {.key = 'a', .sw = &switches[9]},  {.key = 's', .sw = &switches[10]}, {.key = 'd', .sw = &switches[11]},
    {.key = 'f', .sw = &switches[12]}, {.key = 'g', .sw = &switches[13]}, {.key = 'h', .sw = &switches[14]},
    {.key = 'z', .sw = &switches[15]}, {.key = 'x', .sw = &switches[16]}, {.key = 'c', .sw = &switches[17]},
    {.key = 'v', .sw = &switches[18]}, {.key = 'b', .sw = &switches[19]}, {.key = 'n', .sw = &switches[20]},
    {.key = 'm', .sw = &switches[21]}, {.key = ',', .sw = &switches[22]},
};

uint16_t port = 50001; /* Default port */
pad_t pad;
arm_t arm;

void handle_term(int sig) {
    (void)sig;
    pad_disconnect(&pad);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    char *ip = "127.0.0.1";
    pad.sock = -1;

    /* Parse command line options. */

    int c;
    while ((c = getopt(argc, argv, ":a:p:h")) != -1) {
        switch (c) {
        case 'h':
            puts(HELP_TEXT);
            exit(EXIT_SUCCESS);
            break;

        case 'a':
            ip = optarg;
            struct in_addr temp_addr;
            if (inet_pton(AF_INET, ip, &temp_addr) != 1) {
                fprintf(stderr, "Invalid pad_server address %s\n", ip);
                exit(EXIT_FAILURE);
            }
            break;

        case 'p':
            port = atoi(optarg);
            break;
        case '?':
            fprintf(stderr, "Unknown option -%c\n", optopt);
            exit(EXIT_FAILURE);
            break;
        }
    }

    int err;

    signal(SIGINT, handle_term);

    char key;
    for (;;) {
        fprintf(stderr, "Waiting for pad...\n");
        err = pad_init(&pad, ip, port);
        if (err) {
            fprintf(stderr, "Could not initialize pad server with error: %s\n", strerror(err));
            exit(EXIT_FAILURE);
        }

        err = pad_connect_forever(&pad);
        if (err) {
            fprintf(stderr, "Could not connect to pad server with error: %s\n", strerror(err));
            exit(EXIT_FAILURE);
        }

        printf("Connection established!\n");

        /* Send messages in a loop */

        for (;;) {

            printf("Press key and hit enter: ");
            key = getc(stdin);
            if (key != '\n') {
                while (getc(stdin) != '\n')
                    ;
            }

            for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
                if (commands[i].key == key) {
                    err = switch_callback(commands[i].sw, &pad, !commands[i].sw->state);
                } else if (i == (sizeof(commands) / sizeof(commands[0]) - 1)) {
                    fprintf(stderr, "Invalid key: %c\n", key);
                }
            }

            /* Print a helpful error message */

            switch (err) {
            case 0:
                printf("Switch actuated successfully\n");
                break;
            case EPERM:
                fprintf(stderr, "Permission denied\n");
                break;
            case EINVAL:
                fprintf(stderr, "Invalid actuator/arming level.\n");
            case ENODEV:
                fprintf(stderr, "No such actuator/arming level exists\n");
                break;
            default:
                fprintf(stderr, "Something went wrong: %d\n", err);
                break;
            }

            if (err) {
                // TODO: be graceful?
                break;
            }
        }
    }

    pad_disconnect(&pad);
    return 0;
}
