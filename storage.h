#ifndef _STORAGE_H_
#define _STORAGE_H_

#define STORAGE_ID_PACK_DATA_V0			(1)

#if TOTAL_CELL_COUNT_MAX == 1
#define STORAGE_ID_CELLSTRING_DATA_V0			(2)
#elif TOTAL_CELL_COUNT_MAX == 5
#define STORAGE_ID_CELLSTRING_DATA_V0			(3)
#elif TOTAL_CELL_COUNT_MAX == 7
#define STORAGE_ID_CELLSTRING_DATA_V0			(4)
#elif TOTAL_CELL_COUNT_MAX == 13
#define STORAGE_ID_CELLSTRING_DATA_V0			(5)
#elif TOTAL_CELL_COUNT_MAX == 94
#define STORAGE_ID_CELLSTRING_DATA_V0			(6)
#elif TOTAL_CELL_COUNT_MAX == 192
#define STORAGE_ID_CELLSTRING_DATA_V0			(7)
#else
#error "Unexpected cell count, add a new structure ID"
#endif

// Get rid of __pack since the project has structure packing turned on by default
#undef __pack
#define __pack

typedef __pack struct
{
	uint8_t u8StructureID;
	uint16_t u16Voltage;
	uint16_t u16Current;
} SPackData;

typedef __pack struct
{
	uint16_t u16Voltage;
	uint16_t u16Temperature;
} SCellData;

typedef __pack struct
{
	uint8_t u8StructureID;
	SCellData sCells[TOTAL_CELL_COUNT_MAX];
} SCellStringData;

extern bool StorageLogPackData( uint16_t u16Voltage, uint16_t u16Current );
extern bool StorageLogCellData( uint8_t* pu8CellBufffer, uint16_t u16Length, uint16_t u16CellReports );
extern void StorageSetPowerState(bool bSDPowerAvailable);
extern void StorageInit( void );

#endif
