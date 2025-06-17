
/* 
 * Module controller firmware - RTC MCP7940N
 *
 * This module contains the functionality for the RTC interactions. 
 *
 */

#include <stdint.h>
#include <xc.h>
#include <stdbool.h>
#include <time.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include "main.h"
#include "rtc_mcp7940n.h"
#include "I2c.h"

// Anything below YEAR_ROLLOVER_CUTOFF will be interpreted as
// 21xx, and anything equal to or above will be 20xx.
#define	YEAR_ROLLOVER_CUTOFF	20

#define RTC_MAX_ADDRESS_TRIES	(3)
#define RTC_ADDRESS				(0xde)

#define REG_RTSEC				0x00

#define REG_RTCHOUR				0x02
#define HR2412					6

#define REG_RTCWKDAY			0x03
#define OSCRUN					5
#define VBATEN					3

#define REG_RTCC_CONTROL		0x07
#define SQWEN					6

// Structure that mirrors registers 0-6 in the RTC
typedef struct
{
	uint8_t u8Seconds;
	uint8_t u8Minutes;
	uint8_t u8Hours;		// 24hr clock
	uint8_t u8DOW;			// 1-7
	uint8_t u8Day;			// 1-31
	uint8_t u8Month;		// 1-12
	uint8_t u8Year;			// 0-99
} SMCP7940NTime;

// # Of seconds since January 1, 1970
static volatile uint64_t sg_u64Time;

// Called once per second on rev 1 and newer hardware
ISR(INT3_vect, ISR_NOBLOCK)
{
	EIFR |= (1 << INTF3);
	sg_u64Time++;
}

static bool RTCStartTransaction( uint8_t u8Address, 
								 bool bRead, 
								 bool bAllowUnstick )
{
	bool bResult = false;
	uint8_t u8Tries = RTC_MAX_ADDRESS_TRIES;
	
	while( (false == bResult) && u8Tries )
	{
		u8Tries--;
		
		bResult = I2CStartTransaction( u8Address, bRead );
	
		if( false == bAllowUnstick )
		{
			break;
		}
		
		// Unstick and retry on failure
		if( false == bResult )
		{
			I2CUnstick();
		}
	}
	
	return( bResult );
}

bool RTCReadRegisters( uint8_t u8RegisterAddr, 
					   uint8_t* pu8Data, 
					   uint8_t u8Length )
{
	bool bResult;
	
	MBASSERT( u8Length >= 1 );
	
	// First send device address and write register address
	bResult = RTCStartTransaction( RTC_ADDRESS, false, true );
	
	// Couldn't get an ACK?  Give up now
	if( false == bResult )
	{
		goto errorExit;
	}
	
	// Send the register address
	bResult = I2CTxByte( u8RegisterAddr );
	
	if( false == bResult )
	{
		goto errorExit;
	}
	
	// Send repeated start with read
	bResult = RTCStartTransaction( RTC_ADDRESS, true, false );
	
	// Couldn't get an ACK?  Give up now
	if( false == bResult )
	{
		goto errorExit;
	}
	
	while( u8Length >= 2 )
	{
		*pu8Data = I2CRxByte( true );
		pu8Data++;
		u8Length--;
	}
	
	// Final read with NACK
	*pu8Data = I2CRxByte( false );
	
errorExit:

	I2CStop();
	
	return(bResult);
}

bool RTCWriteRegisters( uint8_t u8RegisterAddr,
						uint8_t* pu8Data, 
						uint8_t u8Length )
{
	bool bResult;
	
	MBASSERT( u8Length >= 1 );
	
	// First send device address and write register address
	bResult = RTCStartTransaction( RTC_ADDRESS, false, true );
	
	// Couldn't get an ACK?  Give up now
	if( false == bResult )
	{
		goto errorExit;
	}
	
	// Send the register address
	bResult = I2CTxByte( u8RegisterAddr );
	
	if( false == bResult )
	{
		goto errorExit;
	}
	
	// Now immediately send the sequential data
	while( u8Length )
	{
		bResult = I2CTxByte( *pu8Data );
		
		if( false == bResult )
		{
			goto errorExit;
		}
		
		pu8Data++;
		u8Length--;
	}
	
errorExit:

	I2CStop();
	
	return( bResult );
}

