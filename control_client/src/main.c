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

typedef struct {
    uint8_t act_id;
    bool state;
} switch_t;

typedef struct {
    uint8_t level;
} arm_t;

typedef struct {
    char key;
    cntrl_subtype_e subtype;
    void *priv;
} command_t;

static switch_t switches[] = {
    {.act_id = ID_FIRE_VALVE, .state = false}, {.act_id = ID_XV1, .state = false},
    {.act_id = ID_XV2, .state = false},        {.act_id = ID_XV3, .state = false},
    {.act_id = ID_XV4, .state = false},        {.act_id = ID_XV5, .state = false},
    {.act_id = ID_XV6, .state = false},        {.act_id = ID_XV7, .state = false},
    {.act_id = ID_XV8, .state = false},        {.act_id = ID_XV9, .state = false},
    {.act_id = ID_XV10, .state = false},       {.act_id = ID_XV11, .state = false},
    {.act_id = ID_XV12, .state = false},       {.act_id = ID_QUICK_DISCONNECT, .state = false},
    {.act_id = ID_IGNITER, .state = false},
};

static command_t commands[] = {
    {.key = 'q', .subtype = CNTRL_ACT_REQ, .priv = &switches[0]},
    {.key = 'w', .subtype = CNTRL_ACT_REQ, .priv = &switches[1]},
    {.key = 'e', .subtype = CNTRL_ACT_REQ, .priv = &switches[2]},
    {.key = 'r', .subtype = CNTRL_ACT_REQ, .priv = &switches[3]},
    {.key = 't', .subtype = CNTRL_ACT_REQ, .priv = &switches[4]},
    {.key = 'y', .subtype = CNTRL_ACT_REQ, .priv = &switches[5]},
    {.key = 'u', .subtype = CNTRL_ACT_REQ, .priv = &switches[6]},
    {.key = 'i', .subtype = CNTRL_ACT_REQ, .priv = &switches[7]},
    {.key = 'p', .subtype = CNTRL_ACT_REQ, .priv = &switches[8]},
    {.key = 'a', .subtype = CNTRL_ACT_REQ, .priv = &switches[9]},
    {.key = 's', .subtype = CNTRL_ACT_REQ, .priv = &switches[10]},
    {.key = 'd', .subtype = CNTRL_ACT_REQ, .priv = &switches[11]},
    {.key = 'f', .subtype = CNTRL_ACT_REQ, .priv = &switches[12]},
    {.key = 'g', .subtype = CNTRL_ACT_REQ, .priv = &switches[13]},
    {.key = 'h', .subtype = CNTRL_ACT_REQ, .priv = &switches[14]},

    {.key = 'z', .subtype = CNTRL_ARM_REQ, .priv = (int *)ARMED_PAD},
    {.key = 'x', .subtype = CNTRL_ARM_REQ, .priv = (int *)ARMED_VALVES},
    {.key = 'c', .subtype = CNTRL_ARM_REQ, .priv = (int *)ARMED_IGNITION},
    {.key = 'v', .subtype = CNTRL_ARM_REQ, .priv = (int *)ARMED_DISCONNECTED},
    {.key = 'b', .subtype = CNTRL_ARM_REQ, .priv = (int *)ARMED_LAUNCH},
    {.key = 'n', .subtype = CNTRL_ARM_REQ, .priv = (int *)5},
    {.key = 'm', .subtype = CNTRL_ARM_REQ, .priv = (int *)6},
    {.key = ',', .subtype = CNTRL_ARM_REQ, .priv = (int *)7},
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
    pad.sock = -1;

    /* Parse command line options. */

    int c;
    while ((c = getopt(argc, argv, ":p:h")) != -1) {
        switch (c) {
        case 'h':
            puts(HELP_TEXT);
            exit(EXIT_SUCCESS);
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

    signal(SIGINT, handle_term);
    char key;
    for (;;) {
        int err;
        bool pad_connected = true;

        fprintf(stderr, "Waiting for pad...\n");
        err = pad_init(&pad, "127.0.0.1", port);
        if (err) {
            fprintf(stderr, "Could not initialize pad server with error: %s\n", strerror(err));
            exit(EXIT_FAILURE);
        }

        /* Handle disconnects */

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
                    header_p hdr = {.type = TYPE_CNTRL, .subtype = commands[i].subtype}; // create header
                    ssize_t iovlen = 2;
                    struct iovec iov[iovlen];
                    iov[0].iov_base = &hdr;
                    iov[0].iov_len = sizeof(hdr);

                    // check what type of command it is
                    switch (commands[i].subtype) {
                    case CNTRL_ACT_REQ: {
                        switch_t *actuator = commands[i].priv; // get the actuator data
                        act_req_p act_req = {.id = actuator->act_id, .state = !actuator->state}; // Create message

                        iov[1].iov_base = &act_req;
                        iov[1].iov_len = sizeof(act_req);
                        pad_send(&pad, iov, iovlen);

                        act_ack_p act_ack;
                        err = pad_recv(&pad, &act_ack, sizeof(act_ack));
                        if (err <= 0) {
                            fprintf(stderr, "Server did not acknowledge actuator request with id %d, reconnecting...\n",
                                    actuator->act_id);
                            pad_disconnect(&pad);
                            pad_connected = false;
                            break;
                        }

                        switch (act_ack.status) {
                        case ACT_OK:
                            fprintf(stderr,
                                    "The pad control system has put the actuator with id %d in the requested state\n",
                                    act_ack.id);
                            actuator->state = !actuator->state; // flipping the state of the actuator
                            break;
                        case ACT_DENIED:
                            fprintf(stderr, "The current level is too low to operate the actuator with id %d\n",
                                    act_ack.id);
                            break;
                        case ACT_DNE:
                            fprintf(stderr, "Actuator id %d is not associated with any actuator\n", act_ack.id);
                            break;
                        case ACT_INV:
                            fprintf(stderr, "Invalid state was requested for actuator with id %d\n", act_ack.id);
                            break;
                        }
                    } break;

                    case CNTRL_ARM_REQ: {
                        uint8_t *level = commands[i].priv;             // get arming level data
                        arm_req_p arm_req = {.level = (uint8_t)level}; // create message

                        iov[1].iov_base = &arm_req;
                        iov[1].iov_len = sizeof(arm_req);
                        pad_send(&pad, iov, iovlen);

                        arm_ack_p arm_ack;
                        err = pad_recv(&pad, &arm_ack, sizeof(arm_ack));
                        if (err <= 0) {
                            fprintf(stderr,
                                    "Server did not acknowledge arming request with level %d, reconnecting...\n",
                                    arm_req.level);
                            pad_disconnect(&pad);
                            pad_connected = false;
                            break;
                        }

                        switch (arm_ack.status) {
                        case ARM_OK:
                            fprintf(stderr, "Arming request with level %d was processed succesfully\n", arm_req.level);
                            arm.level = arm_req.level; // saving the arming level
                            break;
                        case ARM_DENIED:
                            fprintf(
                                stderr,
                                "The arming request with level %d could not be completed because the current arming "
                                "level is not high enough for that progression, or the progression must be caused via "
                                "an actuator command\n",
                                arm_req.level);
                            break;
                        case ARM_INV:
                            fprintf(stderr, "Invalid arming level %d was specified\n", arm_req.level);
                            break;
                        }
                    } break;

                    case CNTRL_ACT_ACK:
                    case CNTRL_ARM_ACK:
                        break;
                    }
                    break;
                }

                if (i == (sizeof(commands) / sizeof(commands[0]) - 1)) {
                    fprintf(stderr, "Invalid key: %c", key);
                }
            }
            putchar('\n');
            if (pad_connected == false) {
                break;
            }
        }
    }

    pad_disconnect(&pad);
    return 0;
}
