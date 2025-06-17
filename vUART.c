/* 
 * Module controller firmware - Virtual UART module
 *
 * This module contains the "virtual UART" functionality, implemented with a
 * combination of bit banging, interrupts, and timers. Naming conventions are
 * from the perspective of the module controller:
 *
 * MC RX (PB2) (upstream) - Receive cell status
 * MC TX (PB3) (downstream) - Send messages from the module controller 
 *
 * For each byte received, the format is as follows:
 *
 * 1 Start bit (always asserted)
 * 8 Data bits
 * 1 Stop bit (asserted=more coming, deasserted=nothing coming)
 * 1 Guard bit (always deasserted so there is transition time between bits and guaranteed transition to start)
 * 
 * The stop bit has a unique use, in that if it's a 1, it means more data is coming.
 * If it's a 0, there's no more data, leaving open the opportunity for the 
 * current controller to append more data.
 *
 * Timer0 is used for both upstream and downstream activity. Upstream is
 * handled by compare A, and downstream compare B. Timer0 is set to free run
 * and is NOT reset by a match to either channel.
 *
 * Reception of a byte is documented as follows:
 * 
 * 1) RX Line has a falling edge, causes a GPIO interrupt
 * 2) GPIO RX Pin is masked for interrupt operation
 * 3) Timer is set to TCNT0 + VUART_BIT_TICKS + (VUART_BIT_TICKS / 2) to set up
 * 4) When a timer/counter interrupt expires, sample RX pin
 * 5) Timer is set to TCNT0 + VUART_BIT_TICKS, repeat until all 8 bits are received
 * 6) On the 9th bit, sample the RX pin. If it's 1, more data is coming, go back to step
 *    1. If it's 0, continue on to step 7.
 * 7) Unmask RX Pin to wait for next byte start
 */

#include <stdint.h>
#include <xc.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "main.h"
#include "vUART.h"
#include "debugSerial.h"
#include "../Shared/Shared.h"

//#define PauseCAN 1  // comment out if not using

//The subtracted value for next bit time is empirically
// measured to ensure the per-bit time matches VUART_BIT_TICKS
// microseconds and accounts for CPU/interrupt/preamble overhead.

#define VUART_BIT_START_OFSET 9  // add to first bit, 7 has worked so far
#define VUART_BIT_TICK_OFFSET 7  // subtract per bit except first, 7 has worked best so far

// Uncomment to enable profiler code FOR TESTING ONLY, DO NOT LEAVE ON IN PRODUCTION
// NOTE: on REV E and up PC7 also drives the flipflop clock, it is normally high.  generally this will have no effect in normal operation
// only exception is if there is a latched overcurrent/overtemp condition in which case it may re-enable FET.
#define PROFILER_INIT()						DDRC |= (1 << PORTC7)
#define PROF_1_ASSERT()						PORTC |= (1 << PORTC7)
#define PROF_1_DEASSERT()					PORTC &= (uint8_t) ~(1 << PORTC7)
// uncomment to disable profiler code
//#define PROFILER_INIT()
//#define PROF_1_ASSERT()
//#define PROF_1_DEASSERT()

// If defined, will send a repeating pattern sequence to the cell CPUs
//#define CELL_CPU_PATTERN		1

typedef enum
{
	ESTATE_IDLE,				// Bus is completely idle and nothing is running
	ESTATE_RX_DATA,				// Bus is receiving data
	ESTATE_TX_DATA,				// Bus is transmitting data
	ESTATE_NEXT_BYTE			// Next byte
} EChannelState;

// call_up_tx related
static uint8_t sg_u8Cell_mc_rxBitCount;
static uint8_t sg_u8rxDataByte;
static EChannelState sg_eCell_mc_rxState = ESTATE_IDLE;
static volatile bool sg_bCell_mc_rxPriorState;
static volatile bool sg_bCell_mc_rxMoreData;

// cell_dn_tx related
static volatile uint8_t sg_u8txBitCount;
static volatile uint8_t sg_u8txDataByte;
static volatile bool sg_btxMoreAvailable;
static volatile EChannelState sg_etxState = ESTATE_IDLE;

// distinguishes whether we've sent a command or a reports request
// used by vuart to shorten transmission to first bit only to avoid stepping on rx timing

static volatile bool sg_bCellReportsReuested;


// And the next bit we want to transmit
static volatile bool sg_bMCTxNextBit;

static volatile uint8_t sg_u8SendIndex;						// Index to be sent next
static volatile uint8_t sg_u8SendData[2];					// Storage for value to be sent

static volatile uint8_t sg_u8SavedCANState;

#ifdef CELL_CPU_PATTERN
static uint8_t sg_u8PatternIndex;
static const uint16_t sg_u16Pattern[] =
{
	0x5555,
	0xaaaa,
	0x8089,
};
#endif

bool vUARTIsBusy(void) {
	return ((sg_etxState != ESTATE_IDLE) ||
		(sg_eCell_mc_rxState != ESTATE_IDLE));
}

