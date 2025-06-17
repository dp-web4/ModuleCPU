
#include <stdint.h>
#include <xc.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <avr/interrupt.h>
#include "main.h"
#include "storage.h"
#include "File.h"
#include "rtc_mcp7940n.h"

//#define STORAGE_HALT_ON_ERROR
//#define TEST_SMALL_CLUSTER_SIZE	(1024 * 5)

static char sg_u8CurrentDirectory[20];		// Format: "/rec/YYYYMMDDHHMMSS"
static char sg_u8CurrentFilename[20];		// Format: "/YYYYMMDDHHMMSS.bin"
static char sg_u8FullPath[39];		// Format: "<directory>/YYYYMMDDHHMMSS.bin"
static SFILE sg_sFile;
static uint32_t sg_u32BytesWritten;
static bool sg_bFileErrorState;				// true If there has been a file error

static bool StorageCreateNewDirectory( void )
{
	bool bResult = false;
	struct tm sTime;
			
	// Get the current time
	bResult = RTCRead( &sTime );
	if( bResult )
	{
		char u8Filename[5];
		
		// Copy string to data memory
		memcpy( &u8Filename[0], "/rec", sizeof(u8Filename)-1 );
		u8Filename[sizeof(u8Filename)-1] = 0;
		
		bResult = Filemkdir( &u8Filename[0] );
		if( false == bResult )
		{
			// Error writing!
		}
		else
		{
			// Create the new directory name
			snprintf( sg_u8CurrentDirectory, sizeof(sg_u8CurrentDirectory), 
					  "/rec/%04u%02u%02u%02u%02u%02u",
					  (uint16_t)sTime.tm_year + 1900,
					  sTime.tm_mon,
					  sTime.tm_mday,
					  sTime.tm_hour,
					  sTime.tm_min,
					  sTime.tm_sec );
	
			bResult = Filemkdir( sg_u8CurrentDirectory );
			if( false == bResult )
			{
				// Error writing!
			}
		}
	}
	
	return( bResult );
}

static bool StorageCreateNewFile( void )
{
	bool bResult = false;
	struct tm sTime;
	
	// Get the current time
	bResult = RTCRead( &sTime );
	if( bResult )
	{
		//// Create the new full path filename
		//snprintf( sg_u8CurrentFilename, sizeof(sg_u8CurrentFilename),
				//"%s/%04u%02u%02u%02u%02u%02u.bin",
				//(char*)sg_u8CurrentDirectory,
				//sTime.u8Year,
				//sTime.u8Month,
				//sTime.u8Day,
				//sTime.u8Hours,
				//sTime.u8Minutes,
				//sTime.u8Seconds );
	
		// Create the new full path filename
		snprintf( sg_u8CurrentFilename, sizeof(sg_u8CurrentFilename),
				 "/%04u%02u%02u%02u%02u%02u.bin",
				 (uint16_t)sTime.tm_year + 1900,
				 sTime.tm_mon,
				 sTime.tm_mday,
				 sTime.tm_hour,
				 sTime.tm_min,
				 sTime.tm_sec );
				
		strcpy( (char*)sg_u8FullPath, (char*)sg_u8CurrentDirectory );
		strcat( (char*)sg_u8FullPath, (char*)sg_u8CurrentFilename );
	
		bResult = Filefopen( &sg_sFile, sg_u8FullPath, "ab+");
		if( false == bResult )
		{
			// Error writing!
			sg_bFileErrorState = true;
		}
		else
		{
			bResult = Filefsync( &sg_sFile );
			if( false == bResult )
			{
				// Error writing!
				sg_bFileErrorState = true;
			}
		}
	}
	
	return( bResult );
}

// This fully deinitializes the filesystem only. Note that it does not
// check for power state, etc...
static void StorageFilesystemCleanup(void)
{
	// If the filesystem is mounted, then close the file
	if (FileGetFilesystemState())
	{
		// Close the existing file, if any
		(void) Filefclose( &sg_sFile );

		// Clear any remnants of the file
		memset( &sg_sFile, 0, sizeof(sg_sFile) );
	}
		
	// Now deinit the filesystem
	FileDeinit();		
}

// This is called any time SD power state is set. Note that it's possible
// for the existing state to be set already
//static bool sg_bSDPowerAvailable;
//void StorageSetPowerState(bool bSDPowerAvailable)
//{
	// If we don't have power, deinit everything
//	if (false == bSDPowerAvailable)
//	{
//		StorageFilesystemCleanup();
//	}
	
//	sg_bSDPowerAvailable = bSDPowerAvailable;
//}

static bool StorageMakeReady( void )
{
	bool bResult = true;		// Assume storage is ready
	
	// If we have a file error state, and there's SD card power, try
	// to reinit the filesystem again
	if (sg_bFileErrorState)
	{
		sg_bFileErrorState = false;
		if (sg_bSDPowerAvailable)
		{
			// Unmount the filesystem
			StorageFilesystemCleanup();
		}
	}
	
	// If the filesystem isn't initialized and SD card power is present,
	// init the filesystem.
	if ((false == FileGetFilesystemState()) &&
		(sg_bSDPowerAvailable))
	{

		// Init the filesystem
		bResult = FileInit();
		
		if( bResult )
		{
			// Create a new directory and a new file for that directory
			bResult = StorageCreateNewDirectory();
			if( bResult )
			{
				bResult = StorageCreateNewFile();
				if( bResult )
				{
					// Reset byte counter
					sg_u32BytesWritten = 0;
				}
			}
			
			if (false == bResult)
			{
				// If anything fails to init, deinit the filesystem so we
				// don't attempt to write to nonexistent media.
				FileDeinit();
			}
		}
	}
	
	// If there's no power, then it's a failure to init
	if (false == sg_bSDPowerAvailable)
	{
		bResult = false;
	}
	
	// If the filesystem isn't mounted then we've also failed
	if (false == FileGetFilesystemState())
	{
		bResult = false;
	}
	
	return( bResult );
}

