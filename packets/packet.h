#ifndef _PACKET_H_
#define _PACKET_H_

#include <stdbool.h>
#include <stdint.h>

#define PACKED __attribute__((packed))

/* PACKET HEADERS */

/* Packet header */
typedef struct {
    uint8_t type;    /* Message type */
    uint8_t subtype; /* Message sub-type */
} PACKED header_p;

/* Valid packet types */
typedef enum {
    TYPE_CNTRL = 0, /* Control */
    TYPE_TELEM = 1, /* Telemetry */
} packet_type_e;

/* Valid control message sub-types */
typedef enum {
    CNTRL_ACT_REQ = 0, /* Actuation request */
    CNTRL_ACT_ACK = 1, /* Actuation acknowledgement */
    CNTRL_ARM_REQ = 2, /* Arming request */
    CNTRL_ARM_ACK = 3, /* Arming acknowledgement */
} cntrl_subtype_e;

/* Valid telemetry message sub-types */
typedef enum {
    TELEM_TEMP = 0,     /* Temperature measurement */
    TELEM_PRESSURE = 1, /* Pressure measurement */
    TELEM_MASS = 2,     /* Mass measurement */
    TELEM_THRUST = 3,   /* Thrust measurement */
    TELEM_ARM = 4,      /* Arming state */
    TELEM_ACT = 5,      /* Actuator state */
    TELEM_WARN = 6,     /* Warning message */
    TELEM_CONT = 7,     /* Continuity measurement */
    TELEM_CONN = 8,     /* Connection status */
} telem_subtype_e;

/* CONTROL MESSAGES */

/* Actuation request packet */
typedef struct {
    uint8_t id;    /* Numerical ID of the actuator */
    uint8_t state; /* State for the actuator to transition to */
} PACKED act_req_p;

/* Actuation acknowledgement packet */
typedef struct {
    uint8_t id;     /* Numerical ID of the actuator */
    uint8_t status; /* Status of actuation request */
} PACKED act_ack_p;

/* Actuation acknowledgement statuses */
typedef enum {
    ACT_OK = 0,     /* The request was processed without any errors */
    ACT_DENIED = 1, /* The request was denied due to arming level being too low */
    ACT_DNE = 2,    /* The actuator ID is in the request was not associated with any actuator on the system */
    ACT_INV = 3,    /* The state requested was invalid */
} PACKED act_ack_status_e;

/* Arming request packet */
typedef struct {
    uint8_t level; /* The new arming level requested */
} PACKED arm_req_p;

typedef enum {
    ARMED_PAD = 0,      /* The pad control box is armed */
    ARMED_VALVES = 1,   /* The control input box is armed, permitting control over solenoid valves. */
    ARMED_IGNITION = 2, /* The pad control box is armed for ignition, and ignition circuitry is powered. Actuating quick
                           disconnect is now permitted. */
    ARMED_DISCONNECTED = 3, /* The quick disconnect has been disconnected. The ignitor can now be ignited. */
    ARMED_LAUNCH = 4,       /* The ignitor has been ignited. The main fire valve can now be opened. */
} arm_lvl_e;

/* Arming acknowledgement packet */
typedef struct {
    uint8_t status; /* The status of the arming request just issued. */
} PACKED arm_ack_p;

typedef enum {
    ARM_OK = 0, /* The arming level requested has been transitioned to. */
    ARM_DENIED =
        1, /* The arming request was denied because the current arming level cannot transition to the new level. */
    ARM_INV = 2, /* The arming level requested is not a valid arming level */
} arm_ack_status_e;

/* TELEMETRY MESSAGES */

/* Temperature measurement message */
typedef struct {
    uint32_t time;       /* Time stamp in milliseconds since power on. */
    int32_t temperature; /* Temperature in millidegrees Celsius. */
    uint8_t id;          /* The ID of the sensor which reported the measurement. */
} PACKED temp_p;

/* Pressure measurement message */
typedef struct {
    uint32_t time;    /* Time stamp in milliseconds since power on. */
    int32_t pressure; /* Pressure in thousandths of a PSI. */
    uint8_t id;       /* The ID of the sensor which reported the measurement. */
} PACKED pressure_p;