static uint8_t vUARTtxDataGet( void )
{
	uint8_t u8Send;
	uint16_t u16Data;
	
	if (0 == sg_u8SendIndex)  //only get data if not another byte to send
	{

#ifdef CELL_CPU_PATTERN
		// Data is in big endian form
		sg_u8SendData[0] = (uint8_t) (sg_u16Pattern[sg_u8PatternIndex] >> 8);
		sg_u8SendData[1] = (uint8_t) sg_u16Pattern[sg_u8PatternIndex];
#else
		// Send request for sensors
		// It's big endian, so we need to swap
		u16Data = PlatformGetSendData(true);  //update balance status because we are sending the data
		sg_u8SendData[0] = (uint8_t) (u16Data >> 8);
		sg_u8SendData[1] = (uint8_t) u16Data;
#endif	
	
		if (sg_u8SendData[0] & 0x80)  //requesting cell reports
		{
			sg_bCellReportsReuested = true;
		}
		else
		{
			sg_bCellReportsReuested = false;  //sending a command
		}
	}

	u8Send = sg_u8SendData[sg_u8SendIndex];

	sg_u8SendIndex++;
	if( sg_u8SendIndex >= sizeof(sg_u8SendData) )
	{
		sg_u8SendIndex = 0;
#ifdef CELL_CPU_PATTERN
		++sg_u8PatternIndex;
		if (sg_u8PatternIndex >= (sizeof(sg_u16Pattern) / sizeof(sg_u16Pattern[0]))
		{
			sg_u8PatternIndex = 0;
		}
#endif
	}
	
	return( u8Send );
}

// Return true if there are more data bytes available.
//	This is checked before calling vUARTtxDataGet()
static bool vUARTtxDataAvailable( void )
{
	if( 0 == sg_u8SendIndex )
	{
		
		*(uint16_t*)sg_u8SendData = PlatformGetSendData(false);  //don't update balance status, only a query
	}
	
	if( sg_u8SendIndex <= (sizeof(sg_u8SendData)-2) )
	{
		return(true);
	}
	
	return(false);
}

// Called when we have a string timeout or need to reset the state machine
// for the MC RX side of things
void vUARTRXReset(void)
{
	sg_eCell_mc_rxState = ESTATE_IDLE;
	vUARTRXStart();

#ifdef PauseCAN
	CANGIE = sg_u8SavedCANState;  // re-enable CAN
#endif
}

// This starts an unsolicited transmission on tx. true Is returned if it was successfully
// started, but false if the tx vUART is active.



bool vUARTStarttx(void)
{
	bool bReturnCode = false;
	
	// Is our tx path busy? If so, we can't start
	if ((sg_etxState != ESTATE_IDLE) ||
		(sg_eCell_mc_rxState != ESTATE_IDLE))
	{
		// Can't start. Fall through
	}
	else
	{
		sg_etxState = ESTATE_TX_DATA;
		sg_u8SendIndex = 0;
		sg_u8txBitCount = 0;
		
		// Force assertion to cause a falling edge to happen
		sg_bMCTxNextBit = true;
		
		// We can start! Start at one bit's-worth of time later
		TIMER_CHA_INT(VUART_BIT_TICKS);
		bReturnCode = true;

		// Seed the data stream	
		sg_btxMoreAvailable = vUARTtxDataAvailable();
		sg_u8txDataByte = vUARTtxDataGet();
	}
	
	return(bReturnCode);
}

static bool sg_bState;

// Pin change interrupt - detecting start bit
ISR(INT1_vect, ISR_BLOCK)
{
	// Program up the timer to interrupt about 1 bit's worth, which
	// puts it almost in the center of the next bit. The added value 
	// is empirically measured to ensure the sample of the first
	// bit is in the middle of the bit time.
	
	// microseconds and accounts for CPU/interrupt/preamble overhead.
	TIMER_CHB_INT( VUART_BIT_TICKS + (VUART_BIT_START_OFSET));  
	
	// If RX is asserted then the start has occurred
//	if( IS_PIN_RX_ASSERTED() )
if (1)
	{
		
		// Stop rx pin change interrupts
		VUART_RX_DISABLE();
	
		if (sg_bState)
		{
			PROF_1_ASSERT();
			sg_bState = false;
		}
		else
		{
			PROF_1_DEASSERT();
			sg_bState = true;
		}
		
		// Set the state machine to receive data
		sg_eCell_mc_rxState = ESTATE_RX_DATA;
		sg_u8Cell_mc_rxBitCount = 0;
#ifdef PauseCAN		
	    // Save CAN state
	    sg_u8SavedCANState = CANGIE;
	    // Disable CAN interrupts during VUART reception
	    CANGIE &= ~(1 << ENIT);
#endif
	}
	else
	{
		// Just ignore it. Indicates a spurious interrupt or it's out
		// of sync
		TIMER_CHB_INT_DISABLE();
		sg_eCell_mc_rxState = ESTATE_IDLE;
	}
		
}



// Timer 0 compare B interrupt (bit clock) for mc rx from the cell CPUs
ISR(TIMER0_COMPB_vect, ISR_BLOCK)
{
	bool bData;
	
	// Set the timer to the next bit. The subtracted value is empirically
	// measured to ensure the per-bit time matches VUART_BIT_TICKS
	// microseconds and accounts for CPU/interrupt/preamble overhead.
	TIMER_CHB_INT(VUART_BIT_TICKS-VUART_BIT_TICK_OFFSET);  //different from bit start offset
	
	bData = sg_bCell_mc_rxPriorState;
	sg_bCell_mc_rxPriorState = IS_PIN_RX_ASSERTED();
	if (sg_bState)
	{
		PROF_1_ASSERT();
		sg_bState = false;
	}
	else
	{
		PROF_1_DEASSERT();
		sg_bState = true;	
	}
	sg_u8Cell_mc_rxBitCount++;
	
	if (1 == sg_u8Cell_mc_rxBitCount)
	{
		// Start bit. Zero the data byte
		sg_u8rxDataByte = 0;
		return;
	}
	// Handles cell_mc_rx
	if (sg_u8Cell_mc_rxBitCount < 10)  // actual data
	{
		sg_u8rxDataByte <<= 1;
		sg_u8rxDataByte |= bData;
	}
	else  
	{
		// This is the more data vs. data stop bit, 9th and last one.  we can ignore the guard bit that follows
		sg_bCell_mc_rxMoreData = bData;

		// Enable cell_mc_rx for next byte
		VUART_RX_ENABLE();
		// stop the timed bit interrupts
		TIMER_CHB_INT_DISABLE();
		
		// record the received byte
		vUARTRXData(sg_u8rxDataByte);

		// Flag that more data is coming, even if we don't get another one
		sg_eCell_mc_rxState = ESTATE_NEXT_BYTE;
	}
}




// Timer 0 compare A interrupt (bit clock) for mc tx to the cell CPUs
ISR(TIMER0_COMPA_vect, ISR_BLOCK)
{
	// Set the timer to the next bit
	TIMER_CHA_INT(VUART_BIT_TICKS-5);
	
	// Set the state of the output pin
	if (sg_bMCTxNextBit)
	{
		VUART_TX_ASSERT();
	}
	else
	{
		VUART_TX_DEASSERT();
	}
	
	// Preincrement the bit count 
	++sg_u8txBitCount;
	// see if we are requesting cell data, if yes jump to end
	if ((3 == sg_u8txBitCount) && (sg_bCellReportsReuested))  // after the request bits
	{
		// Stop bit
		sg_u8txBitCount = 11;
		sg_bMCTxNextBit = false;
		sg_btxMoreAvailable = false;
		
		return;		
	}
	else
	// Transmit start condition and prepare data byte
	if( sg_u8txBitCount < 10 )
	{
		// Transmit data! (msb first)
		if (sg_u8txDataByte & 0x80)
		{
			sg_bMCTxNextBit = true;
		}
		else
		{
			sg_bMCTxNextBit = false;
		}
			
		sg_u8txDataByte <<= 1;
		return;
	}
	// Transmit stop bit (stop or continue!)
	else 
	if (10 == sg_u8txBitCount)
	{
		sg_bMCTxNextBit = sg_btxMoreAvailable;
		return;
	}
	else 
	if (11 == sg_u8txBitCount) 
	{
		// Stop bit
		sg_bMCTxNextBit = false;
		return;
	}
	else
	if (12 == sg_u8txBitCount)
	{
		// Cause a falling edge interrupt (if there is one)
		sg_bMCTxNextBit = sg_btxMoreAvailable;
		
		// Already deasserted here
		sg_u8txBitCount = 0;
		
		// If more available, reset the bit count and exit
		if (false == sg_btxMoreAvailable)
		{
			sg_etxState = ESTATE_IDLE;
			TIMER_CHA_INT_DISABLE();
		}
		else
		{
			sg_btxMoreAvailable = vUARTtxDataAvailable();
			sg_u8txDataByte = vUARTtxDataGet();
			
			// Set the timer 2 bits later
			TIMER_CHA_INT(VUART_BIT_TICKS*4);
		}
	}

}

void vUARTInit(void)
{
	// Ensure pullups aren't globally disabled
	MCUCR &= (uint8_t) ~(1 << PUD);
	
	// Set up outputs
	DDRB |= (1 << PIN_TX);
	
	// Now inputs
	DDRB &= (uint8_t) ~(1 << PIN_RX);
	
	// Disable pullups on pin RX
	PORTB &= (uint8_t) ~(1 << PIN_RX);
	
	// Init profiler pins (if needed)
	PROFILER_INIT();
	
	// Mask out the pin change interrupts for now
	VUART_RX_DISABLE();
	
	// Set the TX state to idle, ready for business
	sg_etxState = ESTATE_IDLE;
	
	// Deassert the transmitter so levels are correct to start with
	VUART_TX_DEASSERT();
}

void vUARTInitReceive(void)
{

	// Enable receives
	VUART_RX_ENABLE();
}