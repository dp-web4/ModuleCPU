#include <stdint.h>
#include <xc.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <string.h>
#include <stdio.h>
#include "main.h"
#include "File.h"
#include "FatFS/source/ff.h"
#include "SD.h"

// Set true if the filesystem has been successfully initialized
static bool sg_bFilesystemInitialized;

// FATFS Volume
static FATFS sg_hVolume;

// Convert FileFind data to our SFileFind structure
static void FileFindExtractData(SFileFind *psFileFind,
								FILINFO *psFATFSFileInfo)
{
	// File size
	psFileFind->u64FileSize = (uint64_t) psFATFSFileInfo->fsize;
	
	// Convert the date/time to struct tm equivalents

	// Time
	psFileFind->sDateTimestamp.tm_sec = (psFATFSFileInfo->ftime & 0x1f) << 1;
	psFileFind->sDateTimestamp.tm_min = (psFATFSFileInfo->ftime >> 5) & 0x3f;
	psFileFind->sDateTimestamp.tm_hour = (psFATFSFileInfo->ftime >> 11);
	
	// Date
	psFileFind->sDateTimestamp.tm_mday = (psFATFSFileInfo->fdate & 0x1f);
	psFileFind->sDateTimestamp.tm_mon = ((psFATFSFileInfo->fdate >> 5) - 1) & 0xf;
	psFileFind->sDateTimestamp.tm_year = (psFATFSFileInfo->fdate >> 9) + 80;
									
	// Copy in the filename
	strncpy(psFileFind->eFilename,
			(char const *) psFATFSFileInfo->fname,
			sizeof(psFileFind->eFilename) - 1);

	if (psFATFSFileInfo->fattrib & AM_DIR)
	{
		psFileFind->u32FileAttributes |= ATTRIB_DIRECTORY;
	}
	
	if (psFATFSFileInfo->fattrib & AM_RDO)
	{
		psFileFind->u32FileAttributes |= ATTRIB_READ_ONLY;
	}
	
	if (psFATFSFileInfo->fattrib & AM_HID)
	{
		psFileFind->u32FileAttributes |= ATTRIB_HIDDEN;
	}
	
	if (psFATFSFileInfo->fattrib & AM_SYS)
	{
		psFileFind->u32FileAttributes |= ATTRIB_SYSTEM;
	}
	
	if (psFATFSFileInfo->fattrib & AM_ARC)
	{
		psFileFind->u32FileAttributes |= ATTRIB_ARCHIVE;
	}
}

bool FileFindNext(SFileFind *psFileFind)
{
	FILINFO sFileInfo;
	FRESULT eResult;

	eResult = f_findnext(&psFileFind->sDir,
						 &sFileInfo);
						 
	if (FR_OK == eResult)
	{
		// Convert the data
		FileFindExtractData(psFileFind,
							&sFileInfo);
		
		return(true);
	}
	else
	{
		return(false);
	}
}

bool FileFindFirst(char *pePath,
				   char *pePattern,
				   SFileFind *psFileFind)
{
	FILINFO sFileInfo;
	FRESULT eResult;
	
	// If we aren't initialized, then just return
	if (false == sg_bFilesystemInitialized)
	{
		return(false);
	}
	
	eResult = f_findfirst(&psFileFind->sDir,
						  &sFileInfo,
						  (const TCHAR *) pePath,
						  (const TCHAR *) pePattern);
	
	if (FR_OK == eResult)
	{
		// Convert the data
		FileFindExtractData(psFileFind,
							&sFileInfo);
							
		return(true);
	}
	else
	{
		return(false);
	}
}

bool Filemkdir(char *peDirectoryName)
{
	FRESULT eResult;
	
	// If we aren't initialized, then just return
	if (false == sg_bFilesystemInitialized)
	{
		return(false);
	}
	
	eResult = f_mkdir((const TCHAR *) peDirectoryName);
	if ((eResult != FR_OK) &&
		(eResult != FR_EXIST))
	{
		return(false);
	}
	else
	{
		return(true);
	}
}

