#ifndef PROTOCOL_H
#define PROTOCOL_H

/*
    Definition of SQB (Simple Query Bytes)

    HEAD[1] TYPE[1] LEN[2] BODY[LEN] (EXT[Specified])

    HEAD: packet header
    TYPE: packet type
    LEN: length of packet body (little-endian)
    BODY: packet body
    EXT: extend part of specific type packet

*/

#include <stdint.h>

#define SQB_HEAD 0xf5

/* This kind of data packs provides no action */
#define SQB_TYPE_NONE 0x00

#define SQB_TYPE_INIT 0x01
#define SQB_TYPE_RESP 0x02
#define SQB_TYPE_SWITCH 0x03
#define SQB_TYPE_SELECT 0x04

/*
    Indicate a long pack, unimplemented

    For a long pack, BODY is only used as length of EXT (little-endian)
*/
#define SQB_TYPE_LONGPACK 0x03

/* return packet length */
int sqb_pack(uint8_t *packet, uint8_t type, uint16_t bodylen, const uint8_t *body);

uint8_t sqb_type(const uint8_t *packet);
uint16_t sqb_bodylen(const uint8_t *packet);
uint8_t *sqb_body(uint8_t *packet);

#endif