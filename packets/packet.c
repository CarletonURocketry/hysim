#include "packet.h"

const char *WARNING_STR[] = {
    [WARN_HIGH_TEMP] = "High temperature",
    [WARN_HIGH_PRESSURE] = "High pressure",
};

const char *ARMING_STR[] = {
    [ARMED_PAD] = "Pad armed",
    [ARMED_VALVES] = "Valves armed",
    [ARMED_IGNITION] = "Armed for ignition",
    [ARMED_DISCONNECTED] = "Quick disconnect disconnected",
    [ARMED_LAUNCH] = "Armed for launch",
};

/* PACKET HEADERS */

void packet_header_init(header_p *hdr, packet_type_e type, uint8_t subtype) {
    hdr->type = (uint8_t)type;
    hdr->subtype = subtype;
}

/* CONTROL MESSAGES */

void packet_act_req_init(act_req_p *req, uint8_t id, bool state) {
    req->id = id;
    req->state = state ? 1 : 0;
}

void packet_act_ack_init(act_ack_p *ack, uint8_t id, act_ack_status_e status) {
    ack->id = id;
    ack->status = (uint8_t)status;
}

void packet_arm_req_init(arm_req_p *req, arm_lvl_e level) { req->level = (uint8_t)level; }

void packet_arm_ack_init(arm_ack_p *ack, arm_ack_status_e status) { ack->status = (uint8_t)status; }

/* TELEMETRY MESSAGES */

void packet_temp_init(temp_p *p, uint8_t id, uint32_t time, int32_t temperature) {
    p->id = id;
    p->time = time;
    p->temperature = temperature;
}

void packet_pressure_init(pressure_p *p, uint8_t id, uint32_t time, int32_t pressure) {
    p->id = id;
    p->time = time;
    p->pressure = pressure;
}

void packet_mass_init(mass_p *p, uint8_t id, uint32_t time, uint32_t mass) {
    p->id = id;
    p->time = time;
    p->mass = mass;
}

void packet_arm_state_init(arm_state_p *p, uint32_t time, arm_lvl_e state) {
    p->time = time;
    p->state = (uint8_t)state;
}

void packet_act_state_init(act_state_p *p, uint8_t id, uint32_t time, bool state) {
    p->time = time;
    p->id = id;
    p->state = state ? 1 : 0;
}

void packet_warn_init(warn_p *p, uint32_t time, warn_type_e type) {
    p->time = time;
    p->type = (uint8_t)type;
}

const char *arm_state_str(arm_lvl_e state) { return ARMING_STR[state]; }

const char *warning_str(warn_type_e warning) { return WARNING_STR[warning]; }
