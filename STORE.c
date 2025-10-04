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
}

bool STORE_Init(void) {
	if (!SDInit()) {
		return false;
	}

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
}

bool STORE_WriteFrame(volatile FrameData* frame) {
	uint32_t bytesToWrite;
	uint32_t bytesWritten = 0;
	uint32_t currentOffset = 0;
	uint32_t sectorsToWrite;

	// Verify frame size
	if(frame->m.frameBytes > FRAME_BUFFER_SIZE) {
		return false;
	}

	// Copy ENTIRE frame data to our buffer (always 1024 bytes)
	// This ensures frameBuffer contains complete frame for CAN transfer
	memcpy(frameBuffer, (const void*)frame, FRAME_BUFFER_SIZE);
	
	// Calculate how many complete sectors we need to write
	sectorsToWrite = SECTORS_PER_FRAME;
	
	// Write all sectors
	while(sectorsToWrite > 0) {
		if(!SDWrite(currentSector, frameBuffer + currentOffset, 1)) {
			return false;
		}
		currentSector++;
		currentOffset += SECTOR_SIZE;
		sectorsToWrite--;
	}
	
	return true;
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