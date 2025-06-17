/* ModuleCPU
 *
 * Module CPU - SD Engine
 *
 * Lots was inspired by the following site:
 *   http://www.rjhcoding.com/avrc-sd-interface-1.php
 *	 https://github.com/ryanj1234/SD_TUTORIAL_PART4
 */


#include <stdint.h>
#include <xc.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <string.h>
#include "main.h"
#include "SD.h"
#include "SPI.h"
//#include "FatFS/source/ff.h"
//#include "File.h"

// Chip select port and pin for SD
#define SD_CS_POUT			PORTC
#define	SD_CS_DDR			DDRC
#define SD_CS_PIN			PINC6

// Chip select pin for SD card
#define	SD_CS_ASSERT()		SD_CS_POUT &= (uint8_t) ~(1 << SD_CS_PIN)
#define	SD_CS_DEASSERT()	SD_CS_POUT |= (uint8_t) (1 << SD_CS_PIN)

// Card presence detection
//#define SD_DETECT_POUT		PORTD
//#define	SD_DETECT_PORTIN	PIND
//#define	SD_DETECT_DDR		DDRD
//#define SD_DETECT_PIN		PIND7

// true If a card is present, or false if it's not
//#define	SD_DETECT()			((SD_DETECT_PORTIN & (1 << SD_DETECT_PIN)) ? false : true)

// Basic block size on SD card
#define SD_BLOCK_SIZE		512

// # Of attempts to initialize an SD card
#define SD_GO_IDLE_RETRIES			100
#define SD_GO_OPERATIONAL_RETRIES	100

// # Of times we attempt to get a read queued up
#define SD_MAX_READ_ATTEMPTS		1563
#define SD_MAX_WRITE_ATTEMPTS		39070

// Baud rates
#define SD_SPEED_SLOW				400000
#define SD_SPEED_HIGH				11000000

// Misc SD defines
#define	SD_READY					0x00
#define SD_START_MULTI_TOKEN		0xfc
#define SD_START_TOKEN				0xfe
#define SD_STOP_TRANSACTION			0xfd
#define ACMD41_ARG_SDV1				0x00000000
#define ACMD41_ARG_SDV2				0x40000000

// # Of sectors in this device
static uint32_t sg_u32SDSectorCount = 0;
static uint16_t sg_u16BlockSize = 0;

// Card specific data structure
static uint8_t sg_u8CSD[16];

// This will provide a !CS assertion or deassertion with pre/postamble clock
static void SDSetCS(bool bAsserted)
{
	if (bAsserted)
	{
		SD_CS_ASSERT();
	}
	else
	{
		SD_CS_DEASSERT();
	}
}

// Sends SD powerup sequence to card and attempts to initialize it
static void SDPowerup(void)
{
	// Set SPI baud rate to (SD_SPEED_SLOW, which is the max for the init sequence)
	(void) SPISetBaudRate(SD_SPEED_SLOW);
	
	// Deassert chip select
	SD_CS_DEASSERT();
	
	// Delay time after possible deassertion (2ms)
	Delay(2000);
	
	// Send 74/80 bits of 0xff as a reset (80 bits required since we're byte-
	// sized transactions)
	SPIWritePattern(0xff,
				    128/8);

	// Delay time after write
	Delay(2000);
}

#define CMD55       55
#define CMD55_ARG   0x00000000
#define CMD55_CRC   0x00

// Go idle command
#define CMD0			0x00
#define CMD0_ARG		0x00000000
#define CMD0_CRC		0x94

#define CMD8        0x08
#define CMD8_ARG    0x0000001AA
#define CMD8_CRC    0x87

// Sends a command to the SD card, including argument an CRC
static uint8_t SDCommand(uint8_t u8Cmd,
						 uint32_t u32Arg)
{
	uint8_t u8CRC;
	uint8_t u8Buffer[sizeof(u8Cmd) + sizeof(u32Arg) + sizeof(u8CRC)];
	uint8_t u8Response;
	uint8_t u8Count;
	
	// If this is an ACMD, prefix a CMD55
	if (u8Cmd & 0x80)
	{
		u8Response = SDCommand(CMD55,
							   CMD55_ARG);
							   
		if (u8Response > 1)
		{
			return(u8Response);
		}
	}
	
	// SD Command (OR in 0x40 so bit 47 is always a 1
	u8Buffer[0] = (u8Cmd & 0x7f) | 0x40;
	
	// 32 Bits of argument
	u8Buffer[1] = (uint8_t) (u32Arg >> 24);
	u8Buffer[2] = (uint8_t) (u32Arg >> 16);
	u8Buffer[3] = (uint8_t) (u32Arg >> 8);
	u8Buffer[4] = (uint8_t) (u32Arg >> 0);
	
	// Valid CRCs for specific commands
	u8CRC = 0;
	if (CMD0 == u8Cmd)
	{
		u8CRC = 0x95;
	}
	
	if (CMD8 == u8Cmd)
	{
		u8CRC = 0x87;
	}
	
	// Now the CRC - Bit 0 means "ignore CRC"
	u8Buffer[5] = (uint8_t) (u8CRC | 0x01);
	
	// Send all bytes to the SD card
	SPIWrite(u8Buffer,
			 sizeof(u8Buffer));
			 
	u8Count = 10;
	
	do 
	{
		SPIRead(&u8Response,
				sizeof(u8Response));
	} while ((u8Response & 0x80) && --u8Count);
	
	return(u8Response);
}

