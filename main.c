/* ModuleCPU
 *
 * Module controller firmware - main module
 *
 * (C) Copyright 2023-2024 Modular Battery Technologies, Inc.
 * US Patents 11,380,942; 11,469,470; 11,575,270; others. All
 * rights reserved
 */

#include <stdint.h>
#include <xc.h>
#include <stdbool.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include "main.h"
#include "../Shared/Shared.h"
//#include "FatFS/source/ff.h"
#include "vUART.h"
#include "debugSerial.h"
#include "rtc_mcp7940n.h"
#include "can.h"
#include "STORE.h"
//#include "storage.h"
//#include "File.h"
#include "EEPROM.h"
#include "SD.h"

// watchdog stuff
// Uncomment to enable watchdog timer
//#define WDT_ENABLE
#define WDT_LEASH_LONG WDTO_2S  // used in normal operation, should be at least 1S
#define WDT_LEASH_SHORT WDTO_15MS  // used for high risk ops like switching relays, which often causes journey into the weeds

typedef enum {
EWDT_NORMAL,
EWDT_MECH_RLY_ON,
EWDT_MECH_RLY_OFF,
EWDT_FET_ON,
EWDT_FET_OFF,
} eWDTstatus;

static volatile eWDTstatus __attribute__((section(".noinit"))) sg_eWDTCurrentStatus;  // used to track if we're using WDT to recover from a relay switch glitch, .noinit to survive post-reset init


// Sets the default state of the module
#define STATE_DEFAULT				EMODSTATE_OFF
#define REPORT_MOST_AND_FEWEST_CELLS 1

// Uncomment to automatically cycle through all the power states
//#define	STATE_CYCLE								// if REQUEST_CELL_BALANCE_ENABLE is defined, will test discharge in OFF and STDBY
#define	STATE_CYCLE_INTERVAL		1			// How often do we switch state? (in seconds)

// Request ALL cell detail
#define CELL_DETAIL_ALL						0xff// Uncomment to cause cell CPUs to return fixed patterns (communication test)
// #define REQUEST_DEBUG_CELL_RESPONSE		5

// Uncomment to enable cell balance requests
//#define REQUEST_CELL_BALANCE_ENABLE
#define CELL_BALANCE_DISCHARGE_THRESHOLD 0x0387  // lowest target voltage for balancing, 0x387 is 3.9V
#define BALANCE_VOLTAGE_THRESHOLD 0x40   // difference between highest and lowest cell that triggers balancing

// Indication of temperature reading being invalid
#define TEMPERATURE_INVALID					0xffff

#define FW_BUILD_NUMBER						(8278)

#define MANUFACTURE_ID						(0x02)
#define PART_ID								(0x03)

// 16 Bit hardware compatibility value for module hardware compatibility command
#define HARDWARE_COMPATIBILITY				0x0000

// Value returned when cell temperature is invalid
#define CELL_TEMPERATURE_INVALID			0x7fff

// # Of valid data bits for cell voltage
#define CELL_VOLTAGE_BITS		10			// # Of valid bits in the cell voltage
#define CELL_DIV_TOP			90900		// Resistance of cell plus->voltage resistor on cell CPU
#define CELL_DIV_BOTTOM			30100		// Resistance of ground->voltage resistor on cell CPU
#define CELL_VOLTAGE_SCALE		(((float) CELL_DIV_BOTTOM) / ((float) CELL_DIV_TOP + (float) CELL_DIV_BOTTOM))

#define CELL_VOLTAGE_CAL 1.032 //measured calibration factor

#define FIXED_POINT_SCALE 512  // 2^9, gives us 9 bits of fractional precision  (10 bits overflows)

// Pre-calculate these constants:
#define VOLTAGE_CONVERSION_FACTOR ((uint32_t)((CELL_VREF * 1000.0 / CELL_VOLTAGE_SCALE * CELL_VOLTAGE_CAL * FIXED_POINT_SCALE) + 0.5))
#define ADC_MAX_VALUE (1 << CELL_VOLTAGE_BITS)

#define ADC_CURRENT_BUFFER_SIZE 8

static int16_t sg_sCurrenBuffer [ADC_CURRENT_BUFFER_SIZE];
static uint8_t sg_u8CurrentBufferIndex;

// Cell VREF
#define CELL_VREF				1.1

// Cell string "off to on" time in milliseconds
#define	CELL_POWER_OFF_TO_ON_MS			100

// Cell string power "on to first message" time in milliseconds
#define	CELL_POWER_ON_TO_FIRST_MSG_MS	5

// Charge current defaults when EEPROM not programmed
#define		MAX_DISCHARGE_AMPS			-42
#define		MAX_CHARGE_AMPS				10

// *****
// FAKE DATA ENABLES
//
// Uncomment to enable and set appropriate values

// Uncomment to inject fake cell data
// #define FAKE_CELL_DATA				1

// Prescaler value of 0: (8Mhz internal osc / 1= 8Mhz)
#define SYSCLOCK_PRESCALE					0

// Converts the prescaler 0 value into CS1[2:0] bits
#if 1==TIMER_PRESCALER0
#define TIMER_PRESCALER_CS0		(1 << CS00)
#elif 8==TIMER_PRESCALER0
#define TIMER_PRESCALER_CS0		(1 << CS01)
#elif 64==TIMER_PRESCALER0
#define TIMER_PRESCALER_CS0		((1 << CS01) | (1 << CS00))
#elif 256==TIMER_PRESCALER0
#define TIMER_PRESCALER_CS0		(1 << CS02)
#elif 1024==TIMER_PRESCALER0
#define TIMER_PRESCALER_CS0		((1 << CS02) | (1 << CS00))
#else
#error "Unknown timer prescaler 0 value - needs to be 1, 8, 64, 256, or 1024"
#endif

// Converts the prescaler 1 value into CS1[2:0] bits
#if 1==TIMER_PRESCALER1
#define TIMER_PRESCALER_CS1		(1 << CS10)
#elif 8==TIMER_PRESCALER1
#define TIMER_PRESCALER_CS1		(1 << CS11)
#elif 64==TIMER_PRESCALER1
#define TIMER_PRESCALER_CS1		((1 << CS11) | (1 << CS10))
#elif 256==TIMER_PRESCALER1
#define TIMER_PRESCALER_CS1		(1 << CS12)
#elif 1024==TIMER_PRESCALER1
#define TIMER_PRESCALER_CS1		((1 << CS12) | (1 << CS10))
#else
#error "Unknown timer prescaler 1 value - needs to be 1, 8, 64, 256, or 1024"
#endif

// Clear current interrupt flag, set the next compare, and enable the interrupt
#define TIMER1_CHA_INT(x)					TIFR1 = (1 << OCF1A); OCR1A = (uint16_t) (TCNT1 + (x)); TIMSK1 |= (1 << OCIE1A)

#define PIN_RELAY_EN						(PORTE1)  //moved from c0!
#define PIN_OCF_N							(PORTC1)  // when FET is on configure as input to monitor OCF, value is inverted starting 
#define PIN_FET_EN							(PORTC4)  // set high to enable clocking of FET, set low to asynchronously disable FET
#define PIN_FET_CK							(PORTC7)  // cycle low then high (no delay needed) to clock FET to ON.  if FET_EN is low this will have no effect.

// 5V Detect port definitions
#define PORT_5V_DET							PORTD
#define DDR_5V_DET							DDRD
#define PIN_5V_DET							(PORTD6)
#define IMASK_5V_DET						PCMSK2
#define PCINT_5V_DET						PCINT22
#define PCIFR_5V_DET						PCIF2

#define PIN_CELL_POWER						(PORTB4)
#define DDR_CELL_POWER						DDRB
#define PORT_CELL_POWER						PORTB

#define RELAY_EN_CONFIGURE()				DDRE |= ((uint8_t) (1 << PIN_RELAY_EN))   //moved from c0!
#define RELAY_EN_ASSERT()					PORTE |= ((uint8_t) (1 << PIN_RELAY_EN))   //moved from c0!
#define RELAY_EN_DEASSERT()					PORTE &= ((uint8_t) ~(1 << PIN_RELAY_EN))   //moved from c0!
#define RELAY_ASSERTED()					(PINE & ((uint8_t) (1 << PIN_RELAY_EN)))

// prior to rev E.  
//#define FET_EN_CONFIGURE()					PORTC &= ~  (((uint8_t) (1 << PIN_FET_EN)) | ((uint8_t) (1 << PIN_OCF_N))); DDRC |= (((uint8_t) (1 << PIN_FET_EN)) | ((uint8_t) (1 << PIN_OCF_N)) ); // set both as outputs and OFF
//#define FET_EN_ASSERT()						DDRC &= ~ ((uint8_t) (1 << PIN_OCF_N));  PORTC |= ((uint8_t) (1 << PIN_FET_EN));  // set to read OCF and drive FET_EN
//#define FET_EN_DEASSERT()					PORTC &= ~  (((uint8_t) (1 << PIN_FET_EN)) | ((uint8_t) (1 << PIN_OCF_N))); DDRC |= (((uint8_t) (1 << PIN_FET_EN)) | ((uint8_t) (1 << PIN_OCF_N)) ); // set both as outputs and OFF
//#define FET_ASSERTED()						(PINC & ((uint8_t) (1 << PIN_FET_EN)))

// starting with rev E
// initial configuration  PC4 low (external pulldown), PC7 high (it is also pulled up externally), PC1 input
// to enable FET to be turned on set PC4 high.  this removes the asynchronous /CLR from the flipflop.  note that an over temp or over current condition will override this
// to turn FET on once it is enabled, cycle the clock by driving PC7 low then high.  no need for delay inbetween.
// to turn FET off when it's on, drive PC4 low.  this will asynchronously clear the flipflop
// read the status of FET EN on PC1 (inverted!)
// an overtemp or overcurrent will drive /CLR and cause PC1 to go high, so a rising edge interrupt can be used to detect these events

#define FET_EN_CONFIGURE()					PORTC &= ~  ((uint8_t) (1 << PIN_FET_EN)); DDRC |= ((uint8_t) (1 << PIN_FET_EN)); DDRC &= ~ ((uint8_t) (1 << PIN_OCF_N)); DDRC |= ((uint8_t) (1 << PIN_FET_CK)) // set FET_EN as output and low, OCF_N as input, FET_CK as output
#define FET_EN_ASSERT()						PORTC |= ((uint8_t) (1 << PIN_FET_EN)); PORTC &= ~ ((uint8_t) (1 << PIN_FET_CK)); PORTC |= ((uint8_t) (1 << PIN_FET_CK))  // set  FET_EN high, then drive flipflop clock low then high
#define FET_EN_DEASSERT()					PORTC &= ~  ((uint8_t) (1 << PIN_FET_EN)) // asynchronously clear the flipflop
#define FET_ASSERTED()						~(PINC & ((uint8_t) (1 << PIN_OCF_N)))  // pin is inverted

#define CELL_POWER_ASSERT()					PORT_CELL_POWER |= ((uint8_t) (1 << PIN_CELL_POWER))
#define CELL_POWER_DEASSERT()				PORT_CELL_POWER &= ((uint8_t) ~(1 << PIN_CELL_POWER))


// the following variables need to survive WDT reset. initialize in regular reset init but not on wdt
//---------------------------------------------------------------------------------------------------
static uint8_t __attribute__((section(".noinit"))) sg_u8ModuleRegistrationID;
// State of charge and health
volatile static uint8_t __attribute__((section(".noinit"))) sg_u8SOC;
volatile static uint8_t __attribute__((section(".noinit"))) sg_u8SOH;
volatile static uint8_t __attribute__((section(".noinit"))) sg_u8CellStatusTarget;
volatile static uint8_t __attribute__((section(".noinit"))) sg_u8CellStatus;


volatile static bool __attribute__((section(".noinit"))) sg_bSendAnnouncement;			// true If we're sending a module announcement to the pack controller
volatile static bool __attribute__((section(".noinit"))) sg_bModuleRegistered;			// true If we've received a registration ID from the pack controller
volatile static bool sg_bAnnouncementPending = false;	// true if we need to send announcement after delay
volatile static uint8_t sg_u8AnnouncementDelayTicks = 0;	// ticks remaining before sending announcement

volatile static bool __attribute__((section(".noinit"))) sg_bSendTimeRequest;			// true If we want to send a "set time" command to the pack controller
volatile static bool __attribute__((section(".noinit"))) sg_bPackControllerTimeout;		// true If we've not heard from a pack controller in PACK_CONTROLLER_TIMEOUT_MS

// Cached unique ID to avoid repeated EEPROM reads during message formation
// Module unique ID now stored directly in sg_sFrame.moduleUniqueId

volatile static bool __attribute__((section(".noinit"))) sg_bSendModuleControllerStatus;
volatile static bool __attribute__((section(".noinit"))) sg_bSendCellStatus;
volatile static bool __attribute__((section(".noinit"))) sg_bSendHardwareDetail;
volatile static bool __attribute__((section(".noinit"))) sg_bSendCellCommStatus;
static bool sg_bIgnoreStatusRequests = false;  // Ignore new requests while sending
volatile static bool __attribute__((section(".noinit"))) sg_bCellBalanceReady;
volatile static bool __attribute__((section(".noinit"))) sg_bCellBalancedOnce;
volatile static bool __attribute__((section(".noinit"))) sg_bStopDischarge;
static volatile bool __attribute__((section(".noinit"))) sg_bOvercurrentSignal;
static volatile bool __attribute__((section(".noinit"))) sg_bADCUpdate;
static volatile uint16_t __attribute__((section(".noinit"))) sg_u16LowCellVoltageRAW; //  used for setting balance discharge target