bool Filefclose(SFILE *psFile)
{
	FRESULT eResult;
	
	// If we aren't initialized, then just return
	if (false == sg_bFilesystemInitialized)
	{
		return(false);
	}

	// Or a NULL file handle
	if (NULL == psFile)
	{
		return(false);
	}
	
	eResult = f_close(psFile);
	if (eResult != FR_OK)
	{
		return(false);
	}
	else
	{
		return(true);
	}
}

bool Filefseek(SFILE *psFile, 
			   int32_t s32Offset, 
			   int32_t s32Origin)
{
	FSIZE_t eOffset;
	FRESULT eResult;
	
	// If we aren't initialized, then just return
	if (false == sg_bFilesystemInitialized)
	{
		return(false);
	}

	if (SEEK_CUR == s32Origin)
	{
		// Offset is from the current file position
		eOffset = (FSIZE_t) ((int32_t) f_tell(psFile) + s32Offset);
	}
	else
	if (SEEK_SET == s32Origin)
	{
		// The offset is an absolute offset
		eOffset = (FSIZE_t) s32Offset;
	}
	else
	if (SEEK_END == s32Origin)
	{
		// End of file
		eOffset = f_size(psFile);
	}
	else
	{
		return(false);
	}

	// Do the seek
	eResult = f_lseek(psFile,
					  eOffset);
	
	if (eResult != FR_OK)
	{
		return(false);
	}
	else
	{
		return(true);
	}
}

bool Filefwrite(void *pvBuffer, 
				uint32_t u32SizeToWrite, 
				uint32_t *pu32SizeWritten, 
				SFILE *psFile)
{
	FRESULT eResult;
	UINT eDataWritten = 0;
	
	// If we aren't initialized, then just return
	if (false == sg_bFilesystemInitialized)
	{
		return(false);
	}

	// 0 Size written is a truncation request
	if (0 == u32SizeToWrite)
	{
		eDataWritten = 0;
		eResult = f_truncate(psFile);
	}
	else
	{
		eResult = f_write(psFile,
						  pvBuffer,
						  (UINT) u32SizeToWrite,
						  &eDataWritten);
	}
	
	if (pu32SizeWritten)
	{
		*pu32SizeWritten = eDataWritten;
	}

	if (eResult != FR_OK)
	{
		return(false);
	}
	else
	{
		// See if it's truncated.
		if (eDataWritten != u32SizeToWrite)
		{
			// Data truncation
			return(false);
		}
		else
		{
			// Successfully written
			return(true);
		}
	}
}

bool Filefread(void *pvBuffer, uint32_t u32SizeToRead, uint32_t *pu32SizeRead, SFILE *psFile)
{
	FRESULT eResult;
	UINT eReadLength = 0;
	
	// If we aren't initialized, then just return
	if (false == sg_bFilesystemInitialized)
	{
		return(false);
	}

	eResult = f_read(psFile,
					 pvBuffer,
					 (UINT) u32SizeToRead,
					 &eReadLength);
	if (pu32SizeRead)
	{
		*pu32SizeRead = eReadLength;
	}

	if (eResult != FR_OK)
	{
		return(false);
	}
	else
	{
		if (eReadLength != u32SizeToRead)
		{
			// Truncated read
			return(false);
		}
		else
		{
			// All good
			return(true);
		}
	}
}