// This will return true if a response 7 was received, otherwise false
static void SDWaitResponse7(uint8_t *pu8ResponseByte)
{
	// Go read the remaining 4 bytes
	SPIRead(pu8ResponseByte,
			sizeof(uint32_t));
}

// SD - Go idle!
static uint8_t SDGoIdle(uint8_t *pu8Response)
{
	uint8_t u8Response;
	
	SDSetCS(true);
	
	// Send our CMD0 (go idle)
	u8Response = SDCommand(CMD0,
						   CMD0_ARG);
						   
	SDSetCS(false);

	return(u8Response);
}

// Send interface condition
static uint8_t SDSendInterfaceCondition(uint8_t *pu8Response)
{
	uint8_t u8Response;
	
	SDSetCS(true);
	
	// Send our CMD8 Send interface condition
	u8Response = SDCommand(CMD8,
						   CMD8_ARG);
						   
	SDWaitResponse7(pu8Response);
		
	SDSetCS(false);

	return(u8Response);
}

#define CMD9        0x09
#define CMD9_ARG    0x000000000
#define CMD9_CRC    0x00

// Read CSD (card specific data) - CSD is 128 bits - see the SD spec for details.
static uint8_t SDReadCSD(uint8_t *pu8CSD)
{
	uint8_t u8Response;
	uint8_t u8CmdResponse;
	uint8_t u8Count = 8;
	
	SDSetCS(true);
	
	// Send our CMD9 (read CSD)
	u8CmdResponse = SDCommand(CMD9,
							  CMD9_ARG);
	
	// Success! Let's get a response
	if (u8CmdResponse)
	{
		goto errorExit;
	}
	
	// Wait until SD_START_TOKEN (start of data sector). We should easily see it in a few bytes.
	do 
	{
		SPIRead(&u8Response,
			    sizeof(u8Response));
	} while ((u8Response != SD_START_TOKEN) &&
			 (u8Count--));
			 
	// If we haven't seen it in u8Count bytes, then error the call
	if (0 == u8Count)
	{
		goto errorExit;
	}
	
	// Go read the remaining 16 bytes
	SPIRead(pu8CSD,
		    16);
	
errorExit:	
	SDSetCS(false);
	return(u8CmdResponse);
	
}

#define ACMD41      (0x80 | 41)
#define ACMD41_CRC  0x00

// Send operation condition
static uint8_t SDSendOpCondition(uint8_t *pu8Response,
								 uint32_t u32Arg)
{
	uint8_t u8Response;
	
	// Assert chip select
	SDSetCS(true);
	
	// Send the app command
	u8Response = SDCommand(ACMD41,
						   u32Arg);
	// Deassert chip select
	SDSetCS(false);
	
	return(u8Response);
}

#define CMD58       58
#define CMD58_ARG   0x00000000
#define CMD58_CRC   0x00

// Go read the OCR
static uint8_t SDReadOCR(uint8_t *pu8Response)
{
	uint8_t u8Response;
	
	// Assert chip select
	SDSetCS(true);
	
	// Send the app command
	u8Response = SDCommand(CMD58,
						   CMD58_ARG);

	// Read response
	SDWaitResponse7(pu8Response);

	// Deassert chip select
	SDSetCS(false);
	
	return(u8Response);
}

static uint8_t SDSendOpConditionArg(uint8_t *pu8Resp,
									uint32_t u32Arg)
{
	uint8_t u8Attempts;
	uint8_t u8Response;
	
	u8Attempts = 0;
		
	// Loop until it comes alive - if it comes alive. Put the card in idle state.
	while (u8Attempts < SD_GO_IDLE_RETRIES)
	{
		u8Response = SDSendOpCondition(pu8Resp,
									   u32Arg);
		if (0 == u8Response)
		{
			break;
		}
			
		// Give it a little time to settle
		Delay(10000);
		u8Attempts++;
	}

	// If we've retried too much, bail out. We're done.
	if (SD_GO_IDLE_RETRIES == u8Attempts)
	{
		u8Response = 0x80;
	}
	
	return(u8Response);
}

