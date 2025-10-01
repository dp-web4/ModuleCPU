#include "crc32.h"

// CRC32 calculation using bit-by-bit algorithm
// Polynomial: 0xEDB88320 (reversed 0x04C11DB7)
// This is slower than table-based but saves ~1KB of flash
uint32_t CRC32_Calculate(const uint8_t* data, uint16_t length)
{
	uint32_t crc = 0xFFFFFFFF;

	for (uint16_t i = 0; i < length; i++)
	{
		crc ^= data[i];

		for (uint8_t j = 0; j < 8; j++)
		{
			crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
		}
	}

	return ~crc;
}