bool Filefopen(SFILE *psFile, char *peFilename, const char *peFileMode)
{
	FRESULT eResult;
	BYTE eFileMode = 0;
	FSIZE_t eFileSize;
	bool bAppending = false;

	// If we aren't initialized, then just return
	if (false == sg_bFilesystemInitialized)
	{
		return(false);
	}
	
	if (strcmp(peFileMode, "r+") == 0)
	{
		eFileMode = FA_READ | FA_WRITE | FA_OPEN_EXISTING;
	}
	else
	if (strcmp(peFileMode, "w+") == 0)
	{
		eFileMode = FA_READ | FA_WRITE | FA_CREATE_ALWAYS;
	}
	else
	if (strcmp(peFileMode, "ab") == 0)
	{
		eFileMode = FA_OPEN_ALWAYS | FA_WRITE;
		bAppending = true;
	}
	else
	if (strcmp(peFileMode, "rb") == 0)
	{
		// Read but only if it exists
		eFileMode = FA_READ | FA_OPEN_EXISTING;
	}
	else
	if (strcmp(peFileMode, "wb") == 0)
	{
		eFileMode = FA_READ | FA_WRITE | FA_CREATE_ALWAYS;
	}
	else
	if ((strcmp(peFileMode, "a+") == 0) ||
		(strcmp(peFileMode, "ab+") == 0))
	{
		eFileMode = FA_READ | FA_WRITE | FA_OPEN_ALWAYS;
		bAppending = true;
	}
	else
	{
		eResult = FR_INVALID_PARAMETER;
		goto errorExit;
	}

	// Try to open the file
	eResult = f_open(psFile,
					 (const TCHAR *) peFilename,
					 eFileMode);
	
	if (eResult != FR_OK)
	{
		// Failed
	}
	else
	{
		// If we're appending, then seek to the end of the file
		if (bAppending)
		{
			// Get the file's size
			eFileSize = f_size(psFile);

			// And seek to the end			
			eResult = f_lseek(psFile,
							  eFileSize);
							  
			if (eResult != FR_OK)
			{
				// Failed
			}
		}
	}

errorExit:
	if (eResult != FR_OK)
	{
		return(false);
	}
	else
	{
		return(true);
	}
}

bool Filefsync( SFILE *psFile )
{
	FRESULT eResult;
	
	eResult = f_sync( psFile );
	
	if (eResult != FR_OK)
	{
		return(false);
	}
	else
	{
		return(true);
	}
}

uint32_t FileGetClusterSize(void)
{
	uint32_t u32BlockSize;
	
	// If we aren't initialized, then just return
	if (false == sg_bFilesystemInitialized)
	{
		return(0);
	}
	
	// Get the block size from the SD card
	if (false == SDGetBlockSize(&u32BlockSize))
	{
		MBASSERT(0);
	}
	
	// Get the cluster size from the mounted volume
	return(sg_hVolume.csize * (uint16_t) u32BlockSize);
}

// Needed to translate the volume to partition
PARTITION VolToPart[FF_VOLUMES] = 
{
	    {0, 1},    /* "0:" ==> 1st partition in physical drive 0 */
};

// Single partition to make other OSes happy
static const LBA_t sg_sPartitionList[] =
{
	100,			// Entire partition is for 
	0
};

// Deinitialize the filesystem (if it's currently initialized
void FileDeinit(void)
{
	if (sg_bFilesystemInitialized)
	{
		// IF the filesystem is already initialized, unmount it and ignore
		// the error
		(void) f_unmount("");
		sg_bFilesystemInitialized = false;
		
		// Clear the structure of any remnants
		memset((void *) &sg_hVolume, 0, sizeof(sg_hVolume));
	}
}

