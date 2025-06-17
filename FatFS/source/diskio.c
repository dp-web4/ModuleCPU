/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include <string.h>
#include <stdint.h>
#include <xc.h>
#include <stdbool.h>
#include <avr/interrupt.h>
#include "main.h"
#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include "rtc_mcp7940n.h"
#include "SD.h"

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive number to identify the drive */
)
{
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive number to identify the drive */
)
{
	// Attempt to init the SD card
	if (SDInit())
	{
		return(RES_OK);
	}
	else
	{
		// Something's wrong!
		return(RES_NOTRDY);
	}
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive number to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	if (SDRead(sector,
			   (uint8_t *) buff,
			   count))
	{
		return(RES_OK);
	}
	else
	{
		return(RES_ERROR);
	}
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive number to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	if (SDWrite(sector,
				(uint8_t *) buff,
				count))
	{
		return(RES_OK);
	}
	else
	{
		return(RES_ERROR);
	}
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive number (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT eResult = RES_ERROR;
	
	switch (cmd)
	{
		case CTRL_SYNC:
		{
			// Complete any pending writes - N/A here
			eResult = RES_OK;
			break;
		}
		case GET_SECTOR_COUNT:
		{
			// Get # of sectors for this device
			if (SDGetSectorCount((uint32_t *) buff))
			{
				eResult = RES_OK;
			}
			else
			{
				// Something wrong if we can't get the SD card's size
				eResult = RES_NOTRDY;
			}
			
			break;
		}
		case GET_SECTOR_SIZE:
		{
			// Get size of sectors for this device
			if (SDGetBlockSize((uint32_t *) buff))
			{
				eResult = RES_OK;
			}
			else
			{
				// Something wrong if we can't get the SD card's size
				eResult = RES_NOTRDY;
			}
			
			break;
		}

		case GET_BLOCK_SIZE:
		{
			// Block erasure size. Not applicable - set to 1 per FATFS instructions
			*((uint32_t *) buff) = 1;
			eResult = RES_OK;
			break;
		}

		case CTRL_TRIM:
		{
			// We don't do anything with TRIM. SD Cards do their own wear leveling.
			eResult = RES_OK;
			break;
		}
		
		default:
		{
			// No idea what this is - fall through and return an error
			break;
		}
	}
	
	return(eResult);
}

// Required for timestamping of files. See FATFS documentation for
// time/date encoding
DWORD get_fattime (void)
{
	struct tm sTime;

	if (RTCRead(&sTime))
	{
		uint8_t u8Year = (sTime.tm_year - 1900);
		
		if (sTime.tm_year > 99)
		{
			u8Year = (uint8_t) (sTime.tm_year - 2100);
		}
		else
		{
			u8Year = (sTime.tm_year - 2000);
		}
		
		// Successful RTC read! Return the date/timestamp
		return((DWORD)(((DWORD) u8Year << 25)  |
					   ((DWORD) sTime.tm_mon << 21) |
					   ((DWORD) sTime.tm_mday << 16) |
					   (sTime.tm_hour << 11) |
					   (sTime.tm_min << 5) |
					   (sTime.tm_sec >> 1)));
	}
	else
	{
		// Failed to read it - return 1/1/2024 @ midnight.
		
		return((DWORD)(((DWORD) (2024-1980) << 25) |
					   ((DWORD) 1 << 21) |
					   ((DWORD) 0 << 16) |
					   (0 << 11) |
					   (0 << 5) |
					   (0 >> 1)));
	}
}