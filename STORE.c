#include "STORE.h"
#include "SD.h"
#include <string.h>

static GlobalState gState;
static uint32_t currentSector;
static uint8_t __attribute__((aligned(4))) frameBuffer[FRAME_BUFFER_SIZE];  // Frame data for CAN transfer - DO NOT REUSE
static uint8_t __attribute__((aligned(4))) sectorBuffer[SECTOR_SIZE];  // Temporary buffer for global state/session map operations

static bool readGlobalState(void) {
	// Global state now tracked in EEPROM (frame counter, etc.)
	// SD card no longer used for global state
	// Nothing to read here - EEPROM reads happen in specific modules
	return true;
}

static bool writeGlobalState(void) {
	// Global state now tracked in EEPROM (frame counter, etc.)
	// SD card no longer used for global state
	// TODO: Save key metrics to EEPROM here if needed
	return true;
}

static bool updateSessionMap(void) {

	// Global state now tracked in EEPROM (frame counter, etc.)
	// SD card no longer used for global state
	// TODO: Save key metrics to EEPROM here if needed
	return true;

/*	
	uint32_t mapSector = gState.activeSessionMapSector;
	uint32_t mapOffset = gState.activeSessionMapOffset;

	if (!SDRead(mapSector, sectorBuffer, 1)) {
		return false;
	}

	// Update session map
	*(uint64_t*)(sectorBuffer + mapOffset) = gState.newSessionSector;

	if (!SDWrite(mapSector, sectorBuffer, 1)) {
		return false;
	}

	// Update map pointers
	gState.activeSessionMapOffset += sizeof(uint64_t);
	if (gState.activeSessionMapOffset >= SECTOR_SIZE) {
		gState.activeSessionMapSector++;
		gState.activeSessionMapOffset = 0;
	}

	return writeGlobalState();
	*/
}

bool STORE_Init(void) {
	if (!SDInit()) {
		return false;
	}
	// Global state now tracked in EEPROM (frame counter, etc.)
	// SD card no longer used for global state
	// TODO: Save key metrics to EEPROM here if needed
	return true;
/*
	if (!readGlobalState()) {
		// Initialize global state if it doesn't exist
		memset(&gState, 0, sizeof(GlobalState));
		gState.firstSessionSector = 1;
		gState.newSessionSector = 1;
		gState.activeSessionMapSector = 4;
		gState.activeSessionMapOffset = 0;
		// TODO: Initialize other fields as needed
		if (!writeGlobalState()) {
			return false;
		}
	}

	currentSector = gState.newSessionSector;
	return true;
*/
}

bool STORE_WriteFrame(volatile FrameData* frame, bool bSDCardReady, bool bSDWriteEnabled) {
	uint32_t currentOffset = 0;
	uint32_t sectorsToWrite;

	// Verify frame size
	if(frame->m.frameBytes > FRAME_BUFFER_SIZE) {
		return false;
	}

	// ALWAYS copy frame data to frameBuffer (needed for CAN frame transfer)
	// This ensures frameBuffer contains complete frame regardless of SD write status
	memcpy(frameBuffer, (const void*)frame, FRAME_BUFFER_SIZE);

	// Only write to SD card if flags permit
	if (!bSDCardReady || !bSDWriteEnabled) {
		return true;  // Frame copied to buffer, SD write skipped
	}

	// Set SD busy flag to prevent state transitions during SD write
	SetSDBusy(true);

	// Calculate how many complete sectors we need to write
	sectorsToWrite = SECTORS_PER_FRAME;

	// Write all sectors to SD card
	while(sectorsToWrite > 0) {
		if(!SDWrite(currentSector, frameBuffer + currentOffset, 1)) {
			SetSDBusy(false);  // Clear busy flag before returning on error
			return false;  // SD write failed
		}
		currentSector++;
		currentOffset += SECTOR_SIZE;
		sectorsToWrite--;
	}

	// Clear SD busy flag - operation complete
	SetSDBusy(false);

	// Frame successfully written to SD - this is now a permanent frame
	// Note: Frame counter is incremented by caller BEFORE calling this function
	// and is already updated in the frame metadata

	return true;  // Frame copied and written to SD
}

bool STORE_ReadFrameByCounter(uint32_t frameCounter) {
	// Frame counter directly maps to SD sector address
	// Each frame is 2 sectors (SECTORS_PER_FRAME)
	// Frame N is at sector (N * SECTORS_PER_FRAME)
	uint32_t startSector = frameCounter * SECTORS_PER_FRAME;

	// Set SD busy flag to prevent state transitions during SD read
	SetSDBusy(true);

	// Read the complete frame (2 sectors) into frameBuffer
	if (!SDRead(startSector, frameBuffer, SECTORS_PER_FRAME)) {
		SetSDBusy(false);  // Clear busy flag before returning on error
		return false;  // SD read failed
	}

	// Clear SD busy flag - operation complete
	SetSDBusy(false);

	return true;  // Frame loaded into frameBuffer
}

bool STORE_StartNewSession(void) {
	// Update session info
	gState.sessionCount++;
	gState.newSessionSector = currentSector;
	
	// Ensure sector alignment for new sessions
	if(currentSector % SECTORS_PER_FRAME != 0) {
		currentSector += SECTORS_PER_FRAME - (currentSector % SECTORS_PER_FRAME);
	}
	
	return updateSessionMap();
}

uint8_t* STORE_GetFrameBuffer(void) {
	return frameBuffer;
}

bool STORE_EndSession(void) {
	// Ensure we end on a frame boundary
	if(currentSector % SECTORS_PER_FRAME != 0) {
		currentSector += SECTORS_PER_FRAME - (currentSector % SECTORS_PER_FRAME);
	}
	
	gState.lastSessionSector = currentSector - 1;
	return writeGlobalState();
}

// Modified to account for multi-sector frames
bool STORE_GetSessionInfo(uint32_t sessionIndex, uint32_t* startSector, uint32_t* sectorCount) {
	if (sessionIndex >= gState.sessionCount) {
		return false;
	}

	uint32_t mapSector = gState.activeSessionMapSector;
	uint32_t mapOffset = sessionIndex * sizeof(uint64_t);

	while (mapOffset >= SECTOR_SIZE) {
		mapSector++;
		mapOffset -= SECTOR_SIZE;
	}

	if (!SDRead(mapSector, frameBuffer, 1)) {
		return false;
	}

	*startSector = *(uint64_t*)(frameBuffer + mapOffset);

	if (sessionIndex == gState.sessionCount - 1) {
		// Current session - calculate based on current position
		*sectorCount = currentSector - *startSector;
		} else {
		// Get next session's start sector
		uint32_t nextStartSector;
		if (!STORE_GetSessionInfo(sessionIndex + 1, &nextStartSector, NULL)) {
			return false;
		}
		*sectorCount = nextStartSector - *startSector;
	}

	// Validate sector count is a multiple of SECTORS_PER_FRAME
	if (*sectorCount % SECTORS_PER_FRAME != 0) {
		return false;  // Corrupted session data
	}

	return true;
}