// Init the filesystem. 
bool FileInit(void)
{
	FRESULT eResult;
	bool bReformat = false;
	bool bReformatted = false;
	uint8_t u8Data[2];		// Our "reformat reason"
	
	// If the filesystem is currently initialized, deinitialize it
	FileDeinit();
	
mountAgain:
	bReformat = false;
	
	// Now try to mount the volume immediately
	eResult = f_mount(&sg_hVolume,
					  "",
					  1);
	if (false == bReformatted)
	{
		u8Data[0] = eResult;
		u8Data[1] = 0xff;		// Default value
	}
	
	if (FR_OK == eResult)
	{
		// If it's FAT32, then reformat it
		if (FS_FAT32 == sg_hVolume.fs_type)
		{
			bReformat = true;
			u8Data[1] = (uint8_t) sg_hVolume.fs_type;
			
			// Unmount the filesystem
			eResult = f_unmount("");
			if (eResult != FR_OK)
			{
				// Failed to mount the SD card
				goto errorExit;
			}
		}
	}	
	else
	if (FR_NO_FILESYSTEM == eResult)
	{
		bReformat = true;
	}
	else
	{
		// Other error of some sort
		goto errorExit;
	}
	
	// If this microSD card is FAT32 or it has no filesystem, unmount 
	// it and reformat it as exFAT
	if (bReformat)
	{
		MKFS_PARM sFSParams;
		uint32_t u32SDCardSectorCount;
		
		// Format it with exFAT
		memset((void *) &sFSParams, 0, sizeof(sFSParams));
		
		sFSParams.fmt = FM_EXFAT;
		sFSParams.n_fat = 0;			// No effect on exFAT
		sFSParams.align = 0;			// This will fetch the block size from the ioctl code
		
		// Pick the cluster size based on the SD card size:
		//
		// <256MB		4K
		// 256MB-32GB	32K
		// >32GB		128K
		
		(void) SDGetSectorCount(&u32SDCardSectorCount);
		
		if (u32SDCardSectorCount < 524288)
		{
			// <256MB - 4K - up to 524288 sectors of 512 bytes each
			sFSParams.au_size = 4*1024;
		}
		else
		if (u32SDCardSectorCount > 67108864)
		{
			// >32GB - 128K - over 67108864 sectors of 512 bytes each
			sFSParams.au_size = ((WORD) 128 * (WORD) 1024);
		}
		else
		{
			// 256MB-32GB - 32K - Anything in between gets a 32K cluster
			sFSParams.au_size = ((WORD) 32 * (WORD) 1024);
		}
		
		sFSParams.n_root = 32768;		// Max number of directories in the root
		
		// Create a partition
		eResult = f_fdisk(0,
						  sg_sPartitionList,
						  (void *) &sg_hVolume);
		if (eResult != FR_OK)
		{
			// Failed to mount the SD card
			goto errorExit;
		}
		
		// This is reusing the sg_hVolume structure as a general scratch pad area because
		// nothing is mounted.
		eResult = f_mkfs("0:",
						 &sFSParams,
						 (void *) &sg_hVolume,
						 sizeof(sg_hVolume));
						 
		if (eResult != FR_OK)
		{
			// Failed to reformat the SD card exFAT
			goto errorExit;
		}
		
		// Clear the structure of any remnants
		memset((void *) &sg_hVolume, 0, sizeof(sg_hVolume));
		
		// Indicate we've reformatted the card
		bReformatted = true;
		
		// Mount the filesystem
		goto mountAgain;
	}
	
	// Mark the filesystem initialized so that we can use the following APIs
	sg_bFilesystemInitialized = true;
	
	// If we've reformatted, then write out the reason we reformatted
	if (bReformatted)
	{
		SFILE sFile;
		char u8Filename[13];
		
		// Copy string to data memory
		memcpy( &u8Filename[0], (const char*)"Reformat.bin", sizeof(u8Filename)-1 );
		u8Filename[sizeof(u8Filename)-1] = 0;
		
		if (Filefopen(&sFile,
					  &u8Filename[0], 
					  "wb"))
		{
			(void) Filefwrite((void *) u8Data,
							  sizeof(u8Data),
							  NULL,
							  &sFile);
							  
			(void) Filefclose(&sFile);
		}
	}
	
errorExit:
	return(sg_bFilesystemInitialized);
}

// Returns true if the filesystem is initialized and ready for operation
bool FileGetFilesystemState(void)
{
	return(sg_bFilesystemInitialized);
}
