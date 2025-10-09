#ifndef _MAIN_H_
#define _MAIN_H_

#include "adc.h"

// Maximum number of cells that this module controller can handle
#define TOTAL_CELL_COUNT_MAX				(94)  // current modules are 94

// CPU speed (in hz) - internal clock
#define CPU_SPEED						8000000

// Timer prescaler - can be 1, 8, 64, 256, or 1024
#define TIMER_PRESCALER0				8
#define TIMER_PRESCALER1				256

// # Of clocks per second post prescaler. Integer arithmetic OK here since all
// divisors are powers of 2.
#define TIMER0_CLOCKS_PER_SECOND			((uint32_t)CPU_SPEED / (uint32_t)TIMER_PRESCALER0)
#define TIMER1_CLOCKS_PER_SECOND			((uint32_t)CPU_SPEED / (uint32_t)TIMER_PRESCALER1)

// This provides a 100ms periodic period. (8000000/256)=32150 ticks per interrupt, or 100ms
#define PERIODIC_COMPARE_A_RELOAD			(3125)
#define PERIODIC_INTERRUPT_RATE_MS			100
#define PERIODIC_INTERRUPT_MS_TO_TICKS(x)	(x / PERIODIC_INTERRUPT_RATE_MS)

// Callback rate (in milliseconds)
#define PERIODIC_CALLBACK_RATE_MS		300  //needs to be multiple of 100, per above.  300ms gives enough time for full string to be received (50us * 11 bits * 4 bytes = 2.2ms/cell)
#define PERIODIC_CALLBACK_RATE_TICKS	PERIODIC_INTERRUPT_MS_TO_TICKS(PERIODIC_CALLBACK_RATE_MS)

// Callback frames alternate between active read (where we get string data) and write (where we report and store it).

typedef enum
{
	EFRAMETYPE_READ,
	EFRAMETYPE_WRITE,
} EFrameType;



// Pack controller timeout (in milliseconds)
#define PACK_CONTROLLER_TIMEOUT_MS		11100
#define PACK_CONTROLLER_TIMEOUT_TICKS	PERIODIC_INTERRUPT_MS_TO_TICKS(PACK_CONTROLLER_TIMEOUT_MS)



// 0 - all off
// 1 - mech on, fet off
// 2 (this is a pack level state, means only one module turns on, pack controller will decide which.  modules will never see this state)
// 3 - both on
// * on loss of 5V go to state 0
// * on overcurrent detect go to state 1
//
// NOTE: DO NOT Change the order of these states as they are  tied to the CAN
// specification in addition to the transition code relying on them being in
// the order they're in.
typedef enum
{
	EMODSTATE_OFF = 0,
	EMODSTATE_STANDBY,
	EMODSTATE_PRECHARGE,
	EMODSTATE_ON,
	
	EMODSTATE_COUNT,
	
	// NOTE: This must come AFTER count, as it's a state that kicks off an
	// initial assessment of the system.
	EMODSTATE_INIT,
} EModuleControllerState;

extern EModuleControllerState ModuleGetState(void);

extern void Delay(uint32_t u32Microseconds);
extern uint8_t PlatformGetRegistrationID( void );
extern void ADCCallback(EADCType eType,
						uint16_t u16Reading);

// I2C Port
#define I2C_PORT			PORTD
#define I2C_PORT_READ		PIND
#define I2C_PORT_DDR		DDRD

// I2C Pins on that port
#define I2C_SDA_PIN			PORTD1
#define I2C_SCL_PIN			PORTD0

extern void PlatformAssert( const char* peFilename, const int s32LineNumber );
#define MBASSERT(x)			{if(false == (x)){PlatformAssert(__FILE__, __LINE__);}}
	
extern void DebugGPIOToggle( void );

extern uint16_t PlatformGetSendData( bool bUpdateBalanceStatus );
extern void WatchdogReset( void );
extern void SetSDBusy( bool bBusy );

extern void vUARTRXStart(void);
extern void vUARTRXEnd(void);
extern void vUARTRXData( uint8_t u8rxDataByte );
extern volatile bool g_bServiceNeeded;

extern void FrameInit(bool  bFullInit);  // call with true for full init (session), false for partial (frame)

#endif
