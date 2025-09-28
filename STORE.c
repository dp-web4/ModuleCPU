#include "STORE.h"
#include "SD.h"
#include <string.h>

static GlobalState gState;
static uint32_t currentSector;
static uint8_t __attribute__((aligned(4))) frameBuffer[FRAME_BUFFER_SIZE];  // Enlarged to handle full frame

static bool readGlobalState(void) {
	if (!SDRead(0, frameBuffer, 1)) {
		return false;
	}
	memcpy(&gState, frameBuffer, sizeof(GlobalState));
	// TODO: Implement checksum verification
	return true;
}

static bool writeGlobalState(void) {
	memcpy(frameBuffer, &gState, sizeof(GlobalState));
	// TODO: Calculate and update checksum
	return SDWrite(0, frameBuffer, 1);
}

static bool updateSessionMap(void) {
	uint32_t mapSector = gState.activeSessionMapSector;
	uint32_t mapOffset = gState.activeSessionMapOffset;

	if (!SDRead(mapSector, frameBuffer, 1)) {
		return false;
	}

	// Update session map
	*(uint64_t*)(frameBuffer + mapOffset) = gState.newSessionSector;

	if (!SDWrite(mapSector, frameBuffer, 1)) {
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

	// Copy frame data to our buffer
	memcpy(frameBuffer, (const void*)frame, frame->m.frameBytes);

	// Zero out any remaining buffer space in last sector
	if(frame->m.frameBytes < FRAME_BUFFER_SIZE) {
		memset(frameBuffer + frame->m.frameBytes, 0, FRAME_BUFFER_SIZE - frame->m.frameBytes);
	}
	
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