volatile static FrameData __attribute__((section(".noinit"))) sg_sFrame;  //current frame data to be written to SD card
volatile static EFrameType __attribute__((section(".noinit"))) sg_eFrameStatus;  // alternating between EFRAMETYPE_READ, EFRAMETYPE_WRITE, at PERIODIC_CALLBACK_RATE_MS - we read a frame, then we write/send it
volatile static bool __attribute__((section(".noinit"))) sg_bNewTick;  // set tru in periodic tick isr, cleared in main loop

// # Of sequential cell count expected vs. received messages.
volatile static uint8_t __attribute__((section(".noinit"))) sg_u8SequentailCountMismatchThreshold;

// Incremented each time there's a cell count mismatch from what we get vs. what
// we expect
volatile static uint8_t __attribute__((section(".noinit"))) sg_u8SequentailCellCountMismatches;

// Status
// 0=Charge prohibited/discharge prohibited (always the case in any state by ON)
// 1=Charge allowed/discharge prohibited
// 2=Charge allowed/discharge allowed
// 3=Charge prohibited/discharge allowed

// SD card status

static volatile bool __attribute__((section(".noinit"))) sg_bSDCardReady;

typedef enum
{
	EMODESTATUS_CHARGE_PROHIBITED_DISCHARGE_PROHIBITED=0,
	EMODESTATUS_CHARGE_ALLOWED_DISCHARGE_PROHIBITED=1,
	EMODESTATUS_CHARGE_ALLOWED_DISCHARGE_ALLOWED=2,
	EMODESTATUS_CHARGE_PROHIBITED_DISCHARGE_ALLOWED=3
} EModuleControllerStatus;



// Module controller's status, for later expansion
static EModuleControllerStatus sg_eModuleControllerStatus = EMODESTATUS_CHARGE_PROHIBITED_DISCHARGE_PROHIBITED;

#ifdef STATE_CYCLE
volatile static uint8_t __attribute__((section(".noinit"))) sg_u8StateCounter;
volatile static EModuleControllerState __attribute__((section(".noinit"))) sg_eStateCycle;
#endif

volatile static EModuleControllerState __attribute__((section(".noinit")))sg_eModuleControllerStateCurrent;
volatile static EModuleControllerState __attribute__((section(".noinit")))sg_eModuleControllerStateTarget;
volatile static EModuleControllerState __attribute__((section(".noinit")))sg_eModuleControllerStateMax;

// Counter incremented every time the periodic timer fires (once every PERIODIC_INTERUPT_RATE_MS ms)
static volatile uint8_t sg_u8CellFrameTimer;
static volatile bool sg_bFrameStart;
static volatile uint8_t sg_u8CellStringPowerTimer;
static volatile uint8_t sg_u8TicksSinceLastPackControllerMessage;

// Delay for at least u32Microseconds-worth of timer 0 ticks. 
void Delay(uint32_t u32Microseconds)
{
	uint8_t u8Sample;
	
	u32Microseconds++;
	u32Microseconds >>= 1;
	
	u8Sample = TCNT0;
	while (u32Microseconds)
	{
		uint8_t u8SampleOld;
		
		u8SampleOld = u8Sample;
		
		// Wait until we quantize
		while (u8Sample == TCNT0);
		u8Sample = TCNT0;
		
		// Compute the time delta
		u8SampleOld = u8Sample - u8SampleOld;
		
		if (u8SampleOld > u32Microseconds)
		{
			u32Microseconds = 0;
		}
		else
		{
			u32Microseconds -= u8SampleOld;
		}
	}
}

// definitions for integer math on internal cpu temperature
// The conversion formula needs to be based on the ATmega64M1 datasheet.
// This is a placeholder example, adjust according to your sensor calibration data or datasheet.
//	temperature = (adc_value - 324.31) / 1.22;

#define TEMP_OFFSET 32431  // 324.31 * 100
#define TEMP_SCALE 122    // 1.22 * 100

int16_t ADC_TEMPERATURE_read(uint8_t channel) {
	// Select the ADC channel with the left adjust result (if needed) and reference voltage
	int16_t temp;
	
	// Set the reference voltage to AVcc with external capacitor at AREF pin
	ADMUX = (1<<REFS0);

	// Enable the ADC and set the prescaler to 128 for 125kHz ADC clock with 16MHz system clock
	ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0);
	
	ADMUX = (ADMUX & 0xE0) | (channel & 0x1F);

	// Start the ADC conversion
	ADCSRA |= (1<<ADSC);

	// Wait for the conversion to finish
	while (ADCSRA & (1<<ADSC));

	// Return the ADC value
	temp = ((int32_t)(ADC * 100 - TEMP_OFFSET) * 100 / TEMP_SCALE);
	return temp;
}






void PlatformAssert( const char* peFilename, const int s32LineNumber )
{
	DebugOut("Assert %s: %d\n", peFilename, s32LineNumber);
	
//	while(1);
}

uint8_t PlatformGetRegistrationID( void )
{
	return(sg_u8ModuleRegistrationID);
}

void SetSysclock( void )
{
	// First set the prescaler enable
	CLKPR = (1 << CLKPCE);
	
	// Now set the desired prescale value
	CLKPR = SYSCLOCK_PRESCALE;
}

// Returns unique ID out of the on-chip EEPROM
uint32_t ModuleControllerGetUniqueID(void)
{
	uint32_t u32UniqueID;
	
	u32UniqueID = EEPROMRead(EEPROM_UNIQUE_ID);
	u32UniqueID |= ((uint32_t) EEPROMRead(EEPROM_UNIQUE_ID + 0x01) << 8);
	u32UniqueID |= ((uint32_t) EEPROMRead(EEPROM_UNIQUE_ID + 0x02) << 16);
	u32UniqueID |= ((uint32_t) EEPROMRead(EEPROM_UNIQUE_ID + 0x03) << 24);
	
	return(u32UniqueID);
}

#ifdef FAKE_CELL_DATA
static const uint16_t sg_u16FakeCellData[] =
{
// CELL_VOLTAGE_SCALE = 0.2487603277
// Voltage range: 3.400-3.900
// Temp range   : 21.20-27.50
	0x0347,0x815f,	// Cell #  0 Volt=3.627 Temp= 21.994
	0x031b,0x8190,	// Cell #  1 Volt=3.435 Temp= 25.018
	0x0334,0x817a,	// Cell #  2 Volt=3.542 Temp= 23.658
	0x037c,0x8159,	// Cell #  3 Volt=3.855 Temp= 21.595
	0x033c,0x818c,	// Cell #  4 Volt=3.579 Temp= 24.781
	0x0351,0x81b2,	// Cell #  5 Volt=3.670 Temp= 27.141
	0x0382,0x81ad,	// Cell #  6 Volt=3.881 Temp= 26.828
	0x0385,0x81b4,	// Cell #  7 Volt=3.894 Temp= 27.284
	0x0324,0x818e,	// Cell #  8 Volt=3.474 Temp= 24.895
	0x034b,0x8171,	// Cell #  9 Volt=3.641 Temp= 23.097
	0x0384,0x8191,	// Cell # 10 Volt=3.887 Temp= 25.097
	0x034d,0x8175,	// Cell # 11 Volt=3.653 Temp= 23.371
	0x0342,0x8194,	// Cell # 12 Volt=3.603 Temp= 25.295
	0x033a,0x816d,	// Cell # 13 Volt=3.567 Temp= 22.844
	0x031e,0x8161,	// Cell # 14 Volt=3.448 Temp= 22.066
	0x0368,0x818a,	// Cell # 15 Volt=3.770 Temp= 24.665
	0x0331,0x81a4,	// Cell # 16 Volt=3.532 Temp= 26.305
	0x0325,0x818a,	// Cell # 17 Volt=3.478 Temp= 24.647
	0x032a,0x8159,	// Cell # 18 Volt=3.500 Temp= 21.613
	0x0359,0x818b,	// Cell # 19 Volt=3.705 Temp= 24.714
	0x035c,0x8162,	// Cell # 20 Volt=3.717 Temp= 22.140
	0x034d,0x818f,	// Cell # 21 Volt=3.650 Temp= 24.949
	0x0318,0x8184,	// Cell # 22 Volt=3.421 Temp= 24.284
	0x0354,0x8166,	// Cell # 23 Volt=3.680 Temp= 22.400
	0x031c,0x8157,	// Cell # 24 Volt=3.438 Temp= 21.473
	0x034c,0x8158,	// Cell # 25 Volt=3.646 Temp= 21.511
	0x035f,0x81b7,	// Cell # 26 Volt=3.731 Temp= 27.481
	0x0340,0x815a,	// Cell # 27 Volt=3.597 Temp= 21.632
	0x035e,0x819c,	// Cell # 28 Volt=3.724 Temp= 25.786
	0x0339,0x819e,	// Cell # 29 Volt=3.565 Temp= 25.884
	0x0377,0x815a,	// Cell # 30 Volt=3.833 Temp= 21.636
	0x0335,0x8160,	// Cell # 31 Volt=3.547 Temp= 22.013
	0x0379,0x8180,	// Cell # 32 Volt=3.840 Temp= 24.031
	0x0361,0x815b,	// Cell # 33 Volt=3.738 Temp= 21.705
	0x034e,0x8170,	// Cell # 34 Volt=3.657 Temp= 23.002
	0x035d,0x8162,	// Cell # 35 Volt=3.719 Temp= 22.138
	0x0345,0x8161,	// Cell # 36 Volt=3.618 Temp= 22.075
	0x0369,0x8183,	// Cell # 37 Volt=3.772 Temp= 24.210
	0x035c,0x8171,	// Cell # 38 Volt=3.714 Temp= 23.120
	0x0360,0x819a,	// Cell # 39 Volt=3.734 Temp= 25.639
	0x033b,0x8163,	// Cell # 40 Volt=3.574 Temp= 22.208
	0x036a,0x8154,	// Cell # 41 Volt=3.777 Temp= 21.263
	0x0325,0x8162,	// Cell # 42 Volt=3.478 Temp= 22.132
	0x031c,0x81a4,	// Cell # 43 Volt=3.439 Temp= 26.265
	0x0378,0x817c,	// Cell # 44 Volt=3.838 Temp= 23.772
	0x0352,0x819d,	// Cell # 45 Volt=3.674 Temp= 25.870
	0x034a,0x81a7,	// Cell # 46 Volt=3.639 Temp= 26.497
	0x0378,0x8177,	// Cell # 47 Volt=3.835 Temp= 23.450
	0x0334,0x818a,	// Cell # 48 Volt=3.545 Temp= 24.643
	0x0345,0x81a4,	// Cell # 49 Volt=3.619 Temp= 26.273
	0x0373,0x815a,	// Cell # 50 Volt=3.816 Temp= 21.675
	0x0381,0x816e,	// Cell # 51 Volt=3.877 Temp= 22.887
	0x032c,0x8199,	// Cell # 52 Volt=3.507 Temp= 25.599
	0x0369,0x81a8,	// Cell # 53 Volt=3.773 Temp= 26.510
	0x0313,0x817c,	// Cell # 54 Volt=3.401 Temp= 23.806
	0x0352,0x8176,	// Cell # 55 Volt=3.674 Temp= 23.412
	0x0355,0x8171,	// Cell # 56 Volt=3.687 Temp= 23.099
	0x033d,0x819c,	// Cell # 57 Volt=3.581 Temp= 25.803
	0x0347,0x817f,	// Cell # 58 Volt=3.625 Temp= 23.971
	0x0351,0x8173,	// Cell # 59 Volt=3.667 Temp= 23.248
	0x0375,0x815b,	// Cell # 60 Volt=3.824 Temp= 21.717
	0x031b,0x8174,	// Cell # 61 Volt=3.433 Temp= 23.252
	0x037e,0x81b1,	// Cell # 62 Volt=3.861 Temp= 27.102
	0x0362,0x8168,	// Cell # 63 Volt=3.741 Temp= 22.543
	0x034b,0x815f,	// Cell # 64 Volt=3.642 Temp= 21.957
	0x0315,0x8173,	// Cell # 65 Volt=3.409 Temp= 23.190
	0x0329,0x81b5,	// Cell # 66 Volt=3.498 Temp= 27.327
	0x0356,0x817c,	// Cell # 67 Volt=3.692 Temp= 23.782
	0x0361,0x8174,	// Cell # 68 Volt=3.735 Temp= 23.273
	0x0330,0x8197,	// Cell # 69 Volt=3.526 Temp= 25.445
	0x0369,0x81a3,	// Cell # 70 Volt=3.771 Temp= 26.241
	0x0316,0x8173,	// Cell # 71 Volt=3.412 Temp= 23.193
	0x031f,0x817a,	// Cell # 72 Volt=3.451 Temp= 23.633
	0x0318,0x818a,	// Cell # 73 Volt=3.423 Temp= 24.670
	0x0372,0x818d,	// Cell # 74 Volt=3.813 Temp= 24.864
	0x0378,0x8197,	// Cell # 75 Volt=3.838 Temp= 25.448
	0x0360,0x81b2,	// Cell # 76 Volt=3.732 Temp= 27.137
	0x0387,0x818e,	// Cell # 77 Volt=3.900 Temp= 24.895
	0x0379,0x8198,	// Cell # 78 Volt=3.840 Temp= 25.501
	0x036f,0x8177,	// Cell # 79 Volt=3.800 Temp= 23.484
	0x0370,0x81a5,	// Cell # 80 Volt=3.801 Temp= 26.354
	0x0361,0x81b7,	// Cell # 81 Volt=3.739 Temp= 27.489
	0x036e,0x816d,	// Cell # 82 Volt=3.795 Temp= 22.850
	0x0342,0x8181,	// Cell # 83 Volt=3.604 Temp= 24.107
	0x0357,0x8195,	// Cell # 84 Volt=3.696 Temp= 25.363
	0x0323,0x8174,	// Cell # 85 Volt=3.468 Temp= 23.302
	0x0348,0x8163,	// Cell # 86 Volt=3.630 Temp= 22.209
	0x035e,0x818b,	// Cell # 87 Volt=3.725 Temp= 24.744
	0x0352,0x8199,	// Cell # 88 Volt=3.673 Temp= 25.592
	0x0320,0x8178,	// Cell # 89 Volt=3.457 Temp= 23.546
	0x0333,0x81b6,	// Cell # 90 Volt=3.539 Temp= 27.432
	0x0318,0x81b2,	// Cell # 91 Volt=3.423 Temp= 27.137
	0x037f,0x8157,	// Cell # 92 Volt=3.866 Temp= 21.493
	0x0350,0x81a4,	// Cell # 93 Volt=3.664 Temp= 26.308
		// Min temperature= 21.263
		// Max temperature= 27.489
		// Min voltage    =  3.401
		// Max voltage    =  3.900
};
#endif

