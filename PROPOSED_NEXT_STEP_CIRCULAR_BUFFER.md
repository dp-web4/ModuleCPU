# Proposed: Initialize nstrings and Implement Circular Buffer

## Current State
- Cell data accessed directly from sg_sFrame.c buffer (896 bytes)
- Helper functions GetStringData() provide access to buffer
- Single string reading at start of buffer (legacy behavior preserved)
- System working and reporting to pack controller

## Proposed Changes

### 1. Initialize nstrings Based on Cell Count
**Location**: main.c, ProcessCellExpectedCount() function (around line 542)

```c
// After setting sg_u8CellCountExpected, calculate nstrings
void ProcessCellExpectedCount(uint8_t u8CellCountExpected)
{
    sg_sFrame.m.sg_u8CellCountExpected = u8CellCountExpected;

    // Calculate how many complete string readings fit in buffer
    // Each cell = 4 bytes (2 bytes voltage + 2 bytes temperature)
    uint16_t bytes_per_string = u8CellCountExpected * sizeof(CellData);

    if (bytes_per_string > 0) {
        sg_sFrame.m.nstrings = FRAME_CELLBUFFER_SIZE / bytes_per_string;

        // Ensure at least 1 string
        if (sg_sFrame.m.nstrings == 0) {
            sg_sFrame.m.nstrings = 1;
        }
    } else {
        sg_sFrame.m.nstrings = 1;
    }

    // Initialize circular buffer indices
    sg_sFrame.m.currentIndex = 0;
    sg_sFrame.m.readingCount = 0;

    // Rest of existing voltage calculations...
}
```

### 2. Update Helper Functions for Multiple Readings
**Location**: STORE.h, replace current helper functions

```c
// Get pointer to a specific string reading slot (0 to nstrings-1)
static inline CellData* GetStringDataAtIndex(FrameData* frame, uint16_t index) {
    uint16_t bytes_per_string = frame->m.sg_u8CellCountExpected * sizeof(CellData);
    uint16_t offset = index * bytes_per_string;
    return (CellData*)(&frame->c[offset]);
}

static inline volatile CellData* GetStringDataAtIndexVolatile(volatile FrameData* frame, uint16_t index) {
    uint16_t bytes_per_string = frame->m.sg_u8CellCountExpected * sizeof(CellData);
    uint16_t offset = index * bytes_per_string;
    return (volatile CellData*)(&frame->c[offset]);
}

// Get pointer to current writing slot
static inline CellData* GetStringData(FrameData* frame) {
    return GetStringDataAtIndex(frame, frame->m.currentIndex);
}

static inline volatile CellData* GetStringDataVolatile(volatile FrameData* frame) {
    return GetStringDataAtIndexVolatile(frame, frame->m.currentIndex);
}

// Get pointer to most recent complete reading (for CAN responses)
static inline volatile CellData* GetLatestCompleteString(volatile FrameData* frame) {
    // If we have at least one complete reading, return the previous slot
    if (frame->m.readingCount > 0) {
        uint16_t lastIndex = (frame->m.currentIndex == 0) ?
                             (frame->m.nstrings - 1) :
                             (frame->m.currentIndex - 1);
        return GetStringDataAtIndexVolatile(frame, lastIndex);
    }
    // Otherwise return current slot (may be incomplete)
    return GetStringDataVolatile(frame);
}
```

### 3. Advance Buffer After Complete String
**Location**: main.c, at end of WRITE frame processing (around line 2635)

```c
// After processing all cells in WRITE frame
if (sg_sFrame.m.sg_u8CellCPUCount == sg_sFrame.m.sg_u8CellCountExpected) {
    // We have a complete string reading, advance to next slot
    sg_sFrame.m.currentIndex = (sg_sFrame.m.currentIndex + 1) % sg_sFrame.m.nstrings;

    if (sg_sFrame.m.readingCount < sg_sFrame.m.nstrings) {
        sg_sFrame.m.readingCount++;
    }

    // Increment frame number for this complete reading
    sg_sFrame.m.frameNumber++;
}
```

### 4. Update CAN Response to Use Latest Complete Reading
**Location**: main.c, MODULE_DETAIL message handler (around line 1912)

```c
// Change from GetStringDataVolatile(&sg_sFrame) to:
volatile CellData* stringData = GetLatestCompleteString(&sg_sFrame);
```

## Examples with Different Cell Counts

### 20 Cells
- Bytes per string: 20 * 4 = 80 bytes
- Number of strings: 896 / 80 = 11 complete readings
- Granularity: ~5.5 seconds of history at 2Hz

### 50 Cells
- Bytes per string: 50 * 4 = 200 bytes
- Number of strings: 896 / 200 = 4 complete readings
- Granularity: ~2 seconds of history at 2Hz

### 94 Cells (maximum)
- Bytes per string: 94 * 4 = 376 bytes
- Number of strings: 896 / 376 = 2 complete readings
- Granularity: ~1 second of history at 2Hz

## Key Design Decisions

1. **When to Advance**: Only advance after COMPLETE string (all expected cells received)
2. **What CAN Reports**: Always report the most recent COMPLETE reading, not partial
3. **Frame Number**: Increment on each complete reading for tracking
4. **Power Loss**: All readings preserved in frame, can recover history

## Testing Considerations

1. **Partial Strings**: If cells drop out, don't advance buffer (keep overwriting current slot)
2. **CAN Queries**: Ensure MODULE_DETAIL always returns valid data
3. **Buffer Wraparound**: Test that circular buffer properly wraps at nstrings boundary
4. **Frame Counter**: Verify frameNumber increments correctly

## Questions for Discussion

1. **Should we advance on partial strings?** Current proposal: NO - only complete strings
2. **Should frame number increment per reading or per SD write?** Current proposal: per reading
3. **Do we need a timestamp per reading or just per frame?** Current proposal: just per frame
4. **Should we clear the buffer slot before starting a new reading?** Current proposal: no need, we overwrite

## Risk Assessment
- **Low Risk**: Changes are additive, don't break existing functionality
- **Main Risk**: CAN responses during buffer transition (mitigated by GetLatestCompleteString)
- **Testing**: Can add debug counters to verify buffer cycling