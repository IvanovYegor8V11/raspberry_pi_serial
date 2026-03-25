#ifndef HEADER_H
#define HEADER_H

#include <stdint.h>

const uint8_t OPEN              = 0x4F;
const uint8_t LOAD              = 0x4C;
const uint8_t PIC               = 0x50;

const uint8_t ACK               = 0xAA;
const uint8_t END               = 0xFF;

const uint8_t PACKET_SIZE       = 6;
const uint8_t HEADER_SIZE       = 2;
const uint8_t DATA_SIZE         = 4;

const uint16_t BUFFER_SIZE      = 1024;

#endif // HEADER_H