// following is *100 to avoid floating point
#define CELL_VOLTAGE_STRING_LOWER				2250 // 2.25V/cell in millivolts
#define CELL_VOLTAGE_STRING_UPPER				4500 // 4.5V/cell * in millivolts
#define CELL_VOLTAGE_RANGE						(CELL_VOLTAGE_STRING_UPPER - CELL_VOLTAGE_STRING_LOWER)



// Staging area for all cell data moved to Frame
//static uint8_t sg_u8CellDataStoreStaging[ TOTAL_CELL_COUNT_MAX * (1 << BYTES_PER_CELL_SHIFT) ];

// Most recent data store
//static uint8_t sg_u8CellDataStore[ MAX_CELLS * (1 << BYTES_PER_CELL_SHIFT) ];

// Which cell # are we on?
static uint8_t sg_u8CellIndex;

// How far into sg_u8CellBufferTemp[] are we?
static uint8_t sg_u8CellBufferRX;

// Cell data reception - MUST BE 4 BYTE ALIGNED FOR 32 BIT TRANSFERS
static volatile uint8_t __attribute__((aligned(4))) sg_u8CellBufferTemp[4];


// MC RX Statistic
static uint16_t sg_u16BytesReceived;
static uint8_t sg_u8CellReports;






// Sets the # of cells we expect for this configuration
static void CellCountExpectedSet(uint8_t u8CellCountExpected)
{
	// If we ask for too many cells, clip it so an uninitialized EEPROM won't
	// result in data corruption.
	if (u8CellCountExpected > TOTAL_CELL_COUNT_MAX)
	{
		u8CellCountExpected = TOTAL_CELL_COUNT_MAX;
	}
	
//	MBASSERT(u8CellCountExpected <= (sizeof(sg_sFrame.cellData) >> BYTES_PER_CELL_SHIFT));
	sg_sFrame.sg_u8CellCountExpected = u8CellCountExpected;

	// Calculate string cell voltage min and max
	sg_sFrame.sg_i32VoltageStringMin = CELL_VOLTAGE_STRING_LOWER * ((int32_t) u8CellCountExpected);  // in millivolts
	sg_sFrame.sg_i32VoltageStringMax = CELL_VOLTAGE_STRING_UPPER * ((int32_t) u8CellCountExpected);	// in millivolts
	sg_sFrame.sg_i16VoltageStringPerADC = (int16_t)(((sg_sFrame.sg_i32VoltageStringMax - sg_sFrame.sg_i32VoltageStringMin) * ADC_VOLT_FRACTION) / ((int32_t) (1 << ADC_BITS)));  // in millivolts
}

// Returns the module's current state
EModuleControllerState ModuleGetState(void)
{
	return(sg_eModuleControllerStateCurrent);
}


static uint8_t sg_u8ControllerStatusMsgCount = 0;

static void SendModuleControllerStatus(void)
{
	sg_bSendModuleControllerStatus = true;
	sg_u8ControllerStatusMsgCount = 0;
	sg_bIgnoreStatusRequests = true;  // Start ignoring new requests
}

typedef enum
{
	ESTRING_INIT,
	ESTRING_OFF,
	ESTRING_ON,
	ESTRING_IGNORE_FIRST_MESSAGE,
	ESTRING_OPERATIONAL
} EStringPowerState;

static volatile EStringPowerState sg_eStringPowerState = ESTRING_INIT;

// This handles power state changes for the cell string
static void CellStringPowerStateMachine(void)  // this is called at the start of each READ and WRITE frame
{
	switch (sg_eStringPowerState)
	{
		case ESTRING_INIT:  // occurs after a reset, it is same as ESTRING_OFF with addition of SendModuleControllerStatus
		{
			CELL_POWER_DEASSERT();
			sg_eStringPowerState = ESTRING_OFF;
			FrameInit(false);  // clear out string data
			SendModuleControllerStatus();
			break;
		}
		case ESTRING_OFF: // turns power OFF, then sets the power timer to wait for the circuits to settle
		{
			CELL_POWER_DEASSERT();
			FrameInit(false);  // clear out string data
			sg_eStringPowerState = ESTRING_ON;
			sg_u8CellStringPowerTimer = PERIODIC_INTERRUPT_MS_TO_TICKS(CELL_POWER_OFF_TO_ON_MS);
			sg_sFrame.sg_u8CellCPUCountFewest = 0xff;  // reset min max cell counts
			sg_sFrame.sg_u8CellCPUCountMost = 0x0;
			break;
		}
		case ESTRING_ON:
		{
			if (0 == sg_u8CellStringPowerTimer)  // wait for the off delay before turning power back on
			{
				CELL_POWER_ASSERT();
				
				sg_eStringPowerState = ESTRING_IGNORE_FIRST_MESSAGE;  
				// this will insert at least one frame before an actual read occurs, 
				// which is conditional on ESTRING_OPERATIONAL status
			}
			break;
		}
		case ESTRING_IGNORE_FIRST_MESSAGE:  // this provides one frame delay between power on and first read
		{
			sg_eStringPowerState = ESTRING_OPERATIONAL;
			break;
		}
		case ESTRING_OPERATIONAL:  // we're up and running, nothing to change
		{
			break;
		}
		default:
		{
			MBASSERT(0);
			break;
		}
	}
}

// Returns max charge/discharge in units of 0.02A with a base of -655.36A
static void CurrentThresholdsGet(uint16_t *pu16MaxDischargeCurrent,
								 uint16_t *pu16MaxChargeCurrent)
{
	if (pu16MaxDischargeCurrent)
	{
		*pu16MaxDischargeCurrent = EEPROMRead(EEPROM_MAX_DISCHARGE_CURRENT);
		*pu16MaxDischargeCurrent |= ((uint16_t)EEPROMRead(EEPROM_MAX_DISCHARGE_CURRENT + 1)) << 8;
		
		if ((0x0000 == *pu16MaxDischargeCurrent) ||
			(0xffff == *pu16MaxDischargeCurrent))
		{
			// If the EEPROM isn't programmed, just return the maximum discharge current
			*pu16MaxDischargeCurrent = (uint16_t) ((MAX_DISCHARGE_AMPS - CURRENT_FLOOR) / 0.02);  
		}
	}
	
	if (pu16MaxChargeCurrent)
	{
		*pu16MaxChargeCurrent = EEPROMRead(EEPROM_MAX_CHARGE_CURRENT);
		*pu16MaxChargeCurrent |= ((uint16_t)EEPROMRead(EEPROM_MAX_CHARGE_CURRENT + 1)) << 8;
		
		if ((0x0000 == *pu16MaxChargeCurrent) ||
			(0xffff == *pu16MaxChargeCurrent))
		{
			// If the EEPROM isn't programmed, just return the maximum charge current
			*pu16MaxChargeCurrent = (uint16_t) (( MAX_CHARGE_AMPS - CURRENT_FLOOR) / 0.02); 
		}
	}	
}

void TimerInit( void )
{
	// Timer 0 (8 bit)
	
	// Set mode and prescaler
	TCCR0A = 0;
	TCCR0B = TIMER_PRESCALER_CS0;
		
	// Clear power reduction register to enable timer 0
	PRR &= (uint8_t)~(1 << PRTIM0);
	
	// Timer 1 (16 bit)
	
	// Set mode and prescaler
	TCCR1A = 0;
	TCCR1B = TIMER_PRESCALER_CS1;
	
	// Start the periodic timer
	TIMER1_CHA_INT(PERIODIC_COMPARE_A_RELOAD);
		
	// Clear power reduction register to enable timer 1
	PRR &= (uint8_t)~(1 << PRTIM1);
	
	TIMSK0 &= ~(1 << TOIE0);  // Disable Timer0 overflow interrupt
	TIMSK1 &= ~(1 << TOIE1);  // Disable Timer1 overflow interrupt

}

// Timer 1 compare interrupt. This is called every (cpu speed / divisor) * reload clocks.
// Currently 8Mhz, with a /256, it's once every 100ms due to PERIODIC_COMPARE_A_RELOAD
ISR(TIMER1_COMPA_vect, ISR_NOBLOCK)
{
	//Reload the timer
	TIMER1_CHA_INT(PERIODIC_COMPARE_A_RELOAD);
	sg_bNewTick = true;  // set tru in periodic tick isr, cleared in main loop

	// Handle the cell report timer, this always runs because string state machine is only
	// called at the start of each READ and WRITE frame

	++sg_u8CellFrameTimer;
	if (sg_u8CellFrameTimer >= PERIODIC_CALLBACK_RATE_TICKS)  // frame ended, deal with it
	{
		sg_u8CellFrameTimer = 0;  //reset the frame timer
		sg_bFrameStart = true;  // tell main loop it's first time through
			
		//toggle between read and write frames
		if (EFRAMETYPE_WRITE == sg_eFrameStatus)
		{
			sg_eFrameStatus = EFRAMETYPE_READ;  // start the read frame
		}
		else  
		{
			sg_eFrameStatus = EFRAMETYPE_WRITE;  // start the write frame
		}
	}
	
	// And also handle the pack controller timer
	if (sg_u8TicksSinceLastPackControllerMessage < 0xff)
	{
		sg_u8TicksSinceLastPackControllerMessage++;
	}
	if (sg_u8TicksSinceLastPackControllerMessage >= PACK_CONTROLLER_TIMEOUT_TICKS)
	{
		sg_u8TicksSinceLastPackControllerMessage -= PACK_CONTROLLER_TIMEOUT_TICKS;
		sg_bPackControllerTimeout = true;
	}
	
	// Cell string power timer runs all the time, always a countdown
	if (sg_u8CellStringPowerTimer)
	{
		sg_u8CellStringPowerTimer--;
	}
}

void WatchdogReset( void )
{
#ifdef WDT_ENABLE
	wdt_reset();
#endif
}

// void WatchdogEnable( void )  //  replaced by WDTSetLeash
// {
// #ifdef WDT_ENABLE
// 	wdt_enable(WDT_LEASH_LONG);
// #endif
// 	sg_eWDTCurrentStatus = EWDT_NORMAL;
// }

// Assume interrupts are disabled
void WatchdogOff( void )  // turn it off even when WDT_ENABLE is not defined
{
	wdt_reset();
	wdt_disable();
}

static void WDTSetLeash( uint8_t u8Leash, eWDTstatus eStatus)
{
#ifdef WDT_ENABLE
	sg_eWDTCurrentStatus = eStatus;
	if (WDT_LEASH_SHORT == u8Leash)  // macro needs specific values, if not explicitly short then default to long leash
	{
		wdt_enable(WDT_LEASH_SHORT);
	}
	else
	{
		wdt_enable(WDT_LEASH_LONG);
	}
	wdt_reset();
#endif
}


// Watchdog timer interrupt
ISR(WDT_vect, ISR_BLOCK)
{
	MBASSERT(0);
}

static void ModuleControllerStateSet( EModuleControllerState eNext )
{
	if(eNext < EMODSTATE_COUNT)
	{
		sg_eModuleControllerStateTarget = eNext;
	}
}



static void ModuleControllerStateSetMax( EModuleControllerState eNext )
{
	if(eNext < EMODSTATE_COUNT)
	{
		sg_eModuleControllerStateMax = eNext;
		if (sg_eModuleControllerStateCurrent > eNext) // we are in a higher state than allowed
		{
			sg_eModuleControllerStateTarget = eNext;
		}
	}
}

