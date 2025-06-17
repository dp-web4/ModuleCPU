/* ModuleCPU
 *
 * Module CPU - SPI engine.
 *
 */

#include <stdint.h>
#include <xc.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "main.h"
#include "SPI.h"

// Definitions for SPI bus
#define	DDR_SPI			DDRB
#define PORT_SPI		PORTB
#define MOSI			PINB1
#define MISO			PINB0
#define SCK				PINB7

// Speed of Fclkio (matches CPU speed)
#define FCLKIO			((CPU_SPEED) >> 1)

// Do a SPI transaction - read or write (or write pattern)
void SPITransaction(ESPIBusState eSPIBusState,
					uint8_t *pu8Buffer,
					uint16_t u16ByteCount)
{
	// If we're transmitting, start the transaction
	if (ESTATE_RX_DATA == eSPIBusState)
	{
		while (u16ByteCount--)
		{
			// Clock out 0xff so we can receive the data
			SPDR = 0xff;

			// Wait for data to be available
			while (0 == (SPSR & (1 << SPIF)));
			
			// Read the byte
			*pu8Buffer = SPDR;
			++pu8Buffer;
		}
	}
	else
	if (ESTATE_TX_DATA == eSPIBusState)
	{
		// Clock out whatever our data is in our queue
		while (u16ByteCount--)
		{
			SPDR = *pu8Buffer;
			++pu8Buffer;
			
			// Wait for the transmit to complete
			while (0 == (SPSR & (1 << SPIF)));
		}
	}
	else
	if (ESTATE_TX_PATTERN == eSPIBusState)
	{
		// Clock out whatever our data is in our queue
		while (u16ByteCount--)
		{
			// This is OK - as the position where the pointer would normally
			// be is just the pattern (single byte)
			SPDR = (uint8_t) (pu8Buffer);
		
			// Wait for the transmit to complete
			while (0 == (SPSR & (1 << SPIF)));
		}
	}
}

typedef struct SBaudRateTable
{
	uint32_t u32BaudRate;		// Our baud rate with this divisor
	uint8_t u8SPCR;				// Bits for divisor for SPCR
	uint8_t u8SPSR;				// Bits for divisor for SPSR
} SBaudRateTable;

static const SBaudRateTable sg_sSPIBaudRates []=
{
	{FCLKIO >> 1,	(0 << SPR1) | (0 << SPR0),	(1 << SPI2X)},		// /2
	{FCLKIO >> 2,	(0 << SPR1) | (0 << SPR0),	0},					// /4
	{FCLKIO >> 3,	(0 << SPR1) | (1 << SPR0),	(1 << SPI2X)},		// /8
	{FCLKIO >> 4,	(0 << SPR1) | (1 << SPR0),	0},					// /16
	{FCLKIO >> 5,	(1 << SPR1) | (0 << SPR0),	(1 << SPI2X)},		// /32
	{FCLKIO >> 6,	(1 << SPR1) | (0 << SPR0),	(1 << SPI2X)},		// /64
	{FCLKIO >> 7,	(1 << SPR1) | (0 << SPR0),	0},					// /128
};

// Sets the SPI baud rate and returns the actual baud rate selected
uint32_t SPISetBaudRate(uint32_t u32BaudRate)
{
	uint8_t u8Loop;
	
	// Find the baud rate that's the closest
	for (u8Loop = 0; u8Loop < (sizeof(sg_sSPIBaudRates) / sizeof(sg_sSPIBaudRates[0])); u8Loop++)
	{
		if (u32BaudRate >= sg_sSPIBaudRates[u8Loop].u32BaudRate)
		{
			break;
		}
	}

	// If we've hit the end, use the slowest speed
	if ((sizeof(sg_sSPIBaudRates) / sizeof(sg_sSPIBaudRates[0])) == u8Loop)
	{
		u8Loop--;
	}

	// Found one!
	SPCR = (SPCR & (uint8_t) ~((1 << SPR1) | (1 << SPR0))) | sg_sSPIBaudRates[u8Loop].u8SPCR;
	SPSR = (SPSR & (uint8_t) ~(1 << SPI2X)) | sg_sSPIBaudRates[u8Loop].u8SPSR;
	return(sg_sSPIBaudRates[u8Loop].u32BaudRate);
}

void SPIInit(void)
{
	// Set MOSI and SCK to output
	DDR_SPI |= (1 << MOSI) | (1 << SCK);
	DDR_SPI &= (uint8_t) ~(1 << MISO);
	
	// MISO As input (need our pullup)
	PORT_SPI |= (1 << MISO);
	
	// Enable SPI engine, master only, and clock to fosc/128 and SPI mode 0 (which is what SD wants)
	SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0);
	
	// Zero out SPSR (all zeroes)
	SPSR = 0;
	
	// Zero out SPIPS so the SPI IP is connected to MISO, MOSI, SCK, and SS
	MCUCR &= (uint8_t) ~(1 << SPIPS);

	// Set to something slow/compatible
	(void) SPISetBaudRate(400000);
}