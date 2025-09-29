#ifndef _EEPROM_H_
#define _EEPROM_H_

// Size of EEPROM in bytes
#define		EEPROM_SIZE						2048

//
// Offsets for the module controller. EEPROM Is 2K in size
// First 64 bytes (0x00-0x3F) reserved for metadata
//
#define		EEPROM_METADATA_SIZE				64		// Reserve first 64 bytes for metadata

// Metadata area (0x0000 - 0x003F)
#define		EEPROM_UNIQUE_ID					0x0000
#define		EEPROM_EXPECTED_CELL_COUNT			(EEPROM_UNIQUE_ID + sizeof(uint32_t))
#define		EEPROM_MAX_CHARGE_CURRENT			(EEPROM_EXPECTED_CELL_COUNT + sizeof(uint8_t))
#define		EEPROM_MAX_DISCHARGE_CURRENT		(EEPROM_MAX_CHARGE_CURRENT + sizeof(uint16_t))
#define		EEPROM_SEQUENTIAL_COUNT_MISMATCH	(EEPROM_MAX_DISCHARGE_CURRENT + sizeof(uint16_t))
// Next offset would be 0x000B, leaving room until 0x003F

// Frame counter area (0x0040 - 0x023F)
// 512 bytes for wear leveling
#define		EEPROM_FRAME_COUNTER_BASE			0x0040
#define		EEPROM_FRAME_COUNTER_SIZE			512
#define		EEPROM_FRAME_COUNTER_END			(EEPROM_FRAME_COUNTER_BASE + EEPROM_FRAME_COUNTER_SIZE - 1)

extern void EEPROMWrite(uint16_t u16Address,
						uint8_t u8Data);
extern uint8_t EEPROMRead(uint16_t u16Address);

#endif