static void ModuleControllerStateHandle( void )
{
	EModuleControllerState eNext = sg_eModuleControllerStateTarget;
	
#ifndef STATE_CYCLE	
	if ((eNext > sg_eModuleControllerStateMax) || (sg_eModuleControllerStateCurrent > sg_eModuleControllerStateMax) )  // technically should not occur but just in case
	{
		eNext = sg_eModuleControllerStateMax;
	}
#endif	
	// see if we need to transition
	if ( eNext != sg_eModuleControllerStateCurrent)
	{
		//  if we go off to lunch due to a switch glitch, watchdog will reset and we will eventually come back here to try the transition again
		//  if the relays were successfully switched then the net effect second time thru will just be the housekeeping	
		switch (eNext)  //handle state transitions to target, set via CAN or cycler
		{
			case EMODSTATE_INIT:
			{
				WDTSetLeash(WDT_LEASH_LONG,EWDT_NORMAL);

				// Always start in the off state
				sg_eModuleControllerStateCurrent = EMODSTATE_OFF;
				sg_eModuleControllerStateTarget = STATE_DEFAULT;
				eNext = sg_eModuleControllerStateCurrent;  //fall through to EMODSTATE_OFF
			
				// handle any recovery and init stuff here
				// Turn on the ADC, it should only be turned off in sleep mode
				ADCSetPowerOn();
			}			
			case EMODSTATE_OFF:
			{
				// use WDT to recover from glitches on reset/power
				// Shut off the FET if it's on
				WDTSetLeash(WDT_LEASH_SHORT,EWDT_FET_OFF);  // set to 15mS
				FET_EN_DEASSERT();
				// wait for gate to fully discharge
				Delay((uint32_t) 5* (uint32_t) 1000);  //must be shorter than WDT_LEASH_SHORT
				// made it!

				WDTSetLeash(WDT_LEASH_SHORT,EWDT_MECH_RLY_OFF);

				// Turn off mechanical relay
				RELAY_EN_DEASSERT();
	
				// Give the relay time to settle so no power is applied
				// across it until it's fully settled.
				Delay((uint32_t) 5* (uint32_t) 1000); //must be shorter than WDT_LEASH_SHORT
				// made it!
				
				WDTSetLeash(WDT_LEASH_LONG,EWDT_NORMAL);

				// Disable pin change interrupt for /OCF (and bank C)
				PCMSK1 &= (uint8_t) ~(1 << PIN_OCF_N);		// /OCF interrupt disable
				PCICR &= (uint8_t) ~(1 << PCIE1);			// Bank C interrupt disable
				
				sg_bCellBalanceReady = true;
				sg_bCellBalancedOnce = false;				
				sg_bStopDischarge = false;
				
				// close logging session
				
				if (sg_bSDCardReady)
				{ 
					sg_bSDCardReady = STORE_EndSession();
				}
				break;			
			}
			
			case EMODSTATE_STANDBY:
			{
				// use WDT to recover from glitches on reset/power
				// Shut off the FET if it's on
				WDTSetLeash(WDT_LEASH_SHORT,EWDT_FET_OFF);  // set to 15mS
				FET_EN_DEASSERT();
				// wait for gate to fully discharge
				Delay((uint32_t) 5* (uint32_t) 1000);  //must be shorter than WDT_LEASH_SHORT
				// made it!

	
				WDTSetLeash(WDT_LEASH_SHORT,EWDT_MECH_RLY_ON);
				
				// Disable pin change interrupt for /OCF (and bank C)
				PCMSK1 &= (uint8_t) ~(1 << PIN_OCF_N);		// /OCF interrupt disable
				PCICR &= (uint8_t) ~(1 << PCIE1);			// Bank C interrupt disable

				// Turn on the relay (does nothing when transiting to off)
#ifdef STATE_CYCLE
				RELAY_EN_ASSERT();
#else
				// Safety check: only enable relay if module is registered
				if (sg_bModuleRegistered) {
					RELAY_EN_ASSERT();
				}
#endif
				Delay((uint32_t) 5* (uint32_t) 1000); //must be shorter than WDT_LEASH_SHORT
				// made it!

				WDTSetLeash(WDT_LEASH_LONG,EWDT_NORMAL);

				sg_bCellBalanceReady = true;			
				sg_bCellBalancedOnce = false;
				sg_bStopDischarge = false;
				
//				start new logging session
				if (sg_bSDCardReady)
				{
					sg_bSDCardReady = STORE_StartNewSession();
				}
			
				break;
			}
			
			case EMODSTATE_PRECHARGE:  // this mode needs lots more testing, not currently used
			{
				// Do set number of pulses and look for overcurrent flag
				uint8_t u8PulseCount = 11;
				
				if (!RELAY_ASSERTED()) // was off for whatever reason, turn on and delay
				{					
					WDTSetLeash(WDT_LEASH_SHORT,EWDT_MECH_RLY_ON);
					// Turn on the relay 
#ifdef STATE_CYCLE
					RELAY_EN_ASSERT();
#else
					// Safety check: only enable relay if module is registered
					if (sg_bModuleRegistered) {
						RELAY_EN_ASSERT();
					}
#endif
					// Give the relay time to settle so no power is applied
					Delay((uint32_t) 5* (uint32_t) 1000);  //must be shorter than WDT_LEASH_SHORT
					// made it!

				}

				while (u8PulseCount--)  //overcurrent will set flag but won't transition to STANDBY
				{
					WDTSetLeash(WDT_LEASH_SHORT,EWDT_FET_ON);

#ifdef STATE_CYCLE
					FET_EN_ASSERT();
#else
					// Safety check: only enable FET if module is registered
					if (sg_bModuleRegistered) {
						FET_EN_ASSERT();
					}
#endif
					Delay((uint32_t) 1 * (uint32_t) 1000); // short pulses to save FET! - there is 5uS overhead so 5 will give 10
					FET_EN_DEASSERT();

					Delay((uint32_t) 5* (uint32_t) 1000); //must be shorter than WDT_LEASH_SHORT
					// made it!
					WDTSetLeash(WDT_LEASH_LONG,EWDT_NORMAL);  // set to normal while we delay between pulses also for last pulse

					Delay((uint32_t) 50* (uint32_t) 1000);	// give it time to recover
					
					if (sg_bOvercurrentSignal)
					{
					// if we are hitting current limit add more pulses
						sg_bOvercurrentSignal = false;
						u8PulseCount++;
					}
				}

				
				break;
			}
			
			case EMODSTATE_ON:
			{
				// Now turn on the FET
				if (!RELAY_ASSERTED()) // was off for whatever reason, turn on and delay
				{
					WDTSetLeash(WDT_LEASH_SHORT,EWDT_MECH_RLY_ON);
					// Turn on the relay
#ifdef STATE_CYCLE
					RELAY_EN_ASSERT();
#else
					// Safety check: only enable relay if module is registered
					if (sg_bModuleRegistered) {
						RELAY_EN_ASSERT();
					}
#endif
					// Give the relay time to settle so no power is applied
					Delay((uint32_t) 5* (uint32_t) 1000);  //must be shorter than WDT_LEASH_SHORT
					// made it!
				}
				
				WDTSetLeash(WDT_LEASH_SHORT,EWDT_FET_ON);

#ifdef STATE_CYCLE
				FET_EN_ASSERT();  //overcurrent will set flag AND transition to STANDBY
#else
				// Safety check: only enable FET if module is registered
				if (sg_bModuleRegistered) {
					FET_EN_ASSERT();  //overcurrent will set flag AND transition to STANDBY
				}
#endif
				Delay((uint32_t) 5* (uint32_t) 1000); //must be shorter than WDT_LEASH_SHORT
				// made it!
				WDTSetLeash(WDT_LEASH_LONG,EWDT_NORMAL);  
							
				sg_bCellBalanceReady = false;
				sg_bCellBalancedOnce = false;
				sg_bStopDischarge = true;

				// Enable pin change interrupt for /OCF (and bank C)
				PCMSK1 |= (1 << PIN_OCF_N);		// /OCF interrupt enable
				PCICR |= (1 << PCIE1);			// Bank C interrupt enable


//				StorageSetPowerState(true);
				break;
			}

			default:
			{
				// Bad state
				MBASSERT(0);
				eNext = EMODSTATE_INIT; // try to reset
			}
		}

		// Record the new state	
		sg_eModuleControllerStateCurrent = eNext;  //we are now in the new state
		SendModuleControllerStatus();
	}

}

// Checks for 5V loss
static void Check5VLoss(uint8_t u8NewState)
{
	if( u8NewState & (1 << PIN_5V_DET) )
	{
		// Not asserted
	}
	else
	{
		// 5V detect is low
		FET_EN_DEASSERT();
		RELAY_EN_DEASSERT();
		ModuleControllerStateSet( EMODSTATE_OFF );
	}
}

// Port C pin change interrupt handler
ISR(PCINT1_vect, ISR_NOBLOCK)
{
	uint8_t u8NewState = PINC;

	// Only look at the overcurrent if we're in the on or precharge state
	if ((EMODSTATE_ON == sg_eModuleControllerStateCurrent) || (EMODSTATE_PRECHARGE == sg_eModuleControllerStateCurrent))
	{
		if( u8NewState & (1 << PIN_OCF_N) )  // starting with rev E this is an inverted status of FET control signal at flipflop output.  
											 // if pin is high in on or precharge state it means flipflop was cleared by overcurrent or overtemp.
											 // we can read cpu temperature to see if temp is the likely cause but it doesn't really matter.
//		{
//			// Not asserted
//		}
//		else
		{
			// Over current signal (/OCF) asserted
			FET_EN_DEASSERT();
			if (EMODSTATE_ON == sg_eModuleControllerStateCurrent)  //
				{
				ModuleControllerStateSet( EMODSTATE_STANDBY );
				}
		
			// Disable pin change interrupt for /OCF (and bank C)
			PCMSK1 &= (uint8_t) ~(1 << PIN_OCF_N);		// /OCF interrupt disable
			PCICR &= (uint8_t) ~(1 << PCIE1);			// Bank C interrupt disable
	


			// Cause a CAN message for an overcurrent signal
			sg_bOvercurrentSignal = true;
		}
	}
	
//	Check5VLoss(u8NewState);
}



// Port D pin change interrupt handler
ISR(PCINT2_vect, ISR_NOBLOCK)
{
	uint8_t u8NewState = PORT_5V_DET;

	Check5VLoss(u8NewState);
}

void CANReceiveCallback(ECANMessageType eType, uint8_t* pu8Data, uint8_t u8DataLen)
{
	
// 	uint8_t savedCANGIE = CANGIE;  // these are used to temporarily disable CAN ints 
// 	CANGIE &= ~(1<< ENIT);
	bool bIsRegistered = sg_bModuleRegistered;
// 	CANGIE = savedCANGIE;
		
	if( ECANMessageType_ModuleRegistration == eType )
	{
		if( 8 == u8DataLen )
		{
			uint8_t u8RegID = pu8Data[0];
			
			// If the qualifiers match what was sent in the announcement, this is for us
			if( (MANUFACTURE_ID == pu8Data[2]) &&
				(PART_ID == pu8Data[3]) &&
				(sg_sFrame.moduleUniqueId == *((uint32_t *) &pu8Data[4])) )
			{
				sg_u8TicksSinceLastPackControllerMessage = 0;
				
				// Assign the new registration ID
				sg_u8ModuleRegistrationID = u8RegID;
				
				// Send a status to the pack controller - this will do
				// Status #1-#3 eventually
				SendModuleControllerStatus();
				
				// Send hardware detail as well
				sg_bSendHardwareDetail = true;
				
				// Indicate our module controller is registered
				sg_bModuleRegistered = true;
				
				DebugOut("RX Registration - Module ID=%02x registered successfully\r\n", u8RegID);
				
				// Cancel any pending announcement since we're now registered
				sg_bAnnouncementPending = false;
				sg_u8AnnouncementDelayTicks = 0;
				
				// And that we send a time request
				sg_bSendTimeRequest = true;
			}
		}
	}
	else
	{
		uint8_t u8RegID = pu8Data[0];
	
		// First check the registration ID matches ours	and is non-zero
		if (u8DataLen >= 1)
		{
			sg_u8TicksSinceLastPackControllerMessage = 0;
			
			// see if this is a heartbeat message, most frequent!  process max state even if not registered
			if( ECANMessageType_MaxState == eType )
			{
					uint8_t u8State = pu8Data[1] & 0xf;
					#ifndef STATE_CYCLE  //ignore CAN commands when cycling
					// Change state
					ModuleControllerStateSetMax( u8State );
					#endif
			}
				
			// Handle commands that are only valid when registered
			// Note: For status requests, we re-check sg_bModuleRegistered in case we just got registered
			if ((bIsRegistered || (ECANMessageType_ModuleStatusRequest == eType && sg_bModuleRegistered)) &&
				(u8RegID == sg_u8ModuleRegistrationID))
			{
				// Since this message is for us, now check the type and length
				if( ECANMessageType_ModuleStatusRequest == eType )
				{
					if( 1 == u8DataLen )
					{
						if (!sg_bIgnoreStatusRequests) {
							DebugOut("RX Status Request - sending status\r\n");
							SendModuleControllerStatus();
						}
						// else silently ignore - we're already sending
					}
				}
			
				else if( ECANMessageType_ModuleCellDetailRequest == eType )
				{
					if( 3 == u8DataLen )
					{
						// If not already replying and this is a valid cell number, schedule response
						if( (false == sg_bSendCellStatus) && (pu8Data[1] < sg_sFrame.sg_u8CellCountExpected) )
						{
							sg_u8CellStatus = pu8Data[1];
							sg_u8CellStatusTarget = sg_u8CellStatus;
						
							if (CELL_DETAIL_ALL == pu8Data[1])
							{
								// We're sending all cells
								sg_u8CellStatusTarget = sg_sFrame.sg_u8CellCountExpected;
								sg_u8CellStatus = 0;
							}
						
							// We're sending cell statuses!
							sg_bSendCellStatus = true;
						}
					}
				}
				else if( ECANMessageType_ModuleStateChangeRequest == eType )
				{
					sg_u8TicksSinceLastPackControllerMessage = 0;
					if( 2 == u8DataLen )
					{
						uint8_t u8State = pu8Data[1] & 0xf;
#ifndef STATE_CYCLE  //ignore CAN commands when cycling			
						// Change state
						ModuleControllerStateSet( u8State );
#endif
					}
				}
				else if( ECANMessageType_ModuleHardwareDetail == eType )
				{
					// For hardware detail, the payload is irrelevant.
					sg_bSendHardwareDetail = true;
				}
				else if( ECANMessageType_ModuleDeRegister == eType )
				{
					// Individual deregister for this module
					DebugOut("RX Individual De-Register - module ID=%02x deregistered\r\n", u8RegID);
					sg_u8ModuleRegistrationID = 0;
					sg_bModuleRegistered = false;
					sg_bIgnoreStatusRequests = false;  // Reset all status flags
					ModuleControllerStateSet( EMODSTATE_OFF );  // turn off when deregistered
				}
			}
			
			// Handle commands that are global broadcasts and don't require 
			// registration
			else if( ECANMessageType_AllDeRegister == eType )
			{
				// Deregister this module
				DebugOut("RX All De-Register - module deregistered\r\n");
				sg_u8ModuleRegistrationID = 0;
				sg_bModuleRegistered = false;
				sg_bIgnoreStatusRequests = false;  // Reset all status flags
				ModuleControllerStateSet( EMODSTATE_OFF );  // turn off when deregistered
			}
			else if( ECANMessageType_AllIsolate == eType )
			{
#ifndef STATE_CYCLE  //ignore CAN commands when cycling
				// Isolate this module now
				ModuleControllerStateSet( EMODSTATE_OFF);
#endif
			}			
			else if( ECANMessageType_SetTime == eType )
			{
				// Set time! From pack controller
				RTCSetTime(*((uint64_t *) pu8Data));
			}
			else if( ECANMessageType_ModuleAnnounceRequest == eType )
			{
				// Pack controller is requesting announcements from unregistered modules
				if (!sg_bModuleRegistered && !sg_bAnnouncementPending)
				{
					// Use last byte of unique ID as delay in milliseconds (0-255ms)
					// This saves computation and works well for sequential IDs
					uint8_t u8RandomDelay = (uint8_t)(sg_sFrame.moduleUniqueId & 0xFF);
					
					DebugOut("RX Announce Request (UNREGISTERED) - scheduling response in %dms\r\n", u8RandomDelay);
					
					// Schedule announcement after delay (assuming 10ms ticks)
					sg_u8AnnouncementDelayTicks = u8RandomDelay / 10;  // Convert to 10ms ticks
					if (sg_u8AnnouncementDelayTicks == 0) sg_u8AnnouncementDelayTicks = 1;  // Minimum 1 tick delay
					sg_bAnnouncementPending = true;
				}
				else if (sg_bModuleRegistered)
				{
					// We're registered, ignore announce requests
					DebugOut("RX Announce Request (REGISTERED) - ignoring\r\n");
				}
				else
				{
					// Announcement already pending, ignore duplicate request
					DebugOut("RX Announce Request - already pending\r\n");
				}
			}
		}
	}
}

