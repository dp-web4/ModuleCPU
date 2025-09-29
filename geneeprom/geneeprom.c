#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>

#ifdef _WIN32
#define false   0
#define true    1
#define bool    uint8_t
#else
#include <stdbool.h>
#endif

#include "CmdLine.h"
#include "../../ModuleCPU/EEPROM.h"
#include "../../Shared/Shared.h"

static SCmdLineOption sg_sCmdLineOptions[] =
{
	{"-id",				"Module controller's ID (hex)",						true,	true},
	{"-cells",			"# Of battery cells expected (decimal)",			true,	true},
	{"-chargemax",		"Max charge current (positive decimal amps)",  	 	true,	true},
	{"-dischargemax",	"Max discharge current (negative decimal amps)",	true,	true},
	{"-cellreset",		"Cell reset count (0 to disable)",					true,	true},
	{"-file",			"Output EEPROM filename",							true,	true},

	{NULL}
};

#ifndef _WIN32
static void strupr(char *peString)
{
	while (*peString)
	{
		*peString = toupper(*peString);
		++peString;
	}
}
#endif

static bool DumpHex(uint8_t *pu8Data,
					uint32_t u32Length,
					FILE *psFile)
{
	uint32_t u32Offset = 0;
	char eString[100];
	int8_t u8Checksum = 0;

	while (u32Length)
	{
		uint32_t u32Chunk;
		uint32_t u32Loop;
		char eString[50];

		u32Chunk = u32Length;
		if (u32Chunk > 16)
		{
			u32Chunk = 16;
		}

		sprintf(eString, ":%.2x%.2x%.2x00", u32Chunk, (uint8_t) ((u32Offset >> 8) & 0xff), (uint8_t) (u32Offset & 0xff));
		strupr(eString);
		fprintf(psFile, "%s", eString);

		u32Length -= u32Chunk;

		u8Checksum = (uint8_t) u32Chunk;
		u8Checksum += (uint8_t) (u32Offset >> 8);
		u8Checksum += (uint8_t) (u32Offset & 0xff);

		u32Loop = u32Chunk;
		while (u32Loop)
		{
			sprintf(eString, "%.2x", (uint8_t)  *pu8Data);
			strupr(eString);
			u8Checksum += *pu8Data;
			fprintf(psFile, "%s", eString);
			u32Loop--;
			++pu8Data;
		}

		u8Checksum = (uint8_t) ( ((uint8_t) 255 - u8Checksum) + 1);
		u32Offset += u32Chunk;
		sprintf(eString, "%.2x", (uint8_t) u8Checksum);
		strupr(eString);
		fprintf(psFile, "%s\n", eString);
	}

	// Terminator
	fprintf(psFile, ":00000001FF\n");

	return(true);
}

static bool HexToBin(char *peHexHead,
					 uint32_t *pu32Value)
{
	char *peHex = peHexHead;
	uint8_t u8Digits = 0;

	*pu32Value = 0;

	while (*peHex)
	{
		*pu32Value <<= 4;
		if ((*peHex >= '0') && (*peHex < '9'))
		{
			*pu32Value |= (*peHex - '0');
		}
		else
		if ((tolower(*peHex) >= 'a') && (tolower(*peHex <= 'f')))
		{
			*pu32Value |= (toupper(*peHex) - '0' - 7);
		}
		else
		{
			printf("Invalid hex digit '%c'\n", *peHex);
			return(false);
		}
		++peHex;
		++u8Digits;
		if (u8Digits > 8)
		{
			printf("Too many hex digits - '%s'\n", peHexHead);
			return(false);
		}
	}

	return(true);
}

int main(int argc, char **argv)
{
	FILE *psFile;
	uint8_t *pu8Data;
	uint32_t u32Cells;
	float fValue;
	bool bResult;

	if (false == CmdLineInitArgcArgv(argc,
									 argv,
									 sg_sCmdLineOptions,
									 argv[0]))
	{
		printf("Failed\n");
		CmdLineDumpOptions(sg_sCmdLineOptions);
		return(1);
	}

	pu8Data = malloc(EEPROM_SIZE);
	assert(pu8Data);
	memset((void *) pu8Data, 0xff, EEPROM_SIZE);

	// Get the ID
	if (false == HexToBin(CmdLineOptionValue("-id"),
						  (uint32_t *) (pu8Data + EEPROM_UNIQUE_ID)))
	{
		return(1);
	}

	// And the cell count
	u32Cells = atol(CmdLineOptionValue("-cells"));
	if (u32Cells > 255)
	{
		printf("Cell count can't be larger than 255\n");
		return(1);
	}

	*(pu8Data + EEPROM_EXPECTED_CELL_COUNT) = (uint8_t) u32Cells;

	// Max charge current
	fValue = atof(CmdLineOptionValue("-chargemax"));
	if (fValue < 0.0)
	{
		printf("Charge current maximum must be a positive number\n");
		exit(1);
	}
	if (fValue < CURRENT_FLOOR)
	{
		printf("Charge current is lower than allowed floor - %6.2f\n", CURRENT_FLOOR);
		exit(1);
	}

	fValue = ((fValue - CURRENT_FLOOR) / 0.02);
	*((uint16_t *) (pu8Data + EEPROM_MAX_CHARGE_CURRENT)) = (uint16_t) fValue;

	// Max discharge current
	fValue = atof(CmdLineOptionValue("-dischargemax"));
	if (fValue >= 0.0)
	{
		printf("Discharge current maximum must be a negative number\n");
		exit(1);
	}

	if (fValue > CURRENT_CEILING)
	{
		printf("Discharge current is higher than allowed ceiling - %6.2f\n", CURRENT_CEILING);
		exit(1);
	}

	fValue = ((fValue - CURRENT_FLOOR) / 0.02);
	*((uint16_t *) (pu8Data + EEPROM_MAX_DISCHARGE_CURRENT)) = (uint16_t) fValue;

	// Cell reset (the # Of times we need to see a 
	*(pu8Data + EEPROM_SEQUENTIAL_COUNT_MISMATCH) = (uint8_t) atol(CmdLineOptionValue("-cellreset"));

	psFile = fopen(CmdLineOptionValue("-file"), "wb");
	if (NULL == psFile)
	{
		printf("Can't open file '%s' for writing\n", CmdLineOptionValue("-file"));
		return(1);
	}

	bResult = DumpHex(pu8Data,
					  EEPROM_SIZE,
					  psFile);

	fclose(psFile);

	// Success
	return(bResult);
}

