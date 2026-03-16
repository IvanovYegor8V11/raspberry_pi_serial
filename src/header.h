#ifndef HEADER_H
#define HEADER_H

#include <stdint.h>

const uint8_t PACKET_SIZE = 6;
const uint8_t HEADER_SIZE = 2;
const uint8_t DATA_SIZE = 4;

const uint32_t CMD_OPEN_IMAGE_1 = 0x4E49504F;
const uint32_t CMD_OPEN_IMAGE_2 = 0x4D49504F;

#endif // HEADER_H