// Initializes the microSD card. Returns true if successful, otherwise false.
bool SDInit(void)
{
	bool bResult = false;
	uint8_t u8Attempts = 0;
	uint8_t u8Resp[5];				// Temporary response buffer
	uint8_t u8Response;
	
	
	// Set chip select to an output
	SD_CS_DDR |= (1 << SD_CS_PIN);
	
	// Enable pullup
	SD_CS_POUT |= (1 << SD_CS_PIN);

	SD_CS_DEASSERT();

	// Init the SPI bus
	SPIInit();
	
	bResult = false;  //assume nothing there
		
	// Let's try to initialize it.		
	SDPowerup();
		
	// Now go idle
	u8Attempts = 0;
	
	// Loop until it comes alive - if it comes alive. Put the card in idle state.
	while (u8Attempts < SD_GO_IDLE_RETRIES)
	{
		u8Response = SDGoIdle(u8Resp);
		if (0x01 == u8Response)
		{
			// We are idle!
			break;
		}
		
		// Give a 10ms delay for it to come alive
		Delay(10000);	
		u8Attempts++;
	}
		
	// If we've hit our max # of retries or err'd out, then fail the call
	if (SD_GO_IDLE_RETRIES == u8Attempts)
	{
		goto errorExit;
	}

	memset((void *) u8Resp, 0, sizeof(u8Resp));	
		
	// Go idle succeeded. Now we need to send interface condition.
	u8Response = SDSendInterfaceCondition(u8Resp);
	
	// If response byte is 0x01, this is an SDV2 card
	if (0x01 == u8Response)
	{
		// Byte 3 should be the echoed 0xaa
		if (u8Resp[3] != 0xaa)
		{
			goto errorExit;
		}

		// Now request operating condition
		u8Response = SDSendOpConditionArg(u8Resp,
										  ACMD41_ARG_SDV2);
		if (u8Response)
		{
			goto errorExit;
		}
		
		// Card is active! Now we need to read the OCR
		u8Response = SDReadOCR(u8Resp);
	
		// If the card is busy, it's in error state
		if (u8Response & 0x80)
		{
			// Error state
		}
		else
		{
			// Read the CSD so we can determine how big it is
			u8Response = SDReadCSD(sg_u8CSD);

			if (0 == (u8Response & 0x80))
			{
				// We can read the CSD!
			
				// See the CSD structure specification for more details.
				sg_u32SDSectorCount = ((uint32_t) (sg_u8CSD[7] & 0x3f)) << 16;
				sg_u32SDSectorCount |= (((uint32_t) sg_u8CSD[8]) << 8);
				sg_u32SDSectorCount |= sg_u8CSD[9];
				sg_u32SDSectorCount = (sg_u32SDSectorCount + 1) << 10;

				sg_u16BlockSize = (1 << (sg_u8CSD[5] & 0x0f));
			
				// Set SPI baud rate to (SD_SPEED_HIGH, for data transfers)
				(void) SPISetBaudRate(SD_SPEED_HIGH);
				
				bResult = true;
			}	
		}
	}
	else
	{
		// SDV1 Card. Fire off send OP condition command until it wakes up
		u8Response = SDSendOpConditionArg(u8Resp,
										  ACMD41_ARG_SDV2);
		if (0 == u8Response)
		{
			// Success! 
			bResult = true;
		}
		else
		{
			bResult = false;
		}
	}
	
errorExit:
if (bResult)
{bResult=true;}  //?
		
	return(bResult);
}

bool SDGetSectorCount(uint32_t *pu32SectorCount)
{
	// Fake it for now until we read the CID
	*pu32SectorCount = sg_u32SDSectorCount;
	return(true);
}

bool SDGetBlockSize(uint32_t *pu32BlockSize)
{
	*pu32BlockSize = sg_u16BlockSize;
	return(true);
}

// Receives a datablock during the data phase which may take some time
static bool SDReceiveDataBlock(uint8_t *pu8Buffer,
							   uint16_t u16RXCount)
{
	uint8_t u8Response;
	uint16_t u16Count = 4096;
	
	// Wait for response token
	do 
	{
		SPIRead(&u8Response,
				sizeof(u8Response));
	} 
	while ((u8Response != SD_START_TOKEN) &&
		   (u16Count--));
		   
	if (u8Response != SD_START_TOKEN)
	{
		return(false);
	}
	
	// Start of data. Pull it down.
	SPIRead(pu8Buffer,
			u16RXCount);
			
	// Discard CRC
	SPIRead(&u8Response,
			sizeof(u8Response));
	SPIRead(&u8Response,
			sizeof(u8Response));
	return(true);			   
}

