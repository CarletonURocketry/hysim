#include <errno.h>
#include <stdbool.h>

#include "pad.h"
#include "switch.h"

/*
 * Verifies the response of an arming command.
 * @param pad The pad connection
 * @return 0 on success, errno code on failure.
 */
static int check_arm_response(pad_t *pad) {
    int err;
    arm_ack_p arm_ack;

    err = pad_recv(pad, &arm_ack, sizeof(arm_ack));
    if (err == -1) {
        return errno;
    }

    switch (arm_ack.status) {
    case ARM_OK:
        return 0;
    case ARM_DENIED:
        return EPERM;
    case ARM_INV:
        return EINVAL;
    }

    return 0;
}

/*
 * Verifies the response of an actuation command.
 * @param pad The pad connection
 * @return 0 on success, errno code on failure.
 */
static int check_act_response(pad_t *pad) {
    int err;
    act_ack_p act_ack;

    err = pad_recv(pad, &act_ack, sizeof(act_ack));
    if (err == -1) {
        return errno;
    }

    switch (act_ack.status) {
    case ACT_OK:
        return 0;
    case ACT_DENIED:
        return EPERM;
    case ACT_DNE:
        return ENODEV;
    case ACT_INV:
        return EINVAL;
    }

    return 0;
}

/*
 * Sends a network command to alter the actuator/arming state associated with this switch.
 * @param sw The switch that was flipped
 * @param pad The socket with connection to the pad
 * @param newstate The new state of the switch
 * @return 0 if okay, an errno otherwise
 */
int switch_callback(switch_t *sw, pad_t *pad, bool newstate) {

    /* Header for the command */

    header_p hdr = {.type = TYPE_CNTRL, .subtype = sw->kind};

    /* IOV containing header and space for command body */

    ssize_t iovlen = 2;
    struct iovec iov[iovlen];
    iov[0].iov_base = &hdr;
    iov[0].iov_len = sizeof(hdr);

    /* Mark new switch state */

    sw->state = newstate;

    /* Handle body depending on switch */

    switch (sw->kind) {
    case CNTRL_ACT_REQ: {
        act_req_p act_req = {.id = sw->act_id, .state = sw->state};
        iov[1].iov_base = &act_req;
        iov[1].iov_len = sizeof(act_req);
        pad_send(pad, iov, iovlen); // TODO: handle err
        return check_act_response(pad);
    } break;
    case CNTRL_ARM_REQ: {
        arm_req_p arm_req = {.level = sw->act_id};
        iov[1].iov_base = &arm_req;
        iov[1].iov_len = sizeof(arm_req);
        pad_send(pad, iov, iovlen); // TODO: handle err
        return check_arm_response(pad);
    } break;
    default:
        /* Invalid type of switch */
        return -EINVAL;
        break;
    }

    return 0;
}
