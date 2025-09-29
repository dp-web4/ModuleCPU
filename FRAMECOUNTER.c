#include "FRAMECOUNTER.h"
#include "EEPROM.h"
#include <string.h>

// Number of counter positions for wear leveling
#define COUNTER_POSITIONS   128  // 512 bytes / 4 bytes per counter = 128 positions
#define BYTES_PER_COUNTER   4

// Invalid counter value marker
#define COUNTER_INVALID     0xFFFFFFFF

// Current state
static uint32_t sg_u32CurrentCounter = 0;
static uint8_t sg_u8CurrentPosition = 0;
static uint8_t sg_u8UpdateCounter = 0;  // Update EEPROM every 256 increments

// Read a 32-bit counter from EEPROM at given position
static uint32_t ReadCounterAtPosition(uint8_t position) {
    uint16_t addr = EEPROM_FRAME_COUNTER_BASE + (position * BYTES_PER_COUNTER);
    uint32_t value = 0;

    // Read 4 bytes (MSB first for easier debugging)
    value  = ((uint32_t)EEPROMRead(addr) << 24);
    value |= ((uint32_t)EEPROMRead(addr + 1) << 16);
    value |= ((uint32_t)EEPROMRead(addr + 2) << 8);
    value |= ((uint32_t)EEPROMRead(addr + 3));

    return value;
}

// Write a 32-bit counter to EEPROM at given position
static void WriteCounterAtPosition(uint8_t position, uint32_t value) {
    uint16_t addr = EEPROM_FRAME_COUNTER_BASE + (position * BYTES_PER_COUNTER);

    // Only write bytes that have changed (wear optimization)
    uint32_t current = ReadCounterAtPosition(position);

    // Write MSB first
    if ((uint8_t)(value >> 24) != (uint8_t)(current >> 24)) {
        EEPROMWrite(addr, (uint8_t)(value >> 24));
    }
    if ((uint8_t)(value >> 16) != (uint8_t)(current >> 16)) {
        EEPROMWrite(addr + 1, (uint8_t)(value >> 16));
    }
    if ((uint8_t)(value >> 8) != (uint8_t)(current >> 8)) {
        EEPROMWrite(addr + 2, (uint8_t)(value >> 8));
    }
    if ((uint8_t)(value) != (uint8_t)(current)) {
        EEPROMWrite(addr + 3, (uint8_t)(value));
    }
}

// Find the position with the highest valid counter value
static void FindCurrentPosition(void) {
    uint32_t maxCounter = 0;
    uint8_t maxPosition = 0;
    bool foundValid = false;

    // Scan all positions to find highest counter
    for (uint8_t pos = 0; pos < COUNTER_POSITIONS; pos++) {
        uint32_t value = ReadCounterAtPosition(pos);

        // Skip invalid entries
        if (value == COUNTER_INVALID) {
            continue;
        }

        // Check if this is the highest value so far
        // Handle wrap-around: if difference is huge, assume wrap
        if (!foundValid || value > maxCounter) {
            maxCounter = value;
            maxPosition = pos;
            foundValid = true;
        }
    }

    if (foundValid) {
        sg_u32CurrentCounter = maxCounter;
        sg_u8CurrentPosition = maxPosition;
    } else {
        // No valid counter found, initialize first position
        sg_u32CurrentCounter = 0;
        sg_u8CurrentPosition = 0;
        WriteCounterAtPosition(0, 0);
    }
}

// Initialize frame counter system
void FrameCounter_Init(void) {
    // Find the current counter value and position
    FindCurrentPosition();

    // Reset update counter
    sg_u8UpdateCounter = 0;
}

// Get current counter value
uint32_t FrameCounter_Get(void) {
    return sg_u32CurrentCounter;
}

// Increment and persist counter
void FrameCounter_Increment(void) {
    // Increment the counter
    sg_u32CurrentCounter++;
    sg_u8UpdateCounter++;

    // Only update EEPROM every 256 increments to reduce wear
    if (sg_u8UpdateCounter == 0) {  // Wrapped around from 255 to 0
        // Move to next position for wear leveling
        uint8_t nextPosition = (sg_u8CurrentPosition + 1) % COUNTER_POSITIONS;

        // Mark current position as invalid before moving
        // This helps with recovery if power is lost during transition
        WriteCounterAtPosition(sg_u8CurrentPosition, COUNTER_INVALID);

        // Write new counter to next position
        sg_u8CurrentPosition = nextPosition;
        WriteCounterAtPosition(sg_u8CurrentPosition, sg_u32CurrentCounter);
    } else if ((sg_u8UpdateCounter & 0x0F) == 0) {
        // Every 16 increments, update current position (but don't move)
        // This balances wear vs. data loss on power failure
        WriteCounterAtPosition(sg_u8CurrentPosition, sg_u32CurrentCounter);
    }
}

// Get current wear leveling position (for diagnostics)
uint8_t FrameCounter_GetPosition(void) {
    return sg_u8CurrentPosition;
}