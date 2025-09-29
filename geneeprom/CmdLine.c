#ifdef _WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <ctype.h>
#ifdef _WIN32
#define false   0
#define true    1
#define bool    uint8_t
#define	snprintf	_snprintf
#else
#include <stdbool.h>
#endif
#include "CmdLine.h"


#define	SafeMemFree(x)	if (x) { free(x); }

// Linked list of options and their values
typedef struct SCmdLineOptionList
{
	char *peOption;			// Textual option
	uint32_t u32OptionLen;	// Length of option text

	char *peValue;			// And its value (if appropriate)
	uint32_t u32ValueLen;		// Length of value text

	const SCmdLineOption *psCmdLineOptionDef;
	struct SCmdLineOptionList *psNextLink;
} SCmdLineOptionList;

// Linked list of user provided options
static SCmdLineOptionList *sg_psOptionList;

// Current command line options
static const SCmdLineOption *sg_psCmdLineRef;

int Sharedstrcasecmp(const char *peString1Head, const char *peString2Head)
{
	const unsigned char *peString1 = (const unsigned char *) peString1Head;
	const unsigned char *peString2 = (const unsigned char *) peString2Head;
	unsigned char eChar1;
	unsigned char eChar2;

	if (peString1 == peString2)
	{
		// Well, of course they're equal
		return(0);
	}

	if (peString1 && (NULL == peString2))
	{
		// String 1 comes before string 2
		return(-1);
	}

	if ((NULL == peString1) && peString2)
	{
		// String 2 comes after string 1
		return(1);
	}

	while (1)
	{
		eChar1 = (unsigned char) (tolower(*peString1));
		eChar2 = (unsigned char) (tolower(*peString2));
		++peString1; 
		++peString2; 

		if ((eChar1 != eChar2) ||
			(0x00 == eChar1))
		{
			// Break out if we're at the end of the string or they don't match
			break;
		}
	}

	return(eChar1 - eChar2);
}


// Find a cmd line option by name in the master command list
static const SCmdLineOption *CmdLineFindOption(char *peOptionText,
											   const SCmdLineOption *psCmdLineOptions)
{
	while (psCmdLineOptions->peCmdOption)
	{
		if (Sharedstrcasecmp(psCmdLineOptions->peCmdOption,
							 peOptionText) == 0)
		{
			return(psCmdLineOptions);
		}

		++psCmdLineOptions;
	}

	// Not found
	return(NULL);
}

// Find a provided command line option in the list
static SCmdLineOptionList *CmdLineFindOptionInList(char *peOptionName,
												   SCmdLineOptionList **ppsPriorOptionLink)
{
	SCmdLineOptionList *psOptionList = sg_psOptionList;

	if (ppsPriorOptionLink)
	{
		*ppsPriorOptionLink = NULL;
	}

	while (psOptionList)
	{
		if (Sharedstrcasecmp(psOptionList->peOption,
							 peOptionName) == 0)
		{
			break;
		}

		if (ppsPriorOptionLink)
		{
			*ppsPriorOptionLink = psOptionList;
		}

		psOptionList = psOptionList->psNextLink;
	}

	return(psOptionList);
}

#define	MEMALLOC(x, y)	x = calloc(1, y); if (NULL == x) { goto errorExit; }