// Called at the start of cell string data via MC RX
void vUARTRXStart(void)
{
	sg_u8CellBufferRX = 0;
	sg_u8CellIndex = 0;
	sg_u16BytesReceived = 0;
	sg_u8CellReports = 0;
}

// Called at start of WRITE frame to wrap up any receive housekeeping prior to frame processing
void vUARTRXEnd(void)
{
#ifdef FAKE_CELL_DATA
#else
	// disable the edge interrupt
//	VUART_RX_DISABLE();
	// disable bit timer interrupt, should already be off but just in case
//	TIMER_CHB_INT_DISABLE();
	// update bytes and cells received
	sg_sFrame.sg_u16BytesReceived = sg_u16BytesReceived;
	sg_sFrame.sg_u8CellCPUCount = sg_u8CellReports;
	sg_u16BytesReceived = 0;
	sg_u8CellReports = 0;

#endif
}

// This is called for every byte received from MC RX (from the cell chain)
void vUARTRXData( uint8_t u8rxDataByte )
{
#ifdef FAKE_CELL_DATA
#else
	// Add the data
	sg_u8CellBufferTemp[sg_u8CellBufferRX++] = u8rxDataByte;
	sg_u16BytesReceived++;
	
	// If we have a full cell buffer then copy it into the cell data store
	if (sg_u8CellBufferRX >= sizeof(sg_u8CellBufferTemp))
	{
		sg_u8CellBufferRX = 0;
		
#ifdef REQUEST_DEBUG_CELL_RESPONSE
		// We're looking for a specific pattern. If it doesn't match what
		// we expect, halt
		if ((sg_u8CellBufferTemp[0] != ((uint8_t) PATTERN_VOLTAGE)) ||
			(sg_u8CellBufferTemp[1] != ((uint8_t) (PATTERN_VOLTAGE >> 8))) ||
			(sg_u8CellBufferTemp[2] != ((uint8_t) PATTERN_TEMPERATURE)) ||
			(sg_u8CellBufferTemp[3] != ((uint8_t) (PATTERN_TEMPERATURE >> 8))))
		{
			volatile uint8_t u8Foo = 0;  //change to 1 to enable halting here for debug
			

			while (u8Foo)
			{
				// Loop forever
			}
		}
#endif
		// If we're within range, we should copy the data into the cell data store.
		// Each data store is 4 bytes so do 32-bit xfer:
		if (sg_u8CellIndex < MAX_CELLS)
		{
			// 32bit copy for speed - inside the ISR - copied atomically
			// ASSUMES 4-BYTE ALIGNMENT - CHECK!
			*(uint32_t*)&(sg_sFrame.StringData[sg_u8CellIndex]) = *((uint32_t*)sg_u8CellBufferTemp);
		
			*((uint32_t*)sg_u8CellBufferTemp) = 0;  // zero out all 32 bits to avoid stale data (?)

			// Next cell!
			sg_u8CellIndex++;
			sg_u8CellReports++;
		}
	}
#endif
}

// Converts 0-15 value scaled to 0-100
static const uint8_t sg_u8FractionalLookup[] =
{
	0,
	6,
	12,
	18,
	25,
	31,
	37,
	43,
	50,
	56,
	62,
	68,
	75,
	81,
	87,
	93
};

// Converts incoming cell data to the CAN bus-documented format
// for consumption by the pack controller.

//reality checks for data validity

#define MAX_VALID_CELL_VOLTAGE 0x0400  //4.5V, max ADC range
#define MIN_VALID_CELL_VOLTAGE 0x01f0  //2.2V, cell curcuit doesn't run below that

// just returns result of validity check if pointer is NULL
// otherwise mofifies pointed location to read in mV corrected 
static bool CellDataConvertVoltage( uint16_t u16CellData,   
							 uint16_t* pu16Voltage 
							)
{
	uint16_t u16Voltage = u16CellData;
	bool bCellDataValid = true;  //assume good to start
					
	// Scale voltage - Needs to be in millivolt increments - 0-65.535mv
	//
	// Incoming voltage is 0-(1<<CELL_VOLTAGE_BITS) from 0-CELL_VREF scale (uses internal VREF)
	
	u16Voltage &= ((1 << (CELL_VOLTAGE_BITS)) - 1);		// Mask off anything we aren't using
	
	if ((u16Voltage < MIN_VALID_CELL_VOLTAGE) || (u16Voltage > MAX_VALID_CELL_VOLTAGE))  // see if we are in range
	{
		bCellDataValid = false;  // something got messed up
		u16Voltage = 0;  //
	}
	else
	{
		// Convert from # of scale of the ADC to the nondivided voltage range, in millivolts
		uint32_t temp = (uint32_t)u16Voltage * VOLTAGE_CONVERSION_FACTOR;
		u16Voltage = (uint16_t)((temp / ADC_MAX_VALUE + FIXED_POINT_SCALE/2) / FIXED_POINT_SCALE);
	}
	// Return the values if pointers nonzero
	if( pu16Voltage )
	{
		*pu16Voltage = u16Voltage;
	}
	return(bCellDataValid);
}
	
#define MIN_VALID_CELL_TEMP -20  //-20C
#define MAX_VALID_CELL_TEMP 120  //120C
//
// Scale temperature - 0.01 degree C increments. 0x7fff=Temperature data not valid
//
// Incoming temperature is as follows - signed 8.4 fixed point format (see 9843 datasheet)
//
// Bits 0-3  - 16ths of a degree C
// Bits 4-11 - Whole degrees C
// Bit 12    - Temperature sign bit - 0=Positive, 1=Negative
	
// just returns result of validity check if pointer is NULL		
static bool CellDataConvertTemperature( int16_t s16CellData,  
										int16_t* ps16Temperature
										)
{
	bool bCellDataValid = true;  //assume good to start
	int16_t s16Temperature = s16CellData;
	uint8_t u8Fractional;

	if (TEMPERATURE_INVALID != s16Temperature)
	{
		u8Fractional = (uint8_t) (s16Temperature & 0x0f);
  		
		if (s16Temperature & (1 << 12))
		{
			// Sign extend/2's complement
			s16Temperature |= 0xf000;
		}
		else
		{
			s16Temperature &= ~MSG_CELL_TEMP_I2C_OK; // clear the i2c bit
		}
		
		// Turn s16Temperature into whole degrees C
		s16Temperature >>= 4;
		
		// check for valid range
		if ((s16Temperature < MIN_VALID_CELL_TEMP) || (s16Temperature > MAX_VALID_CELL_TEMP))
		{
			s16Temperature = TEMPERATURE_INVALID;
			bCellDataValid = false;
		}
		else
		{
			// Scale to 100ths of degrees C
			s16Temperature = (s16Temperature * 100) + (int16_t) sg_u8FractionalLookup[u8Fractional];
			s16Temperature += TEMPERATURE_BASE;			
		}
	}
	else
	{
		bCellDataValid = false;
	}
	
	// Return the values if pointers nonzero	
	if( ps16Temperature )
	{
		*ps16Temperature = s16Temperature; 
	}
	return(bCellDataValid);
}


static void CellDataConvert( CellData* pCellData,
							uint16_t* pu16Voltage,
							int16_t* ps16Temperature
							)
{
	if (!CellDataConvertVoltage((*pCellData).voltage, pu16Voltage))
	{
		// add error handling here if needed
	}
	if (!CellDataConvertTemperature((*pCellData).temperature, ps16Temperature))
	{
		// add error handling here if needed
	}
	
}


// Called in interrupt context when it's time to get a new reading
uint16_t PlatformGetSendData( bool bUpdateBalanceStatus )
{
	uint16_t u16SendValue = 0;
	
	
	if( sg_bStopDischarge )
	{
		if (bUpdateBalanceStatus)	// don't update if ony an availability check
		{
			sg_bStopDischarge = false;
			sg_bCellBalancedOnce = true;
		}
		u16SendValue = 0x3ff;
	}
	else
	{
		// Always request reports
#ifdef REQUEST_CELL_BALANCE_ENABLE
		// When not on, send cell balance voltage once
		if( sg_bCellBalanceReady )
		{

#ifdef STATE_CYCLE  //use the OFF and STDBY states to test discharge functionality
			u16SendValue = 0x1f0;  //set target to 2.25V
#else
			u16SendValue = sg_u16LowCellVoltageRAW & 0x3ff;
			
			if (u16SendValue < CELL_BALANCE_DISCHARGE_THRESHOLD)
			{
				u16SendValue = CELL_BALANCE_DISCHARGE_THRESHOLD;
			}
#endif
			if (bUpdateBalanceStatus)	// don't update if only an availability check
			{
				sg_bCellBalanceReady = false;
				sg_bCellBalancedOnce = true;  
			}
		}
		else
#endif
		{
			u16SendValue |= MSG_CELL_SEND_REPORT;
			
			// Optionally request a specific response from the cells 
#ifdef REQUEST_DEBUG_CELL_RESPONSE
			u16SendValue |= MSG_CELL_SEND_PATTERN;
#endif
		}
	}
	
	return( u16SendValue );
}
				

// CURRENT0 and CURRENT1 current measurements combine to form a current value
// current ADC0 (PB5, vout on current sensor) - this is the current reading.  it is used in conjunction with ADC1
// current ADC1 (PB6, vref on current sensor) - this reading is the zero-current value.  subtract it from ADC0 to get a signed current value.
static void ModuleCurrentConvertReadings( void )
{
	int16_t s16Offset;
	int16_t s16ZeroValue;
	int32_t s32Current;
	uint8_t i;
				
//	cli();
	s16Offset = sg_sFrame.ADCReadings[ EADCTYPE_CURRENT0 ].u16Reading; 
	s16ZeroValue = sg_sFrame.ADCReadings[ EADCTYPE_CURRENT1 ].u16Reading;
//	sei();

	// average out the zero value
	// update the buffer - fill or write latest value
	if (0xff == sg_u8CurrentBufferIndex)  // this indicates the buffer is empty, to be filled with first reading)
	{
		while (ADC_CURRENT_BUFFER_SIZE > ++sg_u8CurrentBufferIndex) 
		{
			sg_sCurrenBuffer[sg_u8CurrentBufferIndex] = s16ZeroValue;
		}
	}
	else
	{
		sg_sCurrenBuffer[sg_u8CurrentBufferIndex++] = s16ZeroValue;
		if (ADC_CURRENT_BUFFER_SIZE <= sg_u8CurrentBufferIndex) sg_u8CurrentBufferIndex = 0;
	}
	
	for (i=0, s16ZeroValue=0; ADC_CURRENT_BUFFER_SIZE > i; i++) s16ZeroValue += sg_sCurrenBuffer[i];  // since we are only using 10 bits out of 16, for buffer size of 8 or less it should not overflow
	s16ZeroValue /= ADC_CURRENT_BUFFER_SIZE;
	
	// Subtract the zero-current value from the offset for a signed result
	s32Current = (int32_t)(s16Offset - s16ZeroValue);  // only 10 bits used, so 21 bits of headroom
	
	// GS0 and GS1 are grounded on the ACS37002, so sensitivity is 40 mV/A.
	// outputs are divided in half, so sensitivity at ADC is 20 mV/A with 2.56V 10-bit range or 2.5 mV/bit, giving us 8 bits/A so our resolution is 0.125A per bit.
	// ADC VREF is 2.56V, zero current reference is roughly 1.25V
	// To convert to units of 0.02 amps: multiply by 6.25
	
	//replacing float with integer math
	#define CURRENT_CONVERSION_FACTOR (int32_t)(6.25 * FIXED_POINT_SCALE)  // for increased precision
	int32_t iCurrent = (int32_t)(s32Current * CURRENT_CONVERSION_FACTOR + FIXED_POINT_SCALE/2) / FIXED_POINT_SCALE;
	
	// Add in the current floor in "multiples of 0.02 amps"
 	iCurrent -= (int32_t) (CURRENT_FLOOR / 0.02);  
	
	
	//TODO this will need to be updated once we have ADC running constantly
	sg_sFrame.u16frameCurrent = (uint16_t) iCurrent;
	if (sg_sFrame.u16frameCurrent > sg_sFrame.u16maxCurrent)
	{
		sg_sFrame.u16maxCurrent = sg_sFrame.u16frameCurrent;
	}
	if (sg_sFrame.u16frameCurrent < sg_sFrame.u16minCurrent)
	{
		sg_sFrame.u16minCurrent = sg_sFrame.u16frameCurrent;
	}

}


