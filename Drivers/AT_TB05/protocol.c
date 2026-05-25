#include "protocol.h"
#include <string.h>

int sqb_pack(uint8_t *packet, uint8_t type, uint16_t bodylen, const uint8_t *body) {
    int ret;
    ret = bodylen + 4;
    packet[0] = SQB_HEAD;
    packet[1] = type;
    packet[2] = bodylen & 0xff;
    packet[3] = bodylen >> 8;
    memcpy(packet + 4, body, bodylen);
    return ret;
}

uint8_t sqb_type(const uint8_t *packet) {
    return packet[1];
}

uint16_t sqb_bodylen(const uint8_t *packet) {
    uint16_t ret;
    ret = packet[2] + (packet[3] << 8);
    return ret;
}

uint8_t *sqb_body(uint8_t *packet) {
    return packet + 4;
}