// Process the command line - argc/argv 
static bool CmdLineProcess(int argc,
						   char **argv,
						   const SCmdLineOption *psCmdLineOptionHead,
						   char *peProgramName)
{
	bool bStatus = false;
	uint32_t u32Loop = 0;
	SCmdLineOptionList *psList = NULL;
	const SCmdLineOption *psCmdLineOptions = psCmdLineOptionHead;

	sg_psCmdLineRef = psCmdLineOptionHead;

	// Loop through all provided options and find their definition peer
	while (u32Loop < (uint32_t) argc)
	{
		// Allocate a list item
		if (psList)
		{
			// Not the first of the list
			MEMALLOC(psList->psNextLink, sizeof(*psList->psNextLink));
			psList = psList->psNextLink;
		}
		else
		{
			// First of the list
			MEMALLOC(psList, sizeof(*psList));
			sg_psOptionList = psList;
		}

		psList->psCmdLineOptionDef = CmdLineFindOption(argv[u32Loop],
													   psCmdLineOptions);
		psList->peOption = strdup(argv[u32Loop]);
		if (NULL == psList->peOption)
		{
			goto errorExit;
		}

		if (NULL == psList->psCmdLineOptionDef)
		{
			printf("Unknown command line option '%s'\n", argv[u32Loop]);
			return(false);
		}

		psList->u32OptionLen = (uint32_t) strlen(psList->peOption);

		// See if this requires a value. If so, make sure we have one
		if (psList->psCmdLineOptionDef->bCmdNeedsValue)
		{
			++u32Loop;
			if (u32Loop >= (uint32_t) argc)
			{
				printf("Command line option '%s' requires a value\n", argv[u32Loop - 1]);
				return(false);
			}

			// We have an option!
			psList->peValue = strdup(argv[u32Loop]);
			if (NULL == psList->peValue)
			{
				return(false);
			}

			psList->u32ValueLen = (uint32_t) strlen(psList->peValue);
		}

		u32Loop++;
	}

	// Now run through the list and see if there are an options that are
	// required that we don't have in our list. Also make sure any option
	// that requires a value actually has one
	psCmdLineOptions = psCmdLineOptionHead;

	// Assume that it worked
	bStatus = true;

	while (psCmdLineOptions->peCmdOption)
	{
		if (psCmdLineOptions->bCmdRequired)
		{
			// Let's see if this option is present. If not, complain.
			if (false == CmdLineOption((char *) psCmdLineOptions->peCmdOption))
			{
				printf("Command line option '%s' missing\n", psCmdLineOptions->peCmdOption);
				bStatus = false;
			}
		}

		psCmdLineOptions++;
	}

errorExit:
	return(bStatus);
}

// Single textual command line init
bool CmdLineInit(char *peCmdLine,
				 const SCmdLineOption *psCmdLineOptions,
				 char *peProgramName)
{
	bool bStatus = false;
	int argc = 0;
	char **argv = NULL;

#ifdef _WIN32
	uint32_t u32Loop;
	LPWSTR *argvW = NULL;

	sg_psCmdLineRef = psCmdLineOptions;

	// Convert Windows command line to WCHAR array
	argvW = CommandLineToArgvW(GetCommandLineW(),
							   &argc);
	assert(argvW);

	// If we don't have any command line options, then just bail out
	if (argc < 2)
	{
		goto errorExit;
	}

	// ASCII version - Don't need the first
	argv = (char **) calloc(sizeof(*argv) * (argc - 1), 1);
	assert(argv);

	// Copy all the WCHAR options to ASCII
	for (u32Loop = 1; u32Loop < (uint32_t) argc; u32Loop++)
	{
		// Allocate some space for this string
		argv[u32Loop - 1] = calloc(wcslen(argvW[u32Loop]) + 1, 1);
		assert(argv[u32Loop - 1]);
		wcstombs(argv[u32Loop - 1], argvW[u32Loop], wcslen(argvW[u32Loop]));
	}
#else
	assert(0);
#endif

	bStatus = CmdLineProcess(argc - 1,
							 argv,
							 psCmdLineOptions,
							 peProgramName);

#ifdef _WIN32
	for (u32Loop = 0; u32Loop < (uint32_t) (argc - 1); u32Loop++)
	{
		SafeMemFree(argv[u32Loop]);
	}

	SafeMemFree(argv);
#else
	assert(0);
#endif

/*	{
		char *peLine;

		peLine = CmdLineGet();
		DebugOut("%s: Line='%s'\n", __FUNCTION__, peLine);
		SafeMemFree(peLine);

		CmdLineOptionRemove("-test");

		peLine = CmdLineGet();
		DebugOut("%s: Line='%s'\n", __FUNCTION__, peLine);
		SafeMemFree(peLine);
	} */

errorExit:

	return(bStatus);
}

// Argc/argv init
bool CmdLineInitArgcArgv(int argc,
						 char **argv,
						 const SCmdLineOption *psCmdLineOptions,
						 char *peProgramName)
{
	// Skip past the program name
	return(CmdLineProcess(argc - 1,
						  argv + 1,
						  psCmdLineOptions,
						  peProgramName));
}

char *CmdLineOptionValue(char *peCmdLineOption)
{
	SCmdLineOptionList *psList;

	psList = CmdLineFindOptionInList(peCmdLineOption,
									 NULL);
	if (psList)
	{
		return(psList->peValue);
	}
	else
	{
		return(NULL);
	}
}