// Get the current time from the hardware. This converts the 
// SMCP7940NTime structure in place from BCD to binary
static bool RTCReadHW( SMCP7940NTime* psTime )
{
	bool bResult;
	uint8_t* pu8Read = (uint8_t*)psTime;
	uint8_t u8Byte;
	
	// Read the batch of sequential registers
	bResult = RTCReadRegisters( REG_RTSEC, pu8Read, sizeof(*psTime) );
	
	if( false == bResult )
	{
		goto errorExit;
	}
	
	// Mask out the irrelevant data and convert from BCD
	u8Byte = psTime->u8Seconds;
	psTime->u8Seconds = (((u8Byte >> 4) & 0x07) * 10) + (u8Byte & 0x0f);

	u8Byte = psTime->u8Minutes;
	psTime->u8Minutes = (((u8Byte >> 4) & 0x07) * 10) + (u8Byte & 0x0f);

	u8Byte = psTime->u8Hours;
	if( u8Byte & 0x40 )
	{
		// 12 hr clock
		psTime->u8Hours = (((u8Byte >> 4) & 0x01) * 10) + (u8Byte & 0x0f);
		
		if( u8Byte & 0x20 )
		{
			// PM so add 12 hrs
			psTime->u8Hours += 12;
		}
	}
	else
	{
		// 24 hr clock
		psTime->u8Hours = (((u8Byte >> 4) & 0x03) * 10) + (u8Byte & 0x0f);
	}

	u8Byte = psTime->u8DOW;
	psTime->u8DOW = (u8Byte & 0x07);

	u8Byte = psTime->u8Day;
	psTime->u8Day = (((u8Byte >> 4) & 0x03) * 10) + (u8Byte & 0x0f);

	u8Byte = psTime->u8Month;
	psTime->u8Month = (((u8Byte >> 4) & 0x01) * 10) + (u8Byte & 0x0f);

	u8Byte = psTime->u8Year;
	psTime->u8Year = (((u8Byte >> 4) & 0x0f) * 10) + (u8Byte & 0x0f);
	
	psTime->u8Year += (2000 - 1980);

errorExit:
	return( bResult );
}

// Set the current time to the hardware. This converts the
// SMCP7940NTime structure in place from binary to BCD
static bool RTCWriteHW( SMCP7940NTime* psTime )
{
	bool bResult;
	uint8_t* pu8Read = (uint8_t*)psTime;
	uint8_t u8Byte;
	
	// Seconds
	u8Byte = ((psTime->u8Seconds / 10) << 4) | (psTime->u8Seconds % 10);
	psTime->u8Seconds = u8Byte;
	
	// Minutes
	u8Byte = ((psTime->u8Minutes / 10) << 4) | (psTime->u8Minutes % 10);
	psTime->u8Minutes = u8Byte;

	// Hours
	u8Byte = ((psTime->u8Hours / 10) << 4) | (psTime->u8Hours % 10);
	u8Byte &= (uint8_t) ~(1 << HR2412);	// Force to 24 hour
	psTime->u8Hours = u8Byte;
	
	// Day of week - only significant bits
	psTime->u8DOW &= 0x07;
	psTime->u8DOW |= (1 << VBATEN);
	
	// Day of the month
	u8Byte = ((psTime->u8Day / 10) << 4) | (psTime->u8Day % 10);
	psTime->u8Day = u8Byte;
	
	// Month
	u8Byte = ((psTime->u8Month / 10) << 4) | (psTime->u8Month % 10);
	psTime->u8Month = u8Byte;
	
	// Year
	u8Byte = ((psTime->u8Year / 10) << 4) | (psTime->u8Year % 10);
	psTime->u8Year = u8Byte;

	// Write everything!
	bResult = RTCWriteRegisters( REG_RTSEC, pu8Read, sizeof(*psTime) );

	return( bResult );
}

// Returns true if u16Year is a leap year
static bool IsLeapYear(uint16_t u16Year)
{
	bool bLeapYear = false;
	
	if ((u16Year % 400) == 0)
	{
		bLeapYear = true;
	}
	else
	if ((u16Year % 100) == 0)
	{
		// Not a leap year if divisible by 100 but not by 400
	}
	else
	if ((u16Year % 4) == 0)
	{
		bLeapYear = true;
	}
	
	return(bLeapYear);
}

// Converts an SMCP7940NTime structure to struct tm
static void HWToStructTM(SMCP7940NTime *psTime,
						 struct tm *psTimeTM)
{
	// At this point, the time is in STime and we should
	// turn that in to time_t
	memset((void *) psTimeTM, 0, sizeof(*psTimeTM));
	
