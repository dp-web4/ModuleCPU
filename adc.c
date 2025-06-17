/* 
 * Module controller firmware - ADC
 *
 * This module contains the ADC functionality. 
 *
 */

#include <stdint.h>
#include <xc.h>
#include <stdbool.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

#include "main.h"
#include "adc.h"
#include "STORE.h"

#define MUX_MASK			((uint8_t) ~((1 << MUX0) | (1 << MUX1) | (1 << MUX2) | (1 << MUX3) | (1 << MUX4)))
#define MUX_AREF			((1 << REFS1) | (1 << REFS0))

volatile static EADCState sg_eState = EADCSTATE_INIT;
static EADCType sg_eCurrentType = EADCTYPE_FIRST;


typedef struct
{
	uint8_t u8MuxSelect;
	uint8_t u8DDRPort;
	uint8_t u8Bit;
} SMuxSelect;

#define MUX_PORTA		0
#define MUX_PORTB		1
#define MUX_PORTC		2
#define MUX_PORTD		3

// Table of MUXes for each channel + data port so it can be set as an input 
// and the pullup disabled
static const SMuxSelect sg_sMuxSelectList[ EADCTYPE_COUNT ] = 
{
	{((1 << MUX3) | (1 << MUX0)),				MUX_PORTC,	PORTC5},	// PC5 - ADC9 - EADCTYPE_STRING
	{((1 << MUX2) | (1 << MUX1)),				MUX_PORTB,	PORTB5},	// PB5 - ADC6 - EADCTYPE_CURRENT0
	{((1 << MUX2) | (1 << MUX1) | (1 << MUX0)),	MUX_PORTB,	PORTB6},	// PB6 - ADC7 - EADCTYPE_CURRENT1
	{((1 << MUX3) | (1 << MUX1) | (1 << MUX0)),	MUX_PORTA,	0},	// EADCTYPE_TEMP - first conversion invalid due to driver propagation 2uS
	{((1 << MUX3) | (1 << MUX1) | (1 << MUX0)),	MUX_PORTA,	0},	// EADCTYPE_TEMP - actual reading
};


// Conversion complete interrupt handler
ISR(ADC_vect, ISR_NOBLOCK)
{
	uint16_t u16ADCValue = ADC;  // get the value with existing mux setting
	EADCType eTypePrior = sg_eCurrentType;  // this is what we read
	sg_eCurrentType++;   // set up for next one
	if( sg_eCurrentType >= EADCTYPE_COUNT)  // this was the last conversion, set up for next batch
	{
		sg_eCurrentType = EADCTYPE_FIRST;
	}	
	// Set the MUX for next conversion
	ADMUX = (ADMUX & MUX_MASK) | (sg_sMuxSelectList[sg_eCurrentType].u8MuxSelect) | MUX_AREF;
	
	// write the current reading to frame
	ADCCallback(eTypePrior, u16ADCValue);
					
	// See if we start another conversion or we're done with conversions
	if(EADCTYPE_FIRST ==  sg_eCurrentType)
	{
		// Disable the conversion complete interrupt
		ADCSRA &= (uint8_t)~(1 << ADIE);

		sg_eState = EADCSTATE_IDLE;
	}
	else
	{
		// Start a single conversion and interrupt when complete
		ADCSRA |= (1 << ADSC) | (1 << ADIE);
	}		
}

// Enable ADC
void ADCSetPowerOn( void )
{
	ADCSRA |= (1 << ADEN);
}

void ADCSetPowerOff( void )
{
	// Wait for things to go idle
	while (sg_eState != EADCSTATE_IDLE);
	
	// Disable ADC and set to idle
	ADCSRA &= ((uint8_t) ~(1 << ADEN));
	sg_eState = EADCSTATE_IDLE;
		
	// Set back to the first channel
	sg_eCurrentType = EADCTYPE_FIRST;

	// Disable the conversion complete interrupt
	ADCSRA &= (uint8_t)~(1 << ADIE);

	// Clear any pending interrupt	
	ADCSRA |= (1 << ADIF);
}

void ADCStartConversion(void)
{
	// Don't start anything if we're not idle
	if (EADCSTATE_IDLE != sg_eState)
	{
		return;
	}
	
	// Set back to the first channel
	sg_eCurrentType = EADCTYPE_FIRST;
	
	// Set the MUX
	ADMUX = (ADMUX & MUX_MASK) | (sg_sMuxSelectList[sg_eCurrentType].u8MuxSelect) | MUX_AREF;
		
	sg_eState = EADCSTATE_READING;
	
	// Start a single conversion  - *dp run one at a time for now instead of continuous
	ADCSRA |= (1 << ADSC) | (1 << ADIE) | (1 << ADEN);  // turn power on if it isn't
}

void ADCInit( void )
{
	uint8_t u8Loop;

    // Set the prescaler to clock/4 (With 8Mhz system clock = 2Mhz ADC clock)
    ADCSRA = (1 << ADPS1);
    
    // Turn off ADLAR (ADC left adjust result) and set to external reference
    ADMUX = 0;
	
	// Turn off all 4 analog comparator channels
	AC0CON = 0;
	AC1CON = 0;
	AC2CON = 0;
	AC3CON = 0;

	// Enable the external reference AREF pin and high speed mode and clear current source enable
	ADCSRB = (1 << AREFEN) | (1 << ADHSM);

	// Run through all analog inputs and set their GPIO as input and no pullup
	for (u8Loop = 0; u8Loop < (sizeof(sg_sMuxSelectList) / sizeof(sg_sMuxSelectList[0])); u8Loop++)
	{
		switch (sg_sMuxSelectList[u8Loop].u8DDRPort)
		{
			// Each of these set the GPIO as an input and turn off the pullup
			case MUX_PORTA:  //internal temp sensor
			{
				break;
			}
			case MUX_PORTB:
			{
				DDRB &= (uint8_t) ~(1 << sg_sMuxSelectList[u8Loop].u8Bit);
				PORTB &= (uint8_t) ~(1 << sg_sMuxSelectList[u8Loop].u8Bit);
				break;
			}
			
			case MUX_PORTC:
			{
				DDRC &= (uint8_t) ~(1 << sg_sMuxSelectList[u8Loop].u8Bit);
				PORTC &= (uint8_t) ~(1 << sg_sMuxSelectList[u8Loop].u8Bit);
				break;
			}
			
			case MUX_PORTD:
			{
				DDRD &= (uint8_t) ~(1 << sg_sMuxSelectList[u8Loop].u8Bit);
				PORTD &= (uint8_t) ~(1 << sg_sMuxSelectList[u8Loop].u8Bit);
				break;
			}
			
			default:
			{
				// This means there's a definition in the table that the code doesn't understand
				MBASSERT(0);
			}
		}
	}
	
	sg_eState = EADCSTATE_IDLE;
}