char *CmdLineGet(void)
{
	uint32_t u32Length = 0;
	SCmdLineOptionList *psOptionList = sg_psOptionList;
	char *peLine = NULL;

	while (psOptionList)
	{
		u32Length += psOptionList->u32OptionLen + 1;
		u32Length += psOptionList->u32ValueLen + 1;
		psOptionList = psOptionList->psNextLink;
	}

	peLine = calloc(u32Length + 1, 1);
	if (NULL == peLine)
	{
		goto errorExit;
	}

	psOptionList = sg_psOptionList;
	while (psOptionList)
	{
		strcat(peLine, psOptionList->peOption);
		if (psOptionList->peValue)
		{
			strcat(peLine, " ");
			strcat(peLine, psOptionList->peValue);
		}

		// Need to add another space if there are options in there
		if (psOptionList->psNextLink)
		{
			strcat(peLine, " ");
		}

		psOptionList = psOptionList->psNextLink;
	}

errorExit:
	return(peLine);
}

bool CmdLineOption(char *peCmdLineOption)
{
	if (CmdLineFindOptionInList(peCmdLineOption,
								NULL))
	{
		return(true);
	}
	else
	{
		return(false);
	}
}

void CmdLineDumpOptions(const SCmdLineOption *psOptions)
{
	printf("Command line options:\n");

	while (psOptions->peCmdOption)
	{
		printf("  %-12s %s\n", psOptions->peCmdOption, psOptions->peHelpText);
		++psOptions;
	}
}


void CmdLineOptionAdd(char *peCmdLineOptionName,
					  char *peCmdLineOptionValue,
					  bool bCheckOption)
{
	SCmdLineOptionList *psOptionList = sg_psOptionList;

	while ((psOptionList) &&
		   (psOptionList->psNextLink))
	{
		psOptionList = psOptionList->psNextLink;
	}

	if (NULL == psOptionList)
	{
		psOptionList = calloc(sizeof(*psOptionList), 1);
		if (NULL == psOptionList)
		{
			return;
		}

		sg_psOptionList = psOptionList;
	}
	else
	{
		psOptionList->psNextLink = calloc(sizeof(*psOptionList->psNextLink), 1);
		if (NULL == psOptionList)
		{
			return;
		}

		psOptionList = psOptionList->psNextLink;
	}

	psOptionList->peOption = strdup(peCmdLineOptionName);
	psOptionList->u32OptionLen = (uint32_t) strlen(psOptionList->peOption);

	if (peCmdLineOptionValue)
	{
		psOptionList->peValue = strdup(peCmdLineOptionValue);
		psOptionList->u32ValueLen = (uint32_t) strlen(psOptionList->peValue);
	}

	if (bCheckOption)
	{
		psOptionList->psCmdLineOptionDef = CmdLineFindOption(psOptionList->peOption,
															 sg_psCmdLineRef);

		if (NULL == psOptionList->psCmdLineOptionDef)
		{
			printf("Unknown command line option '%s'\n", psOptionList->peValue);
		}

		if (psOptionList->psCmdLineOptionDef->bCmdNeedsValue)
		{
			if (NULL == peCmdLineOptionValue)
			{
				printf("Command line option '%s' missing a value\n", psOptionList->peOption);
			}
		}
		else
		{
			if (peCmdLineOptionValue)
			{
				printf("Command line option '%s' does not take a value\n", psOptionList->peOption);
			}
		}
	}
}

void CmdLineOptionRemove(char *peCmdLineOptionName)
{
	SCmdLineOptionList *psPrior = NULL;
	SCmdLineOptionList *psCurrent = NULL;

	psCurrent = CmdLineFindOptionInList(peCmdLineOptionName,
										&psPrior);
	if (NULL == psCurrent)
	{
		// Option doesn't exist
		return;
	}

	SafeMemFree(psCurrent->peOption);
	SafeMemFree(psCurrent->peValue);

	if (psPrior)
	{
		assert(psPrior->psNextLink == psCurrent);
		psPrior->psNextLink = psCurrent->psNextLink;
	}
	else
	{
		assert(psCurrent == sg_psOptionList);
		sg_psOptionList = psCurrent->psNextLink;
	}

	SafeMemFree(psCurrent);
}