// Called every time there's an ADC read since apparently ISR can't access global variables?  compiler fails
void ADCCallback(EADCType eType,
				 uint16_t u16Reading)
{
	// End of chain? Indicate it's time for an ADC update
	if ((EADCTYPE_COUNT-1) == eType)
	{
		sg_bADCUpdate = true;
	}
	sg_sFrame.ADCReadings[eType].u16Reading = u16Reading;
	sg_sFrame.ADCReadings[eType].bValid = true;

}



volatile static uint8_t sg_u8Reason;

// Length (in bytes) of the response for controller statuses
#define CAN_STATUS_RESPONSE_SIZE			8

static void ControllerStatusMessagesSend(uint8_t *pu8Response)
{
	// If this is set, send a "request time" command
	if (sg_bSendTimeRequest)
	{
		bool bSent;

		memset((void *) pu8Response, 0, CAN_STATUS_RESPONSE_SIZE);
		
		// Send time request to pack controller
		bSent = CANSendMessage( ECANMessageType_ModuleRequestTime, pu8Response, CAN_STATUS_RESPONSE_SIZE );
			
		if( bSent )
		{
			sg_bSendTimeRequest = false;
		}
	}
	
	// If this is set, send a PKT_MODULE_STATUS1 through _STATUS3
	if( sg_bSendModuleControllerStatus )
	{
		bool bStatusSendSuccessful = true;
		bool bSent;
		
		if (0 == sg_u8ControllerStatusMsgCount)
		{
			uint16_t u16Temp;
				
			// Lower 4 bits is state
			pu8Response[0] = (uint8_t) sg_eModuleControllerStateCurrent & 0x0f;

			// Upper 4 bits is status
			// TODO: Status once details have been filled in
			pu8Response[0] |= ((EMODESTATUS_CHARGE_PROHIBITED_DISCHARGE_PROHIBITED) << 4) & 0xf0;
				
			// 8 Bit state of charge
			pu8Response[1] = sg_u8SOC;
				
			// 8 Bit state of health
			pu8Response[2] = sg_u8SOH;
				
			// 8 Bit expected cell count (comes from EEPROM)
			pu8Response[3] = sg_sFrame.sg_u8CellCountExpected;
				
			// Clear the rest
			memset((void *) &pu8Response[4], 0, CAN_STATUS_RESPONSE_SIZE - 4);
				
			// 16 Bit module current (-655.36A to 655.24A in 0.02A increments)
			if( sg_sFrame.ADCReadings[ EADCTYPE_CURRENT0 ].bValid &&
				sg_sFrame.ADCReadings[ EADCTYPE_CURRENT1 ].bValid &&
				(EMODSTATE_ON == sg_eModuleControllerStateCurrent))
			{
				// update frame with most recent reading
				ModuleCurrentConvertReadings();  //use frame data						
				u16Temp = (uint16_t) sg_sFrame.u16frameCurrent;
			}
			else
			{
				// Return a value of 0.00 for current
				int32_t iCurrent = 0;		// Report 0.0 amps
				// Report 0 amps since we're off, so we don't give "floating" readings			
				// Add in the current floor in "multiples of 0.02 amps"		
				iCurrent -= (int32_t) (CURRENT_FLOOR / 0.02);  
				
				// Finally in units of 0.02 amps
				u16Temp = (uint16_t) iCurrent;				
			}
				
			// Store current value, whatever it may be
			pu8Response[4] = (uint8_t) u16Temp;
			pu8Response[5] = (uint8_t) (u16Temp >> 8);
			
			// 16 Bit voltage in 0.015V increments (0-983.025). Compute the voltage from
			// the string, then check the cells added together. If the cells are lower,
			// go with that reading since the string voltage has a floor.
			{
				uint32_t u32Voltage = 0;  // in case ADC is invalid
				
				if( sg_sFrame.ADCReadings[ EADCTYPE_STRING ].bValid )
				{
				// Analog input is inverted
				u32Voltage = (uint32_t)((1 << ADC_BITS) - 1) - sg_sFrame.ADCReadings[ EADCTYPE_STRING ].u16Reading;
				}
				
				// If our ADC is pegged, it means the cell string is lower voltage than
				// the string input, so return the added-up cell voltage total instead
//				if ((sg_sFrame.sg_u8CellCPUCount == sg_sFrame.sg_u8CellCountExpected) || (0 == u32Voltage))
if(0)
				{
					// if cells ok or ADC slammed - go with the cell voltage total because precision is higher
					// Turn millivolts into 0.015 volt increments
					u32Voltage = (sg_sFrame.sg_u32CellVoltageTotal / 15);
				}
				else
				{
					// something wrong with cells - go with the string voltage via ADC
					
					// Calculate string voltage, value is in corrected ADC bits at this point
					u32Voltage *= sg_sFrame.sg_i16VoltageStringPerADC;  // in fractions of millivolts
					u32Voltage /= ADC_VOLT_FRACTION;  // convert to millivolts 
					u32Voltage += sg_sFrame.sg_i32VoltageStringMin;  // in millivolts
					
					// Convert whatever final voltage we have to 0.015 volt increments
					u32Voltage /= 15;
				}

				sg_sFrame.sg_i32VoltageStringTotal = (int32_t) u32Voltage;
				
				pu8Response[6] = (uint8_t) u32Voltage;
				pu8Response[7] = (uint8_t) (u32Voltage >> 8);
			}
	
			// Send module status1 to the pack controller
			bSent = CANSendMessage( ECANMessageType_ModuleStatus1, pu8Response, CAN_STATUS_RESPONSE_SIZE );
				
			if( false == bSent )
			{
				bStatusSendSuccessful = false;
			}
		}
		else
		if (1 == sg_u8ControllerStatusMsgCount)
		{
			uint16_t u16Voltage;
			
			// 16 Bit lowest cell voltage (0-65.535 volts in millivolt increments)				
			pu8Response[0] = (uint8_t) sg_sFrame.sg_u16LowestCellVoltage;
			pu8Response[1] = (uint8_t) (sg_sFrame.sg_u16LowestCellVoltage >> 8);

			// 16 Bit highest cell voltage (0.65.535 volts in millivolt increments)
			pu8Response[2] = (uint8_t) sg_sFrame.sg_u16HighestCellVoltage;
			pu8Response[3] = (uint8_t) (sg_sFrame.sg_u16HighestCellVoltage >> 8);
				
			// 16 Bit average cell voltage (0.65.535 volts in millivolt increments)
			pu8Response[4] = (uint8_t) sg_sFrame.sg_u16AverageCellVoltage;
			pu8Response[5] = (uint8_t) (sg_sFrame.sg_u16AverageCellVoltage >> 8);
				
			// 16 Bit total cell voltage in 0.015v increments
			u16Voltage = (uint16_t) (sg_sFrame.sg_u32CellVoltageTotal / 15);  //cell voltage total is in mv

			pu8Response[6] = (uint8_t) u16Voltage;
			pu8Response[7] = (uint8_t) (u16Voltage >> 8);

			bSent = CANSendMessage( ECANMessageType_ModuleStatus2, pu8Response, CAN_STATUS_RESPONSE_SIZE );
			
			if( false == bSent )
			{
				bStatusSendSuccessful = false;
			}
			else
			{
			}
		}
		else
		if (2 == sg_u8ControllerStatusMsgCount)
		{
			// 16 Bit lowest cell temperature (-55.35 base + 0.01 degree C increments)
			pu8Response[0] = (uint8_t) sg_sFrame.sg_s16LowestCellTemp;
			pu8Response[1] = (uint8_t) (sg_sFrame.sg_s16LowestCellTemp >> 8);

			// 16 Bit highest cell temperature (-55.35 base + 0.01 degree C increments)
			pu8Response[2] = (uint8_t) sg_sFrame.sg_s16HighestCellTemp;
			pu8Response[3] = (uint8_t) (sg_sFrame.sg_s16HighestCellTemp >> 8);
				
			// 16 Bit average cell temperature (-55.35 base + 0.01 degree C increments)
			pu8Response[4] = (uint8_t) sg_sFrame.sg_s16AverageCellTemp;
			pu8Response[5] = (uint8_t) (sg_sFrame.sg_s16AverageCellTemp >> 8);
				
			// Unused/0
			pu8Response[6] = 0;
			pu8Response[7] = 0;

			bSent = CANSendMessage( ECANMessageType_ModuleStatus3, pu8Response, CAN_STATUS_RESPONSE_SIZE );
				
			if( false == bSent )
			{
				bStatusSendSuccessful = false;
			}
			else
			{
			}
		}

		// Only clear the module controller status if successful
		if( bStatusSendSuccessful )
		{
			++sg_u8ControllerStatusMsgCount;
			if (sg_u8ControllerStatusMsgCount >= 3)
			{
				// Stop status state machine
				sg_u8ControllerStatusMsgCount = 0;
				sg_bSendModuleControllerStatus = false;
				sg_bIgnoreStatusRequests = false;  // Allow new requests again
				
				// Send the comms status
				sg_bSendCellCommStatus = true;
			}
			else
			{
				// Leave the module controller status alone so we pick it up next time
			}
		}
	}
			
	if( sg_bSendCellStatus )
	{
		bool bSuccess;
		uint16_t u16Voltage = 0;
		int16_t s16Temperature = 0;
			
		// Only send data if this cell is in range
		if (sg_u8CellStatus < sg_sFrame.sg_u8CellCPUCount)
		{
			// Lookup current cell details
			// Post-process the cell data to be returned
			CellDataConvert( &sg_sFrame.StringData[sg_u8CellStatus], &u16Voltage, &s16Temperature);  
				
			// First byte is the cell ID
			pu8Response[0] = sg_u8CellStatus;
				
			// Expected total cell count
			pu8Response[1] = sg_sFrame.sg_u8CellCountExpected;
				
			// Cell temperature
				
			pu8Response[2] = (uint8_t) s16Temperature;
			pu8Response[3] = (uint8_t) (s16Temperature >> 8);
				
			// Cell voltage
			pu8Response[4] = (uint8_t) u16Voltage;
			pu8Response[5] = (uint8_t) (u16Voltage >> 8);

// TODO need to write dedicated routines for SOC and SOH, these should also derate max charge and discharge currents
			// State of charge (% up to 4.1 volts)
			pu8Response[6] = (((uint32_t) u16Voltage / (uint32_t) (4100.0)) * (uint32_t) 100.0);
				
			// State of health
			pu8Response[7] = ((uint32_t) u16Voltage / (uint32_t) (sg_sFrame.sg_u16HighestCellVoltage - sg_sFrame.sg_u16LowestCellVoltage) * (uint32_t) 100.0);
				
			// Reply with a cell detail
			bSuccess = CANSendMessage( ECANMessageType_ModuleCellDetail, pu8Response, CAN_STATUS_RESPONSE_SIZE );
			if( bSuccess )
			{
				// Success! Advance to the next cell (if applicable) and shut off
				// transmission if we've reached our target.
				sg_u8CellStatus++;
				if (sg_u8CellStatus >= sg_u8CellStatusTarget)
				{
					sg_bSendCellStatus = false;
				}
			}
		}
		else
		{
			// We're beyond the available cells we have - just set to 0
			sg_u8CellStatusTarget = 0;
			sg_u8CellStatus = 0;
			sg_bSendCellStatus = false;
		}
	}
		
	// If we need to send the cell comm stats, do so
	if( sg_bSendCellCommStatus )
	{
		bool bSuccess;

		// Fewest/most # of cells received
		pu8Response[0] = sg_sFrame.sg_u8CellCPUCountFewest;
		pu8Response[1] = sg_sFrame.sg_u8CellCPUCountMost;
				
		// I2C Faults
		pu8Response[2] = (uint8_t) sg_sFrame.sg_u16CellCPUI2CErrors;
		pu8Response[3] = (uint8_t) (sg_sFrame.sg_u16CellCPUI2CErrors >> 8);
				
		// MC RX framing errors
		pu8Response[4] = sg_sFrame.sg_u8MCRXFramingErrors;

		// Which first cell # is generating an I2C error?
		
		if (sg_sFrame.sg_u8CellFirstI2CError != 0xff)
		{
			// Need to invert it since the oldest is at the start of the list
			pu8Response[5] = (sg_sFrame.sg_u8CellCPUCount - sg_sFrame.sg_u8CellFirstI2CError); // - 1;
		}
		else
		{
			// None are in error
			pu8Response[5] = 0xff;
		}
		
		pu8Response[6] = 0;
		pu8Response[7] = 0;
			
		bSuccess = CANSendMessage( ECANMessageType_ModuleCellCommStat1, pu8Response, CAN_STATUS_RESPONSE_SIZE );

		// If we've successfully sent the communication stats, don't send it again
		if (bSuccess)
		{
			sg_bSendCellCommStatus = false;
		}
	}

	// If we want the hardware detail, send it.
	if (sg_bSendHardwareDetail)
	{
		bool bSuccess;
	
		// Get current thresholds
		CurrentThresholdsGet((uint16_t *) (pu8Response + sizeof(uint16_t)),
							 (uint16_t *) (pu8Response));
				
		// 16 Bit maximum end of charge voltage
		pu8Response[4] = 0;
		pu8Response[5] = 0;
				
		// 16 Bit hardware compatibility fields
		pu8Response[6] = (uint8_t) HARDWARE_COMPATIBILITY;
		pu8Response[7] = (uint8_t) (HARDWARE_COMPATIBILITY >> 8);
				
		bSuccess = CANSendMessage( ECANMessageType_ModuleHardwareDetail, pu8Response, CAN_STATUS_RESPONSE_SIZE );

		// If we've successfully sent the hardware detail, don't send it again				
		if (bSuccess)
		{
			sg_bSendHardwareDetail = false;
		}
	}
}




