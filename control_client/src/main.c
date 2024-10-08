#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../../packets/packet.h"
#include "helptext.h"
#include "pad.h"
#include "../../pad_server/src/actuator.h"

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
    {.act_id = ID_FIRE_VALVE, .state = false}, {.act_id = ID_XV1, .state = false}, {.act_id = ID_XV2, .state = false},
    {.act_id = ID_XV3, .state = false}, {.act_id = ID_XV4, .state = false}, {.act_id = ID_XV5, .state = false},
    {.act_id = ID_XV6, .state = false}, {.act_id = ID_XV7, .state = false}, {.act_id = ID_XV8, .state = false},
    {.act_id = ID_XV9, .state = false}, {.act_id = ID_XV10, .state = false}, {.act_id = ID_XV11, .state = false},
    {.act_id = ID_XV12, .state = false}, {.act_id = ID_QUICK_DISCONNECT, .state = false}, {.act_id = ID_IGNITER, .state = false},
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

void handle_term(int sig) {
    (void)sig;
    pad_disconnect(&pad);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {

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

    int err;
    err = pad_init(&pad, "127.0.0.1", port);
    if (err) {
        fprintf(stderr, "Could not initialize pad server with error: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }

    /* Handle disconnects */
    signal(SIGINT, handle_term);

    err = pad_connect_forever(&pad);
    if (err) {
        fprintf(stderr, "Could not connect to pad server with error: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }

    printf("Connection established!\n");

    /* Send messages in a loop */

    char key;
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

                // check what type of command it is
                switch (commands[i].subtype) {
                case CNTRL_ACT_REQ:
                    switch_t *actuator = commands[i].priv; // get the actuator data
                    actuator->state = !actuator->state;    // flip the state
                    act_req_p act_req = {.id = actuator->act_id, .state = actuator->state}; // Create message

                    pad_send(&pad, &hdr, sizeof(hdr));
                    pad_send(&pad, &act_req, sizeof(act_req));
                    break;
                case CNTRL_ARM_REQ:
                    uint8_t *level = commands[i].priv; // get arming level data
                    arm_req_p arm_req = {.level = *level}; // create message

                    pad_send(&pad, &hdr, sizeof(hdr));
                    pad_send(&pad, &arm_req, sizeof(arm_req));
                    break;
                case CNTRL_ACT_ACK:
                case CNTRL_ARM_ACK:
                    break;
                } break;
            }

            if (i == (sizeof(commands) / sizeof(commands[0]) - 1)) {
                fprintf(stderr, "Invalid key: %c", key);
            }
        }
        putchar('\n');
    }

    pad_disconnect(&pad);
    return 0;
}