/* Mass measurement message */
typedef struct {
    uint32_t time; /* Time stamp in milliseconds since power on. */
    int32_t mass;  /* Mass in grams. */
    uint8_t id;    /* The ID of the sensor which reported the measurement. */
} PACKED mass_p;

typedef struct {
    uint32_t time;   /* Time stamp in milliseconds since power on. */
    uint32_t thrust; /* Thrust in Newtons. */
    uint8_t id;      /* The ID of the sensor which reported the measurement. */
} PACKED thrust_p;

/* Arming state message */
typedef struct {
    uint32_t time; /* Time stamp in milliseconds since power on. */
    uint8_t state; /* The current arming state. */
} PACKED arm_state_p;

/* Actuator state message */
typedef struct {
    uint32_t time; /* Time stamp in milliseconds since power on. */
    uint8_t id;    /* The numerical ID of the actuator */
    uint8_t state; /* The current state of the actuator. */
} PACKED act_state_p;

/* Warning message */
typedef struct {
    uint32_t time; /* Time stamp in milliseconds since power on. */
    uint8_t type;  /* The type of warning. */
} PACKED warn_p;

/* Warning types */
typedef enum {
    WARN_HIGH_PRESSURE = 0, /* Pressure levels have exceeded the threshold and manual intervention is required. */
    WARN_HIGH_TEMP = 1,     /* Temperature levels have exceeded the threshold and manual intervention is required. */
} warn_type_e;

/* Continuity state message */
typedef struct {
    uint32_t time; /* Time stamp in milliseconds since power on. */
    uint8_t state; /* The current state of the continuity check. */
} PACKED continuity_state_p;

typedef enum {
    CONTINUITY_LOW = 0,  /* Continuity sensor is reading low, circuit is open. */
    CONTINUITY_HIGH = 1, /* Continuity sensor is reading high, circuit is cloesed. */
} continuity_state_e;

/* Connection status message */
typedef struct {
    uint32_t time;  /* Time stamp in milliseconds since power on. */
    uint8_t status; /* The current status of the control client connection */
} PACKED conn_status_p;

typedef enum {
    CONN_CONNECTED = 0,    /* The control client is connected */
    CONN_RECONNECTING = 1, /* Re-connection to the control client being attempted */
    CONN_DISCONNECTED = 2, /* Control client disconnected, re-connect failed */
} conn_status_e;

/* PACKET HEADERS */

void packet_header_init(header_p *hdr, packet_type_e type, uint8_t subtype);

/* CONTROL MESSAGES */

void packet_act_req_init(act_req_p *req, uint8_t id, bool state);
void packet_act_ack_init(act_ack_p *ack, uint8_t id, act_ack_status_e status);
void packet_arm_req_init(arm_req_p *req, arm_lvl_e level);
void packet_arm_ack_init(arm_ack_p *ack, arm_ack_status_e status);

/* TELEMETRY MESSAGES */

void packet_temp_init(temp_p *p, uint8_t id, uint32_t time, int32_t temperature);
void packet_pressure_init(pressure_p *p, uint8_t id, uint32_t time, int32_t pressure);
void packet_mass_init(mass_p *p, uint8_t id, uint32_t time, int32_t mass);
void packet_thrust_init(thrust_p *p, uint8_t id, uint32_t time, uint32_t thrust);
void packet_arm_state_init(arm_state_p *p, uint32_t time, arm_lvl_e state);
void packet_act_state_init(act_state_p *p, uint8_t id, uint32_t time, bool state);
void packet_warn_init(warn_p *p, uint32_t time, warn_type_e type);
void packet_continuity_state_init(continuity_state_p *p, uint32_t time, continuity_state_e state);
void packet_conn_init(conn_status_p *p, uint32_t time, conn_status_e status);

const char *warning_str(warn_type_e warning);
const char *arm_state_str(arm_lvl_e state);

#endif // _PACKET_H_