// Transmits a datablock during the data phase which may take some time
static bool SDTransmitDataBlock(uint8_t *pu8Buffer,
							    uint16_t u16TXCount,
								uint8_t u8DataPhaseToken)
{
	uint8_t u8Response;
	uint16_t u16Attempts;
	bool bResult = true;

	// Send the data phase token (whatever it is)
	SPIWrite(&u8DataPhaseToken,
			 sizeof(u8DataPhaseToken));
	
	if (u8DataPhaseToken != SD_STOP_TRANSACTION)
	{
		// As long as we're not stopping a transaction, we're writing data
		SPIWrite(pu8Buffer,
				 u16TXCount);
				 
		// Wait for acceptance of the sector
		u16Attempts = 0;
		while (u16Attempts < SD_MAX_WRITE_ATTEMPTS)
		{
			// Get some data
			SPIRead(&u8Response,
					sizeof(u8Response));
			if (u8Response != 0xff)
			{
				break;
			}
				
			++u16Attempts;
		}
			
		// We've timed out or else the card is dead
		if (SD_MAX_WRITE_ATTEMPTS == u16Attempts)
		{
			bResult = false;
			goto errorExit;
		}
			
		// See if the data was accepted
		u8Response &= 0x1f;
		if (0x05 == u8Response)
		{
			// Wait for the write to finish
			u16Attempts = 0;
			while (u16Attempts < SD_MAX_WRITE_ATTEMPTS)
			{
				// Wait for the card to accept writing the sector
				SPIRead(&u8Response,
						sizeof(u8Response));
				if (u8Response)
				{
					break;
				}
					
				++u16Attempts;
			}
				
			if (SD_MAX_WRITE_ATTEMPTS == u16Attempts)
			{
				bResult = false;
				goto errorExit;
			}
			
			// Success!
		}
		else
		{
			// Sector not accepted
			bResult = false;
		}
	}
	
errorExit:
	return(bResult);
}

#define CMD17                   17
#define CMD17_CRC               0x00

#define CMD18					18
#define CMD12					12

// Read one or more sectors from SD
bool SDRead(uint32_t u32Sector,
			uint8_t *pu8Buffer,
			uint32_t u32SectorCount)
{
	bool bResult = false;
	
	// Pet the watchdog
	WatchdogReset();
		
	// Assert chip select
	SDSetCS(true);

	if (1 == u32SectorCount)
	{
		if (0 == SDCommand(CMD17,
						   u32Sector))
		{
			bResult = SDReceiveDataBlock(pu8Buffer,
										 sg_u16BlockSize);
		}
		else
		{
			// Failed
		}
	}
	else
	{
		// Multisector
		if (0 == SDCommand(CMD18,
						   u32Sector))
		{
			do 
			{
				bResult = SDReceiveDataBlock(pu8Buffer,
							 				 sg_u16BlockSize);
				// Pet the watchdog
				WatchdogReset();
				
				pu8Buffer += sg_u16BlockSize;
			} 
			while ((bResult) &&
				   (u32SectorCount--));
				   
			// Regardless, send a stop command
			(void) SDCommand(CMD12,
							 0);
		}
		else
		{
			// Failed
		}
	}
	
	SDSetCS(false);

	return(bResult);
}

#define CMD24               24
#define CMD25				25

// Write one or more sectors to SD
bool SDWrite(uint32_t u32Sector,
			 uint8_t *pu8Buffer,
			 uint32_t u32SectorCount)
{
	bool bResult = false;
	
	// Pet the watchdog
	WatchdogReset();
	
	// Assert chip select
	SDSetCS(true);

 	if (1 == u32SectorCount)
	{
		// Single sector
		if (0 == SDCommand(CMD24,
						   u32Sector))
		{
			bResult = SDTransmitDataBlock(pu8Buffer,
										  sg_u16BlockSize,
										  SD_START_TOKEN);
		}
		else
		{
			// Failed
		}
	}
	else
	{
		// Multisector
		if (0 == SDCommand(CMD25,
						   u32Sector))
		{
			do
			{
				bResult = SDTransmitDataBlock(pu8Buffer,
											  sg_u16BlockSize,
											  SD_START_MULTI_TOKEN);
											  
				// Pet the watchdog
				WatchdogReset();
				
				pu8Buffer += sg_u16BlockSize;
			}
			while ((bResult) &&
				   (u32SectorCount--));
			
			// Regardless, send a stop command
			(void) SDCommand(CMD12,
							 0);
		}
		else
		{
			// Failed
		}
	}
	
	SDSetCS(false);

	return(bResult);
}
