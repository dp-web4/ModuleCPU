
/* 
 * Module controller firmware - DebugSerial
 *
 * This module contains the DebugSerial functionality. 
 *
 */

#include <stdint.h>
#include <xc.h>
#include <stdbool.h>
#include <avr/interrupt.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "debugSerial.h"

#define DEBUG_OUT_DISABLE			1

#ifndef DEBUG_OUT_DISABLE
#define DEBUG_SERIAL_BAUD			(9600)
#define DEBUG_SERIAL_SAMPLES		(32)
#define DEBUG_SERIAL_TX_BUFFER_LEN	(200)

#define DEBUG_OUT_MAX				(DEBUG_SERIAL_TX_BUFFER_LEN-1)

volatile static uint8_t sg_u8HeadIndex;
volatile static uint8_t sg_u8TailIndex;
volatile static uint8_t sg_u8OutBuffer[DEBUG_SERIAL_TX_BUFFER_LEN];	// Max 256 for u8 indexes
volatile static bool sg_bBusy;

static void DebugSerialHandleTX( void )
{
	uint8_t u8Tail = sg_u8TailIndex;
	
	MBASSERT( u8Tail < sizeof(sg_u8OutBuffer) );

	// If the indexes don't match, there's more to send
	if( u8Tail != sg_u8HeadIndex )
	{
		// Send a byte
		LINDAT = sg_u8OutBuffer[u8Tail];
		
		// Advance the tail and wrap if necessary
		u8Tail++;
		if( u8Tail >= sizeof(sg_u8OutBuffer) )
		{
			u8Tail = 0;
		}
		
		// Update the tail index
		sg_u8TailIndex = u8Tail;
	}
	// If no more bytes to send, just clear TXOK
	else
	{
		LINSIR |= (1 << LTXOK);
		sg_bBusy = false;
	}
}

// TX interrupt handler
ISR(LIN_TC_vect, ISR_BLOCK)
{
	// Clear the interrupt and then re-enable nested global
	LINSIR |= (1 << LTXOK);
	sei();
	
	DebugSerialHandleTX();
}

void DebugSerialSendSingle( uint8_t u8Data )
{
	uint8_t u8Tail = sg_u8TailIndex;
	uint8_t u8Head = sg_u8HeadIndex;
	
	MBASSERT( u8Tail < sizeof(sg_u8OutBuffer) );
	MBASSERT( u8Head < sizeof(sg_u8OutBuffer) );
	
	// Calculate next head to see if there's room in the buffer
	u8Head++;
	if( u8Head >= sizeof(sg_u8OutBuffer) )
	{
		u8Head = 0;
	}
	
	// If there's room, store the byte
	if( u8Tail != u8Head )
	{
		sg_u8OutBuffer[sg_u8HeadIndex] = u8Data;
		
		// Update the head index
		sg_u8HeadIndex = u8Head;
		
		// If not waiting for completion, start a new transmit
		if( false == sg_bBusy )
		{
			sg_bBusy = true;
			
			// Disable interrupts for TX
			LINENIR &= (uint8_t)~(1 << LENTXOK);
			
			DebugSerialHandleTX();
			
			// Enable interrupts for TX
			LINENIR |= (1 << LENTXOK);
		}
	}
	else
	{
		// Buffer is full!  Drop this byte
	}
}

void DebugSerialSend( uint8_t* pu8Data, uint16_t u16Length )
{
	while( u16Length )
	{
		// Convert LF to CRLF
		if( '\n' == *pu8Data )
		{
			DebugSerialSendSingle('\r');
		}
		
		DebugSerialSendSingle(*pu8Data);
		
		pu8Data++;
		u16Length--;
	}
}

#endif

void DebugOut( const char* peFormat, ... )
{
#ifndef DEBUG_OUT_DISABLE
	char u8Buffer[DEBUG_OUT_MAX];
	va_list args;
	
	// Null terminate the end of the buffer for safety
	u8Buffer[sizeof(u8Buffer)-1] = 0;
	
	va_start( args, peFormat );
	
	// Compose the output string, ok if it fills it up completely because it's already terminated
	vsnprintf( u8Buffer, sizeof(u8Buffer)-1, peFormat, args );
	
	va_end( args );	 */
	
	//DebugSerialSend( (uint8_t*)u8Buffer, strlen(u8Buffer) );
#endif
}

#ifndef DEBUG_OUT_DISABLE
void DebugSerialInit( void )
{
	// Set the serial clock
	// 17.5.6.1: LDIV[11..0] = (fclki/o / LBT[5..0] x BAUD) - 1
	//LINBTR = DEBUG_SERIAL_SAMPLES & 0x3f;  // Just use default of 32
	//LINBRR = (uint16_t)((uint32_t)CPU_SPEED / ((uint32_t)DEBUG_SERIAL_SAMPLES * (uint32_t)DEBUG_SERIAL_BAUD)) - 1;
	//LINBRR = (uint16_t)((uint32_t)509436 / ((uint32_t)DEBUG_SERIAL_SAMPLES * (uint32_t)DEBUG_SERIAL_BAUD)) - 1;
	
    // Baud rate 9600 (9433 determined experimentally)
    LINBTR = 9 | (1 << LDISR);
    LINBRR = 5;
	
	// Spin until the UART is not busy (shouldn't be at this point)
    while( LINSIR & (1 << LBUSY) );

	// Enable UART mode, TX only, and enable overall module
	LINCR = (1 << LENA) | (1 << LCMD2) | (0 << LCMD1) | (1 << LCMD0);
}
#endif