	psTimeTM->tm_sec = psTime->u8Seconds;
	psTimeTM->tm_min = psTime->u8Minutes;
	psTimeTM->tm_hour = psTime->u8Hours;
	psTimeTM->tm_mday = psTime->u8Day;
	psTimeTM->tm_mon = psTime->u8Month - 1;
	
	if (psTime->u8Year < YEAR_ROLLOVER_CUTOFF)
	{
		psTimeTM->tm_year = 2100 + psTime->u8Year;
	}
	else
	{
		psTimeTM->tm_year = 2000 + psTime->u8Year;
	}	
	
	psTimeTM->tm_year -= 1900;
}

// Converts struct tm to SMCP7940NTime structure
static void StructTMToHW(struct tm *psTimeTM,
						 SMCP7940NTime *psTime)
{
	memset((void *) psTime, 0, sizeof(*psTime));
	
	psTime->u8Seconds = psTimeTM->tm_sec;
	psTime->u8Minutes = psTimeTM->tm_min;
	psTime->u8Hours = psTimeTM->tm_hour;
	psTime->u8Day = psTimeTM->tm_mday;
	psTime->u8Month = psTimeTM->tm_mon;
	
	if (psTimeTM->tm_year > (2099-1900))
	{
		psTime->u8Year = psTimeTM->tm_year - 2100;
	}
	else
	{
		psTime->u8Year = psTimeTM->tm_year - 2000;
	}
	
}

bool RTCRead( struct tm * psTime )
{
	uint64_t u64Time;
	struct tm *psSrc;
	
	// Read our 64 bit time_t
	cli();
	u64Time = sg_u64Time;
	sei();
	
	psSrc = gmtime((const time_t *) &u64Time);
	memcpy((void *) psTime, (void *) psSrc, sizeof(*psTime));
	return(true);
}

// This sets the RTC to the specific time passed in. True is returned
// if it's 
bool RTCSetTime(uint64_t u64Timet)
{
	struct tm sTime;
	struct tm *psTime;
	SMCP7940NTime sTimeHW;

	// Convert to GMTime
	memset((void *) &sTime, 0, sizeof(sTime));
	psTime = gmtime((const time_t *) &u64Timet);
	MBASSERT(psTime);
	memcpy((void *) &sTime, (void *) psTime, sizeof(sTime));
	
	// Now convert to hardware
	StructTMToHW(&sTime,
				 &sTimeHW);
	
	// Set the time			 
	cli();
	sg_u64Time = u64Timet;
	sei();
	
	// Now go tell the hardware about it
	return(RTCWriteHW(&sTimeHW));
}

