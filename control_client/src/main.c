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

typedef struct {
    char key;
    uint8_t act_id;
    bool state;
} switch_t;

static switch_t switches[] = {
    {.key = 'q', .act_id = 0, .state = false}, {.key = 'w', .act_id = 1, .state = false},
    {.key = 'e', .act_id = 2, .state = false}, {.key = 'r', .act_id = 3, .state = false},
    {.key = 't', .act_id = 4, .state = false}, {.key = 'y', .act_id = 5, .state = false},
    {.key = 'u', .act_id = 6, .state = false}, {.key = 'i', .act_id = 7, .state = false},
    {.key = 'o', .act_id = 8, .state = false},
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

    header_p hdr = {.type = TYPE_CNTRL, .subtype = CNTRL_ACT_REQ};
    char key;
    for (;;) {

        printf("Press key and hit enter: ");
        key = getc(stdin);
        if (key != '\n') {
            while (getc(stdin) != '\n')
                ;
        }

        for (unsigned int i = 0; i < sizeof(switches) / sizeof(switches[0]); i++) {
            if (switches[i].key == key) {
                switches[i].state = !switches[i].state;                                 // Flip switch
                act_req_p req = {.id = switches[i].act_id, .state = switches[i].state}; // Create message
                pad_send(&pad, &hdr, sizeof(hdr));
                pad_send(&pad, &req, sizeof(req));
                break;
            }

            if (i == (sizeof(switches) / sizeof(switches[0]) - 1)) {
                fprintf(stderr, "Invalid key: %c", key);
            }
        }
        putchar('\n');
    }

    pad_disconnect(&pad);
    return 0;
}
