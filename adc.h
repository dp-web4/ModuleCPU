#ifndef _ADC_H_
#define _ADC_H_

// How many bits is our ADC?
#define ADC_BITS			10
#define TEMPERATURE_BASE	5535
#define ADC_VOLT_FRACTION 128 // fractions of millivolts for total voltage adc conversion

typedef enum
{
	// First element below should equal EADCTYPE_FIRST's value
	EADCTYPE_STRING =  0,				//STRING VOLTAGE
	EADCTYPE_CURRENT0 = 1,				//CURRENT 
	EADCTYPE_CURRENT1 = 2,				//CURRENT REF
	EADCTYPE_TEMP_DELAY = 3,			//INTERNAL TEMP SENSOR DUMMY READING FOR DRIVER PROPAGATION 2uS
	EADCTYPE_TEMP = 4,					//INTERNAL TEMP SENSOR

	// Leave this last	
	EADCTYPE_COUNT
} EADCType;

#define EADCTYPE_FIRST EADCTYPE_STRING

typedef enum
{
	EADCSTATE_INIT = 0,
	EADCSTATE_IDLE,
	EADCSTATE_READING,
	
	EADCSTATE_MAX
} EADCState;

extern void ADCSetPowerOn( void );
extern void ADCSetPowerOff( void );
extern void ADCStartConversion(void);
extern void ADCInit( void );

#endif