static const uint8_t sg_u8DaysInMonths[] =
{
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

// PLL Should EASILY come alive within 2 seconds
#define RTC_PLL_RETRY_COUNT		200

bool RTCInit( void )
{
	bool bResult;
	uint8_t u8Data;
	SMCP7940NTime sTime;
	struct tm sTimeTm;
	bool bSetDefaultTime = false;
	uint64_t u64TempTime;
	uint8_t u8Retry;

	// Disable external oscillator bit EXTOSC (3) in control register (0x07),
	u8Data = 0;
	bResult = RTCWriteRegisters( REG_RTCC_CONTROL, &u8Data, sizeof(u8Data) );
	if( false == bResult )
	{
		goto errorExit;
	}
	
	// Set start oscillator bit ST (7) in RTCSEC register (0x00)
	u8Data = (1 << 7);
	bResult = RTCWriteRegisters( REG_RTSEC, &u8Data, sizeof(u8Data) );
	if( false == bResult )
	{
		goto errorExit;
	}
	
	// Wait until oscillator run bit OSCRUN (5) in RTCWKDAY register (0x03) is set
	u8Retry = 0;
	while( u8Retry < RTC_PLL_RETRY_COUNT)
	{
		u8Data = 0;
		bResult = RTCReadRegisters( REG_RTCWKDAY, &u8Data, sizeof(u8Data) );
		if( false == bResult )
		{
			goto errorExit;
		}

		if( u8Data & (1 << OSCRUN) )
		{
			break;
		}
		
		Delay(10*000);
		u8Retry++;
	}
	
	// If we didn't hear from the PLL within RTC_PLL_RETRY_COUNT passes,
	// fail the call.
	if (u8Retry >= RTC_PLL_RETRY_COUNT)
	{
		goto errorExit;
	}
	
	// Enable VBATEN (VBAT). Note - u8Data is assumed to be the existing
	// value of REG_RTCWKDAY so the week day isn't changed
	u8Data |= (1 << VBATEN);
	bResult = RTCWriteRegisters( REG_RTCWKDAY, &u8Data, sizeof(u8Data) );
	if( false == bResult )
	{
		goto errorExit;
	}
	
	// Ensure we're in 24 hour mode
	bResult = RTCReadRegisters( REG_RTCHOUR, &u8Data, sizeof(u8Data) );
	if( false == bResult )
	{
		goto errorExit;
	}
	
	// Mask out the 24/12 hour selection bit to be 24 hour
	u8Data &= (uint8_t) ~(1 << HR2412);
	bResult = RTCWriteRegisters( REG_RTCHOUR, &u8Data, sizeof(u8Data) );
	if( false == bResult )
	{
		goto errorExit;
	}
	
	// Read out the current time/date. If anything is unreasonable, then set a default time
	bResult = RTCReadHW(&sTime);
	if( false == bResult )
	{
		goto errorExit;
	}
	
	if (sTime.u8Seconds > 59)
	{
		bSetDefaultTime = true;
	}
	if (sTime.u8Minutes > 59)
	{
		bSetDefaultTime = true;
	}
	if (sTime.u8Hours > 59)
	{
		bSetDefaultTime = true;
	}
	if ((sTime.u8DOW < 1) ||
	    (sTime.u8DOW > 7))
	{
		bSetDefaultTime = true;
	}
	if ((sTime.u8Month < 1) ||
		(sTime.u8Month > 12))
	{
		bSetDefaultTime = true;
	}
	else
	if (sTime.u8Day < 1)
	{
		bSetDefaultTime = true;
	}
	else
	{
		uint8_t u8Days = sg_u8DaysInMonths[sTime.u8Month - 1];
		
		if (2 == sTime.u8Month)
		{
			uint16_t u16Year = sTime.u8Year;
		
			if (u16Year < YEAR_ROLLOVER_CUTOFF)
			{
				u16Year += 2100;
			}
			else
			{
				u16Year += 2000;
			}
			
			// If this is a leap year, then add another day
			// for February
			if (IsLeapYear(u16Year))
			{
				u8Days++;
			}
		}

		// If this day exceeds # of days in this month, set defaults
		if (sTime.u8Day > u8Days)
		{
			bSetDefaultTime = true;
		}		
	}
	
	// If we're setting the default time, do so!
	if (bSetDefaultTime)
	{
		// Midnight, January 1st, 2024 is the base
		sTime.u8Seconds = 0;
		sTime.u8Minutes = 0;
		sTime.u8Hours = 0;
		sTime.u8Day = 1;
		sTime.u8Month = 1;
		sTime.u8Year = 24;
		
		bResult = RTCWriteHW(&sTime);
		if( false == bResult )
		{
			goto errorExit;
		}
		
		// Now go read it out
		bResult = RTCReadHW(&sTime);
		if( false == bResult )
		{
			goto errorExit;
		}
	}

	// Convert the HW structure to struct tm
	HWToStructTM(&sTime,
				 &sTimeTm);

	u64TempTime = mktime(&sTimeTm);	

	cli();
	sg_u64Time = u64TempTime;
	sei();
	
	// Init the INT3 GPIO (PC0) - input + pullup. MFP Pin is open drain
	// according to MCP7940N documentation (page 1)
	DDRC &= (uint8_t) ~(1 << DDC0);
	PORTC |= (uint8_t) (1 << PORTC0);

	// 1Hz timer/counter is on !INT3/PC0, setting falling edge interrupt
//	EICRA = (1 << ISC31); EIFR |= (1 << INTF3); EIMSK |= (1 << INT3);

	// Read RTCC so we can enable SQWEN - 1hz square wave output
	bResult = RTCReadRegisters( REG_RTCC_CONTROL, &u8Data, sizeof(u8Data) );
	if( false == bResult )
	{
		goto errorExit;
	}

	u8Data |= (1 << SQWEN);
	bResult = RTCWriteRegisters( REG_RTCC_CONTROL, &u8Data, sizeof(u8Data) );
	if( false == bResult )
	{
		goto errorExit;
	}

	bResult = true;
	
errorExit:
	return(bResult);
}
