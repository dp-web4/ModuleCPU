#ifndef _EEPROM_H_
#define _EEPROM_H_

// Size of EEPROM in bytes
#define		EEPROM_SIZE						2048

// 
// Offsets for the module controller. EEPROM Is 2K in size
//
#define		EEPROM_UNIQUE_ID					0x0000
#define		EEPROM_EXPECTED_CELL_COUNT			(EEPROM_UNIQUE_ID + sizeof(uint32_t))
#define		EEPROM_MAX_CHARGE_CURRENT			(EEPROM_EXPECTED_CELL_COUNT + sizeof(uint8_t))
#define		EEPROM_MAX_DISCHARGE_CURRENT		(EEPROM_MAX_CHARGE_CURRENT + sizeof(uint16_t))
#define		EEPROM_SEQUENTIAL_COUNT_MISMATCH	(EEPROM_MAX_DISCHARGE_CURRENT + sizeof(uint16_t))

extern void EEPROMWrite(uint16_t u16Address,
						uint8_t u8Data);
extern uint8_t EEPROMRead(uint16_t u16Address);

#endif
