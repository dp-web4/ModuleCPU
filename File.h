#ifndef _FILE_H_
#define _FILE_H_

#include <time.h>

// Needed for file operators
#include "FatFS/source/ff.h"

// An alias for the file structure
#define SFILE	FIL

// Used for findfirst/findnext
typedef struct SFileFind
{
	struct tm sDateTimestamp;	// Time/datestamp
	uint32_t u32FileAttributes;	// File attributes
	uint32_t u64FileSize;		// Size of the file (in bytes)
	char eFilename[30];			// Just so the structure isn't huge
	DIR sDir;					// Our opened directory
} SFileFind;

// File attributes (mirrors DOS)
#define	ATTRIB_READ_ONLY	0x01		// Read only
#define	ATTRIB_HIDDEN		0x02		// Hidden
#define	ATTRIB_SYSTEM		0x04		// System
#define	ATTRIB_LFN_ENTRY	0x0f		// Long filename entry
#define	ATTRIB_DIRECTORY	0x10		// Directory
#define	ATTRIB_ARCHIVE		0x20		// Archive
#define	ATTRIB_NORMAL		0x40		// Normal

// File functions
extern bool FileInit(void);
extern bool Filefclose(SFILE *psFile);
extern bool Filefopen(SFILE *psFile, 
					  char *peFilename,
					  const char *peFileMode);
extern bool Filefread(void *pvBuffer, 
					  uint32_t u32SizeToRead, 
					  uint32_t *pu32SizeRead, 
					  SFILE *psFile);
extern bool Filefwrite(void *pvBuffer,
					   uint32_t u32SizeToWrite,
					   uint32_t *pu32SizeWritten,
					   SFILE *psFile);
extern bool Filefseek(SFILE *psFile, 
					  int32_t s32Offset, 
					  int32_t s32Origin);	// SEEK_SET, SEEK_CUR, SEEK_END
extern bool Filemkdir(char *peDirName);
extern bool FileFindFirst(char *pePath,
						  char *pePattern,
						  SFileFind *psFileFind);
extern bool FileFindNext(SFileFind *psFileFind);
extern uint32_t FileGetClusterSize(void);
extern bool Filefsync( SFILE *psFile );
extern bool FileGetFilesystemState(void);
extern void FileDeinit(void);

#endif
