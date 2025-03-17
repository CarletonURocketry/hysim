#ifndef _CONTROL_SWITCH_H_
#define _CONTROL_SWITCH_H_

#include <stdint.h>

#include "../../packets/packet.h"
#include "pad.h"

/* Represents a switch's state */

typedef struct {
    uint8_t act_id;       /* Actuator ID associated with this switch */
    cntrl_subtype_e kind; /* The kind of switch (arming or actuator) */
    bool state;           /* State of this switch (on/off) */
} switch_t;

int switch_callback(switch_t *sw, pad_t *pad, bool newstate);

#endif // _CONTROL_SWITCH_H_