static void CellStringProcess(uint8_t *pu8Response)  // no longer does float calcs on every cell, doesn't do anything with pu8Response
{
	// If nothing to do, don't do anything
	if (!sg_sFrame.sg_u16BytesReceived)  // didn't get any new bytes
	{
		return;
	}


	// Assume there are no I2C errors...
	sg_sFrame.sg_u8CellFirstI2CError = 0xff;

	// Update comm stats
	if (sg_sFrame.sg_u8CellCPUCountFewest > sg_sFrame.sg_u8CellCPUCount)
	{
		sg_sFrame.sg_u8CellCPUCountFewest = sg_sFrame.sg_u8CellCPUCount;
		sg_bSendCellCommStatus = true;
	}
	if (sg_sFrame.sg_u8CellCPUCountMost < sg_sFrame.sg_u8CellCPUCount)
	{
		sg_sFrame.sg_u8CellCPUCountMost = sg_sFrame.sg_u8CellCPUCount;
		sg_bSendCellCommStatus = true;
	}


	// OK, we have at least one. Let's see if the cell count is
	// reasonable. If it isn't, flag it as a framing error
	if (sg_sFrame.sg_u16BytesReceived & (((1 << BYTES_PER_CELL_SHIFT) - 1))) // number of bytes not evenly divisible by bytes per cell
	{
		if (sg_sFrame.sg_u8MCRXFramingErrors != 0xff)
		{
			sg_sFrame.sg_u8MCRXFramingErrors++;
			sg_bSendCellCommStatus = true;
		}
	}

	// Find the highest and lowest temperatures and voltages for this batch of cell data
	sg_sFrame.sg_u16HighestCellVoltage = 0;
	sg_sFrame.sg_u16LowestCellVoltage  = 0xffff;
	sg_sFrame.sg_u16AverageCellVoltage = 0;
	sg_sFrame.sg_s16HighestCellTemp = -32768;
	sg_sFrame.sg_s16LowestCellTemp = 32767;
	sg_sFrame.sg_s16AverageCellTemp = 0;
	int16_t s16HighestCellTempRAW = -32768;  // keep stats in RAW
	int16_t s16LowestCellTempRAW = 32767;
	int32_t s32TempTotal = 0;
	uint32_t u32CellVoltageTotalmV = 0;		// All cell CPU voltages added up, RAW
	uint8_t u8CellVoltageCount = 0;
	uint8_t u8CellTemperatureCount = 0;

	for (uint8_t u8CellIndex = 0; u8CellIndex < sg_sFrame.sg_u8CellCPUCount; u8CellIndex++)  // process however many cells we received
	{
		uint16_t u16Voltage = sg_sFrame.StringData[u8CellIndex].voltage;
		int16_t s16Temperature = sg_sFrame.StringData[u8CellIndex].temperature;
		
		// Bits 0-3  - 16ths of a degree C
		// Bits 4-11 - Whole degrees C
		// Bit 12    - Temperature sign bit - 0=Positive, 1=Negative



		// if valid, count it
		if (CellDataConvertTemperature(s16Temperature,NULL))  // only do validity check, don't convert since we're doing stats raw
		{
			if (s16Temperature & (1 << 12))
			{
				// Sign extend/2's complement
				s16Temperature |= 0xf000;
			}
			else
			{
				s16Temperature &= ~MSG_CELL_TEMP_I2C_OK;  // clear the i2c bit
			}

			if (s16HighestCellTempRAW < s16Temperature)
			{
				s16HighestCellTempRAW = s16Temperature;
			}
			if (s16LowestCellTempRAW > s16Temperature)
			{
				s16LowestCellTempRAW = s16Temperature;
			}
			s32TempTotal += s16Temperature;
			u8CellTemperatureCount++;
		}

		// Is it discharging?
		if (sg_sFrame.StringData[u8CellIndex].voltage & MSG_CELL_DISCHARGE_ACTIVE)
		{
			// update the flag
			sg_sFrame.bDischargeOn = true;
		}	
		
		// if valid, count it		
		if (CellDataConvertVoltage(u16Voltage,&u16Voltage))  // this will check limits, scale to mV and clear unused bits
		{
			if (sg_sFrame.sg_u16HighestCellVoltage < u16Voltage)
			{
				sg_sFrame.sg_u16HighestCellVoltage = u16Voltage;
			}
			if (sg_sFrame.sg_u16LowestCellVoltage > u16Voltage)
			{
				sg_sFrame.sg_u16LowestCellVoltage = u16Voltage;
			}
			u32CellVoltageTotalmV += u16Voltage;
			u8CellVoltageCount++;
		}
	}

	if (u8CellVoltageCount)  // got at least some
	{
		sg_sFrame.sg_u32CellVoltageTotal = u32CellVoltageTotalmV;
		sg_sFrame.sg_u16AverageCellVoltage = (uint16_t)(sg_sFrame.sg_u32CellVoltageTotal / u8CellVoltageCount);

		if ((EMODSTATE_ON != sg_eModuleControllerStateCurrent) &&
		(false == sg_bCellBalancedOnce) &&
		(sg_sFrame.sg_u16HighestCellVoltage >= sg_sFrame.sg_u16LowestCellVoltage) &&
		((sg_sFrame.sg_u16HighestCellVoltage - sg_sFrame.sg_u16LowestCellVoltage) >= BALANCE_VOLTAGE_THRESHOLD))
		{
			sg_bCellBalanceReady = true;
		}
	}

	if (u8CellTemperatureCount)  // got at least some
	{
		int16_t s16AverageTempRAW = (int16_t)(s32TempTotal / u8CellTemperatureCount);
		CellDataConvertTemperature(s16AverageTempRAW,&sg_sFrame.sg_s16AverageCellTemp);
		CellDataConvertTemperature(s16HighestCellTempRAW,&sg_sFrame.sg_s16HighestCellTemp);
		CellDataConvertTemperature(s16LowestCellTempRAW,&sg_sFrame.sg_s16LowestCellTemp);
	}


	// Write the cell data out to file
	if ((sg_bSDCardReady) && (EMODSTATE_OFF != sg_eModuleControllerStateCurrent))
	{
		sg_bSDCardReady = STORE_WriteFrame(&sg_sFrame);  
	}

	// Send out a status
	SendModuleControllerStatus();


}

void FrameInit(bool  bFullInit)  // receives true if full init is needed, false if only cell reading data is to be reset
{
		// initialize frame structure
		if (bFullInit || (FRAME_VALID_SIG != sg_sFrame.validSig)) // full init or invalid frame
		{
			memset(&sg_sFrame,0,sizeof(sg_sFrame)); // set all to 0
			sg_sFrame.frameBytes = sizeof(sg_sFrame);
			sg_sFrame.validSig = FRAME_VALID_SIG;
			sg_sFrame.moduleUniqueId = ModuleControllerGetUniqueID();
			sg_sFrame.sg_u8CellFirstI2CError = 0xff;
			sg_sFrame.sg_u8CellCPUCountFewest = 0xff;
			CellCountExpectedSet(EEPROMRead(EEPROM_EXPECTED_CELL_COUNT));
		}
		else  // do only if partial init, in full init the memset takes care of all this
		{
			sg_sFrame.sg_u32CellVoltageTotal = 0;		// All cell CPU voltages added up (in millivolts)
			sg_sFrame.sg_u16HighestCellVoltage = 0;
			sg_sFrame.sg_u16LowestCellVoltage = 0;
			sg_sFrame.sg_u16AverageCellVoltage = 0;
			memset(&sg_sFrame.StringData,0,sizeof(sg_sFrame.StringData)); // set all to 0
			sg_sFrame.bDischargeOn = false;
			sg_sFrame.sg_u16CellCPUI2CErrors = 0;
			sg_sFrame.sg_u8CellFirstI2CError = 0;
			sg_sFrame.sg_u8CellCPUCount = 0;     // for the frame
			sg_sFrame.sg_u8MCRXFramingErrors = 0;
		}
		
		sg_sFrame.sg_s16HighestCellTemp = TEMPERATURE_BASE;	// 0C default, in case we get no readings
		sg_sFrame.sg_s16LowestCellTemp = TEMPERATURE_BASE;		// 0C default, in case we get no readings
		sg_sFrame.sg_s16AverageCellTemp = TEMPERATURE_BASE;	// 0C default, in case we get no readings
		
		// initialize other housekeeping
		sg_u8CurrentBufferIndex=0xff;  // this indicates the buffer is empty, to be filled with first reading
	
}

