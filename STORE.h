#ifndef _STORE_H_
#define _STORE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // For offsetof macro
#include <adc.h>
#include <vUART.h>

// Define constants
#define SECTOR_SIZE 512
#define MAX_CELLS 108  // this may change later - dp




// Global state structure (Sector 0) - MUST BE 4-BYTE ALIGNED FOR 32-BIT XFERS
typedef struct __attribute__((aligned(4))) {
	uint64_t lastUpdateTimestamp;
	uint32_t checksum;
	uint32_t firstSessionSector;
	uint32_t lastSessionSector;
	uint32_t sessionCount;
	uint32_t newSessionSector;
	uint32_t activeSessionMapSector;
	uint32_t activeSessionMapOffset;
	uint8_t cellDataDescriptor[16];  // Placeholder size, adjust as needed
	uint8_t frameDataDescriptor[32]; // Placeholder size, adjust as needed
	uint8_t cellCount;
	uint8_t cellStructuresPerFrame;
	uint8_t lifetimeStats[384];      // Placeholder size, adjust as needed
} GlobalState;

// Cell data structure - MUST BE 4-BYTE ALIGNED FOR 32-BIT XFERS
typedef struct __attribute__((aligned(4))) {
	uint16_t voltage;
	int16_t temperature;
} CellData;


typedef struct
{
	bool bValid;
	uint16_t u16Reading;
} SADCReading;

#define FRAME_VALID_SIG 0xBA77DA7A

// Frame data structure - MUST BE 4-BYTE ALIGNED FOR 32-BIT XFERS
typedef struct __attribute__((aligned(4))) {
	uint32_t validSig;  // if != FRAME_VALID_SIG then frame is not valid
	uint16_t frameBytes;  // actual number of bytes per frame, for diagnostics
	uint64_t timestamp;
	uint32_t moduleUniqueId;  // unique ID for this module (from EEPROM)
	
// session variables, survive frame write	
	uint8_t sg_u8WDTCount;  // number of WDT resets in current session
	uint8_t sg_u8CellCPUCountFewest;   // for the session
	uint8_t sg_u8CellCPUCountMost;     // for the session
	uint8_t sg_u8CellCountExpected;
	
	uint16_t u16maxCurrent;  // session current is in units of 0.02A relative to CURRENT_FLOOR	-655.36
	uint16_t u16minCurrent;
	uint16_t u16avgCurrent;
	uint8_t sg_u8CurrentBufferIndex;
	// these are used for calculations and reality checks, not actually reported
	int32_t sg_i32VoltageStringMin;  // in millivolts
	int32_t sg_i32VoltageStringMax;  // in millivolts
	int16_t sg_i16VoltageStringPerADC;  // in ADC_VOLT_FRACTION fractions of millivolts
	
	
//	frame variables, reset after frame write
	bool bDischargeOn;
	uint16_t sg_u16CellCPUI2CErrors;
	uint8_t sg_u8CellFirstI2CError;
	uint16_t sg_u16BytesReceived;
	uint8_t sg_u8CellCPUCount;     // for the frame
	uint8_t sg_u8MCRXFramingErrors;
	uint8_t sg_u8LastCompleteCellCount;  // Cell count from last complete frame (for MODULE_DETAIL responses)

// processed data, calculated at start of each WRITE frame
	uint16_t u16frameCurrent;
	int16_t sg_s16HighestCellTemp;	//  updated after whole string is processed
	int16_t sg_s16LowestCellTemp;   //  updated after whole string is processed
	int16_t sg_s16AverageCellTemp;	//  updated after whole string is processed
	uint16_t sg_u16HighestCellVoltage;  // in millivolts updated after whole string is processed
	uint16_t sg_u16LowestCellVoltage;  // in millivolts updated after whole string is processed
	uint16_t sg_u16AverageCellVoltage;  // in millivolts updated after whole string is processed
	uint32_t sg_u32CellVoltageTotal;	// All cell CPU voltages added up, in millivolts updated after whole string is processed

	int32_t sg_i32VoltageStringTotal;  // in 15mv increments, from ADC
	
// ADC stuff
	SADCReading ADCReadings[ EADCTYPE_COUNT ];

// Cell stuff
	CellData StringData[ MAX_CELLS ];
} FrameData;

// Define the desired frame size to be exactly 2 sectors (1024 bytes)
#define FRAME_SIZE_TARGET 1024

// Calculate padding needed - this will be computed at compile time
#define FRAME_SIZE_WITHOUT_PADDING (offsetof(FrameData, StringData) + sizeof(CellData) * MAX_CELLS)
#define FRAME_PADDING_NEEDED (FRAME_SIZE_TARGET - FRAME_SIZE_WITHOUT_PADDING)

// Extended frame structure with padding to make exactly 1024 bytes
typedef struct __attribute__((aligned(4))) {
	FrameData frame;
	uint8_t padding[FRAME_PADDING_NEEDED];
} FrameDataPadded;

#define SECTORS_PER_FRAME 2  // Always 2 sectors (1024 bytes)
#define FRAME_BUFFER_SIZE 1024  // Exactly 1024 bytes



// Verify frame size at compile time
#define STATIC_ASSERT(COND,MSG) typedef char static_assertion_##MSG[(COND)?1:-1]
STATIC_ASSERT(FRAME_BUFFER_SIZE >= sizeof(FrameData), frame_buffer_too_small);


// Function prototypes
bool STORE_Init(void);
bool STORE_WriteFrame(volatile FrameData* frame);
bool STORE_StartNewSession(void);
bool STORE_EndSession(void);
bool STORE_GetSessionCount(uint32_t* count);
bool STORE_GetSessionInfo(uint32_t sessionIndex, uint32_t* startSector, uint32_t* sectorCount);


#endif