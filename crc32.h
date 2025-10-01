#ifndef _CRC32_H_
#define _CRC32_H_

#include <stdint.h>

// Calculate CRC32 checksum of data buffer
extern uint32_t CRC32_Calculate(const uint8_t* data, uint16_t length);

#endif // _CRC32_H_