static bool StoragePrepareSpace( uint32_t u32Length )
{
	bool bResult;
	
	// First check if there's any setup necessary and we're in a good state
	bResult = StorageMakeReady();
	
	if( bResult )
	{
		// If this write would exceed the cluster size, close this file
		//	and open a new one
#ifndef TEST_SMALL_CLUSTER_SIZE		
		if( (sg_u32BytesWritten + u32Length) > FileGetClusterSize() )
#else
		if( (sg_u32BytesWritten + u32Length) > TEST_SMALL_CLUSTER_SIZE )
#endif
		{
			(void) Filefclose( &sg_sFile );
			
			bResult = StorageCreateNewFile();
			if( bResult )
			{
				sg_u32BytesWritten = 0;
			}
		}
	}

	return( bResult );
}

bool StorageLogPackData( uint16_t u16Voltage, uint16_t u16Current )
{
	bool bResult = false;
	SPackData sPackData;
	
	sPackData.u8StructureID = STORAGE_ID_PACK_DATA_V0;
	sPackData.u16Voltage = u16Voltage;
	sPackData.u16Current = u16Current;
	
	// Prepare space for storage
	bResult = StoragePrepareSpace( sizeof(sPackData) );
	
	// Write out structure ID and pack data
	if( bResult )
	{
		uint32_t u32Written;
			
		// Write out structure ID and pack data
		bResult = Filefwrite( &sPackData, sizeof(sPackData), &u32Written, &sg_sFile );
		if( (false == bResult) || (u32Written != sizeof(sPackData)) )
		{
			// Error writing!
			bResult = false;
			sg_bFileErrorState = true;
			goto errorExit;
		}
				
		sg_u32BytesWritten += u32Written;
			
		bResult = Filefsync( &sg_sFile );
		if( false == bResult )
		{
			// Error writing!
			sg_bFileErrorState = true;
			goto errorExit;
		}
	}
	
errorExit:
	return( bResult );
}

bool StorageLogCellData( uint8_t* pu8CellBufffer, uint16_t u16Length, uint16_t u16CellReports )
{
	bool bResult = false;
	uint32_t u32FrameBytesWritten = 0;
	
	// Prepare space for whole frame storage
	bResult = StoragePrepareSpace( sizeof(SCellStringData) );
		
	if( bResult )
	{
		uint8_t u8StructureID = STORAGE_ID_CELLSTRING_DATA_V0;
		uint32_t u32Written;
		uint32_t u32BytesRemaining = 0;
						
		// Write out structure ID first
		bResult = Filefwrite( &u8StructureID, sizeof(u8StructureID), &u32Written, &sg_sFile );
		if( (false == bResult) || (u32Written != sizeof(u8StructureID)) )
		{
			// Error writing!
			bResult = false;
			sg_bFileErrorState = true;
			goto errorExit;
		}
		
		u32FrameBytesWritten += u32Written;
		sg_u32BytesWritten += u32Written;
		
		// Now write out the entire buffer
		bResult = Filefwrite( pu8CellBufffer, u16Length, &u32Written, &sg_sFile );
		if( (false == bResult) || (u32Written != u16Length) )
		{
			// Error writing!
			bResult = false;
			sg_bFileErrorState = true;
			goto errorExit;
		}
		
		u32FrameBytesWritten += u32Written;
		sg_u32BytesWritten += u32Written;
		
		u32BytesRemaining = ((u16CellReports *sizeof(SCellData))+1);
		if( u32BytesRemaining > u32FrameBytesWritten )
		{
			u32BytesRemaining -= u32FrameBytesWritten;
		}
		else
		{
			u32BytesRemaining = 0;
		}

		if( u32BytesRemaining )		
		{
			uint8_t u8EmptyBuffer[32];
			
			memset( u8EmptyBuffer, 0, sizeof(u8EmptyBuffer) );
			
			// Write out remaining cells data as zeros to fill out the frame
			while( u32BytesRemaining )
			{
				uint32_t u32ToWrite = sizeof(u8EmptyBuffer);
				uint32_t u32Written;
				
				if( u32ToWrite > u32BytesRemaining )
				{
					u32ToWrite = u32BytesRemaining;
				}
				
				bResult = Filefwrite( u8EmptyBuffer, u32ToWrite, &u32Written, &sg_sFile );
				if( (false == bResult) || (u32ToWrite != u32Written) )
				{
					// Error writing!
					bResult = false;
					sg_bFileErrorState = true;
					goto errorExit;
				}
				
				sg_u32BytesWritten += u32ToWrite;
				u32BytesRemaining -= u32ToWrite;
			}
		}
		
		bResult = Filefsync( &sg_sFile );
		if( false == bResult )
		{
			// Error writing!
			sg_bFileErrorState = true;
			goto errorExit;
		}
	}
	
errorExit:
	return( bResult );
}

void StorageInit( void )
{
}
