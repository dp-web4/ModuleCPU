#include <stdint.h>
#include <xc.h>
#include <stdbool.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include "EEPROM.h"

void EEPROMWrite(uint16_t u16Address,
				 uint8_t u8Data)
{
	// Wait for any completion of writes, etc..
	while (EECR & (1 << EEWE));

	// Set address
	EEAR = u16Address;
	
	// And the data
	EEDR = u8Data;
	
	// Indicate a write phase
	EECR |= (1 << EEMWE);
	
	// Start write
	EECR |= (1 << EEWE);
}

uint8_t EEPROMRead(uint16_t u16Address)
{
	// Wait for any completion of writes, etc..
	while (EECR & (1 << EEWE));

	// Set address	
	EEAR = u16Address;
	
	// Start EEPROM read
	EECR |= (1 << EERE);
	
	// Return the data
	return(EEDR);
}