int main(void)
{
	
	// Always disable the watchdog on startup for safety (see datasheet)
	WatchdogOff();
	
	// Enable FET and Relay enable outputs
	FET_EN_CONFIGURE();  // DO THE FET FIRST
	FET_EN_DEASSERT();   // MAKE SURE IT'S OFF
	
	RELAY_EN_CONFIGURE();
	RELAY_EN_DEASSERT();

	// Set cell power port/pin as an output
	DDR_CELL_POWER |= (1 << PIN_CELL_POWER);	
	
	// Turn off the cell chain
	CELL_POWER_DEASSERT();
	
	uint8_t* memptr;
	for (memptr=(uint8_t*)0x0800; memptr < (uint8_t*)0x1000; memptr++) *memptr = 0xaa;  // data ends at 0x679, fill a chunk with 0xaa to see if stack is clobbering it
	
	//OSCCAL = 0x62;  // *dp try different cal value, default is 0x60
		
	sg_u8Reason = MCUSR;
	if ((1 << WDRF) & sg_u8Reason)
	{
		// Watchdog reset, most likely caused by power/reset glitch when switching relays
		// determine what we were doing and tweak as needed
		switch (sg_eWDTCurrentStatus)  // it is in .noinit so should survive the reset
		{		
			case EWDT_NORMAL:
			{
				// shoould not occur, but if it did try to continue like nothing happened
			}
			case EWDT_MECH_RLY_ON:
			{
				// we were switching mech relay on
			}
			case EWDT_MECH_RLY_OFF:
			{
				// we were switching mech relay off
			}
			case EWDT_FET_ON:
			{
				// we were switching fet on, add overcurrent handling here  *dp
			}
			case EWDT_FET_OFF:
			{
				// we were switching fet off
			}
		}
		sg_sFrame.sg_u8WDTCount++;
		WDTSetLeash(WDT_LEASH_LONG, EWDT_NORMAL);  // set on long leash
		ModuleControllerStateHandle();  // finish what we were doing
	}
	else
	{
		// all other reset types, do normal init		
		if ((1 << BORF) & sg_u8Reason)
		{
			// Brown out reset
		}
		if ((1 << EXTRF) & sg_u8Reason)
		{
			// External reset
		}
		if ((1 << PORF) & sg_u8Reason)
		{
			// Power on reset
		}
		// Stop all interrupts
		cli();
		// Set the sysclock to 8 MHz
		// *dp some of these may also need to be done on WDT
		SetSysclock();
		TimerInit();
		vUARTInit();
		ADCInit();
	
#ifndef STATE_CYCLE		// don't attempt SD when cycling
		sg_bSDCardReady = STORE_Init();
#else
		sg_bSDCardReady = false;  
		sg_eStateCycle = EMODSTATE_OFF;
		sg_u8StateCounter = 0;
#endif
	
		// Set how many cells we're expecting	
	
		FrameInit(true);  // true for full init
		
	
		// And how many sequential incorrect cell count until we reset the
		// chain?
		sg_u8SequentailCountMismatchThreshold = EEPROMRead(EEPROM_SEQUENTIAL_COUNT_MISMATCH);

		// Cache the unique ID from EEPROM to avoid repeated reads during message formation

		// Set 5V DET as input
		DDR_5V_DET &= (uint8_t) ~(1 << PIN_5V_DET);
		// And PIN_5V_DET pullup
		PORT_5V_DET |= (1 << PIN_5V_DET);
	
		// Clear pin change interrupt flag by writing a one
		PCIFR = (1 << PCIF0);			// Bank A flag
		PCIFR = (1 << PCIF1);			// Bank B flag
		PCIFR = (1 << PCIF2);			// Bank C flag
		PCIFR = (1 << PCIF3);			// Bank D flag

		// Enable 5V loss interrupt
		IMASK_5V_DET |= (1 << PCINT_5V_DET);
	
		// Pin change interrupt - enable it
		PCIFR |= (1 << PCIFR_5V_DET);
	
		// Set the CAN bus receive callback and init it
		CANSetRXCallback(CANReceiveCallback);
		CANInit();
	
	
//		DebugOut("ModBatt: Module Controller\n");
	
		(void) RTCInit();
	
		sg_eModuleControllerStateCurrent = EMODSTATE_INIT;
		sg_eModuleControllerStateTarget = STATE_DEFAULT;
		sg_eModuleControllerStateMax = EMODSTATE_OFF;  // Initialize to OFF to prevent relay turning on at startup
		sg_bModuleRegistered = false;  // Initialize to false to prevent relay/FET activation without registration
		WDTSetLeash(WDT_LEASH_LONG, EWDT_NORMAL);  // set on long leash
		sg_eFrameStatus = EFRAMETYPE_WRITE; // start on a write so that housekeeping gets done
	}
	
		
	// Enable all interrupts!
	sei();
	
	while(1)
	{
		WatchdogReset();
		if (sg_bNewTick)
		{
			sg_bNewTick = false;  // set tru in periodic tick isr, cleared in main loop
			uint8_t u8Reply[CAN_STATUS_RESPONSE_SIZE];

			// Clear separation of registered vs unregistered behavior
			if (!sg_bModuleRegistered)
			{
				// Not registered - handle announcement delays
				if (sg_bAnnouncementPending)
				{
					if (sg_u8AnnouncementDelayTicks > 0)
					{
						sg_u8AnnouncementDelayTicks--;
					}
					
					if (sg_u8AnnouncementDelayTicks == 0)
					{
						// Time to send the announcement
						sg_bSendAnnouncement = true;
						sg_bAnnouncementPending = false;
						DebugOut("Announcement delay complete - sending\r\n");
					}
				}
			}
			else
			{
				// Registered - handle normal operations
				// Send controller status messages
				ControllerStatusMessagesSend(u8Reply);
			}


		
			// Check for a pack controller timeout.  Reset our ID if we've lost contact.
			if( sg_bPackControllerTimeout )
			{
				sg_bPackControllerTimeout = false;
				sg_u8ModuleRegistrationID = 0;
			
// 				savedCANGIE = CANGIE;  // these are used to temporarily disable CAN ints
// 				CANGIE &= ~(1<< ENIT);
				sg_bModuleRegistered = false;
				sg_bIgnoreStatusRequests = false;  // Reset all status flags
// 				CANGIE = savedCANGIE;			
			
				sg_bSendAnnouncement = true;			// Send an announcement to hasten registration
			
				// Don't send statuses in case they're queued up - we've lost connection
				// to the pack controller.
				SendModuleControllerStatus();
				#ifndef STATE_CYCLE  //ignore in cycle mode
				// Can't hear from the pack controller! Shut it down!
				ModuleControllerStateSet( EMODSTATE_OFF );
				#endif
			}

	//------------------------- section specific to WRITE frame --------------------------------

			uint8_t savedTIMSK1 = TIMSK1;  // disable timer int to preserve state
			TIMSK1 &= ~(1 << OCIE1A);
			EFrameType eCurrentFrame = sg_eFrameStatus;
			bool bFrameStart = sg_bFrameStart;	
			TIMSK1 = savedTIMSK1;
		
			if (EFRAMETYPE_WRITE == eCurrentFrame)  // only do these ops if vUART is idle!
			{
				if(bFrameStart) // start of WRITE frame, reference local variable saved earlier
				{
					sg_bFrameStart = false;
					CellStringPowerStateMachine(); // if we just turned off the string it will clear out frame
				
					vUARTRXEnd();  // wrap up previous read
					CellStringProcess(u8Reply);  // get it processed


					if (ESTRING_OPERATIONAL == sg_eStringPowerState)
					{
						// We're operational. If we didn't get the number of cells we expect
						// increment sg_u8SequentailCellCountMismatches. If this exceeds
						// sg_u8SequentailCountMismatchThreshold, reset the chain. Setting to 0 will
						// disable the cell string reset.
						if ((sg_sFrame.sg_u8CellCPUCount != sg_sFrame.sg_u8CellCountExpected) &&
						(sg_sFrame.sg_u8CellCountExpected))
						{
							if ((0 != sg_u8SequentailCountMismatchThreshold) &&  // feature disabled in EEPROM
							(0xff != sg_u8SequentailCountMismatchThreshold))  // unprogrammed EEPROM
							{
								++sg_u8SequentailCellCountMismatches;
								if ((sg_u8SequentailCellCountMismatches >= sg_u8SequentailCountMismatchThreshold))
								{
									sg_eStringPowerState = ESTRING_OFF;  // this will turn string off on the start of read frame
									sg_u8SequentailCellCountMismatches = 0; // reset the timer
								}
							}
						}
						else
						{
							// All good
							// Clear running count since we got a good read
							sg_u8SequentailCellCountMismatches = 0;
						}
					}
					
					if( sg_bSendAnnouncement )  //we should announce ourselves
					{
						bool bSent;

						// Reply with general status
						u8Reply[0] = (uint8_t) FW_BUILD_NUMBER;
						u8Reply[1] = (uint8_t) (FW_BUILD_NUMBER >> 8);
						u8Reply[2] = MANUFACTURE_ID;
						u8Reply[3] = PART_ID;
						*((uint32_t*)&u8Reply[4]) = sg_sFrame.moduleUniqueId;
			
						bSent = CANSendMessage( ECANMessageType_ModuleAnnouncement, u8Reply, CAN_STATUS_RESPONSE_SIZE );

						if( bSent )
						{
							sg_bSendAnnouncement = false;
						}
					// Send a module announcement if unregistered
		
					}
					// Only send the announcement if the module isn't registered
					// disable can interrupts as they seem to result in misreading of status
					
	
				}  //end of write frame start code
			
				// the following is done continuously while in WRITE frame

				// Check for a state transition and handle it
				ModuleControllerStateHandle();
		
				// If we have an overcurrent condition, send a CAN message
				//*dp
				if (sg_bOvercurrentSignal)
				{
					sg_bOvercurrentSignal = false;
				}


				// This code will cycle through all of the states once every
				// STATE_CYCLE_INTERVAL frames
	#ifdef STATE_CYCLE
				if(bFrameStart) // Only increment once per frame, not continuously
				{
					sg_u8StateCounter++;
					if (sg_u8StateCounter >= STATE_CYCLE_INTERVAL)
					{
						sg_u8StateCounter = 0;

						if (sg_eStateCycle >= EMODSTATE_ON)
						{
							sg_eStateCycle = EMODSTATE_OFF;  //turn off sequence and delays handled by state handler
						}
						else
						{
							sg_eStateCycle++;
						} 

						// Now set the new state
						ModuleControllerStateSet(sg_eStateCycle);
					}
				}
	#endif
		
			}
	//------------------------- section specific to READ frame --------------------------------
			else  // we are in READ frame
			{
				if(bFrameStart) // start of READ frame, reference local variable saved earlier
				{
					sg_bFrameStart = false;
					CellStringPowerStateMachine(); // if we just turned off the string in write frame, it will take effect here
					
					FrameInit(false);  // init frame data
					
					if (ESTRING_OPERATIONAL == sg_eStringPowerState)  //only do this if we are up and running
					{
						
		#ifdef FAKE_CELL_DATA   // fake it
						uint8_t *pu8Dest = sg_sFrame.StringData;
						const uint8_t *pu8Src = (const uint8_t *) sg_u16FakeCellData;;
						uint16_t u16Count = sizeof(sg_u16FakeCellData);

						// This is done explicitly because it's copying from code space into data space
						// and memcpy() doesn't deal with the difference.
						while (u16Count--)
						{
							*pu8Dest = *pu8Src;
							++pu8Dest;
							++pu8Src;
						}

						sg_sFrame.sg_u16BytesReceived = sizeof(sg_u16FakeCellData);
						sg_sFrame.sg_u8CellCPUCount = sizeof(sg_u16FakeCellData) >> 2;	// Each cell report is 4 bytes
		#else  // make it
						// Initialize receive capability
						vUARTInitReceive();
						// Clear receive state machine - using reset instead of start clears the state to ESTATE_IDLE
						vUARTRXReset();
						// Start a request for data to the cell CPUs.
						vUARTStarttx();  //requesting cell data
		#endif
					}

				}
			}  // end of frame-specific section
	//------------------------- end of frame specific section --------------------------------

			// If completed all the readings, write to frame
			if (sg_bADCUpdate)
			{
				sg_bADCUpdate = false;
				ModuleCurrentConvertReadings();  //now updates frame
			}
			// Read all the ADCs
			ADCStartConversion();	// will only start if not busy		
		}  // end of tick processing
		
	}
}


// This variable will be set to indicate which unhandled interrupt vector was hit
static volatile uint8_t sg_u8UnhandledInterruptVector;
static volatile uint8_t sg_u8PCMSK0;
static volatile uint8_t sg_u8PCMSK1;

// NOTE: If you're getting unexpected AVR resets, uncomment these unused handlers to see
// if another interrupt is being taken that doesn't have a handler. AVR Defaults to jumping
// to 0 when not vector is programmed.
ISR(ANACOMP0_vect, ISR_BLOCK)
{
	sg_u8UnhandledInterruptVector = (uint8_t) ANACOMP0_vect;
	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
	while (1);
}

ISR(ANACOMP1_vect, ISR_BLOCK)
{
	sg_u8UnhandledInterruptVector = (uint8_t) ANACOMP1_vect;
	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
	while (1);
}

ISR(ANACOMP2_vect, ISR_BLOCK)
{
	sg_u8UnhandledInterruptVector = (uint8_t) ANACOMP2_vect;
	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
	while (1);
}

ISR(ANACOMP3_vect, ISR_BLOCK)
{
	sg_u8UnhandledInterruptVector = (uint8_t) ANACOMP3_vect;
	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
	while (1);
}

ISR(PSC_FAULT_vect, ISR_BLOCK)
{
	sg_u8UnhandledInterruptVector = (uint8_t) PSC_FAULT_vect;
	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
	while (1);
}

ISR(PSC_EC_vect, ISR_BLOCK)
{
	sg_u8UnhandledInterruptVector = (uint8_t) PSC_EC_vect;
	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
	while (1);
}

ISR(PCINT0_vect, ISR_BLOCK)
{
	sg_u8UnhandledInterruptVector = (uint8_t) PCINT0_vect;
	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
	while (1);
}
/*
// 
// ISR(INT1_vect, ISR_BLOCK)
// {
// 	sg_u8UnhandledInterruptVector = (uint8_t) INT0_vect;
// 	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
// 	while (1);
// }
*/

ISR(INT2_vect, ISR_BLOCK)
{
	sg_u8UnhandledInterruptVector = (uint8_t) INT2_vect;
	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
	while (1);
}

// ISR(INT3_vect, ISR_BLOCK)
// {
// 	sg_u8UnhandledInterruptVector = (uint8_t) INT3_vect;
// 	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
// 	while (1);
// }


ISR(TIMER1_CAPT_vect, ISR_BLOCK)
{
	sg_u8UnhandledInterruptVector = (uint8_t) TIMER1_CAPT_vect;
	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
	while (1);
}
// 
// ISR(TIMER0_COMPB_vect, ISR_BLOCK)
// {
// 	sg_u8UnhandledInterruptVector = (uint8_t) TIMER0_COMPB_vect;
// 	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
// 	while (1);
// }


ISR(TIMER1_COMPB_vect, ISR_BLOCK)
{
	sg_u8UnhandledInterruptVector = (uint8_t) TIMER1_COMPB_vect;
	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
	while (1);
}


ISR(TIMER1_OVF_vect, ISR_BLOCK)
{
	sg_u8UnhandledInterruptVector = (uint8_t) TIMER1_OVF_vect;
	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
	while (1);
}

ISR(TIMER0_OVF_vect, ISR_BLOCK)
{
	sg_u8UnhandledInterruptVector = (uint8_t) TIMER0_OVF_vect;
	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
	while (1);
}

ISR(CAN_TOVF_vect, ISR_BLOCK)
{
	sg_u8UnhandledInterruptVector = (uint8_t) CAN_TOVF_vect;
	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
	while (1);
}

ISR(LIN_ERR_vect, ISR_BLOCK)
{
	sg_u8UnhandledInterruptVector = (uint8_t) LIN_ERR_vect;
	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
	while (1);
}

// 
// ISR(PCINT2_vect, ISR_BLOCK)
// {
// 	sg_u8UnhandledInterruptVector = (uint8_t) PCINT2_vect;
// 	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
// 	while (1);
// }


ISR(PCINT3_vect, ISR_BLOCK)
{
	sg_u8UnhandledInterruptVector = (uint8_t) PCINT3_vect;
	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
	while (1);
}


ISR(SPI_STC_vect, ISR_BLOCK)
{
	sg_u8UnhandledInterruptVector = (uint8_t) SPI_STC_vect;
	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
	while (1);
}

ISR(EE_READY_vect, ISR_BLOCK)
{
	sg_u8UnhandledInterruptVector = (uint8_t) EE_READY_vect;
	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
	while (1);
}

ISR(SPM_READY_vect, ISR_BLOCK)
{
	sg_u8UnhandledInterruptVector = (uint8_t) SPM_READY_vect;
	sg_u8PCMSK0 = PCMSK0; sg_u8PCMSK1 = PCMSK1;
	while (1);
}
