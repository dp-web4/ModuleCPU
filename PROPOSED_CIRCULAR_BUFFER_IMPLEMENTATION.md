# Proposed Next Step: Implement Circular Buffer Logic for Multiple String Readings

## Current State
- FrameMetadata structure properly separated from cell buffer
- 896 bytes available for cell data storage (1024 - 128 bytes metadata)
- All code references updated to use sg_sFrame.m.xxx
- System compiles and runs correctly

## Proposed Implementation Steps

### Step 1: Initialize nstrings and Granularity
**Location**: main.c, in initialization code where sg_u8CellCountExpected is set

**Changes**:
1. Calculate how many complete string readings can fit in the buffer
2. Initialize sg_sFrame.m.nstrings based on cell count and buffer size
3. Set granularity (cells per reading slot)

**Formula**:
```c
// Each cell reading is 4 bytes (uint16_t voltage + int16_t temperature)
int bytes_per_string = sg_sFrame.m.sg_u8CellCountExpected * sizeof(CellData);
sg_sFrame.m.nstrings = FRAME_CELLBUFFER_SIZE / bytes_per_string;

// Ensure at least 1 string can be stored
if (sg_sFrame.m.nstrings == 0) {
    sg_sFrame.m.nstrings = 1;
}

// Initialize circular buffer indices
sg_sFrame.m.currentIndex = 0;
sg_sFrame.m.readingCount = 0;
```

### Step 2: Create Helper Functions for Buffer Access
**Location**: New inline functions in STORE.h or main.c

**Functions**:
```c
// Get pointer to specific string reading in buffer
static inline CellData* GetStringReading(FrameData* frame, uint16_t index) {
    int bytes_per_string = frame->m.sg_u8CellCountExpected * sizeof(CellData);
    int offset = index * bytes_per_string;
    return (CellData*)(&frame->c[offset]);
}

// Get pointer to current string reading slot
static inline CellData* GetCurrentStringSlot(FrameData* frame) {
    return GetStringReading(frame, frame->m.currentIndex);
}

// Advance to next buffer slot
static inline void AdvanceBufferIndex(FrameData* frame) {
    frame->m.currentIndex = (frame->m.currentIndex + 1) % frame->m.nstrings;
    if (frame->m.readingCount < frame->m.nstrings) {
        frame->m.readingCount++;
    }
}
```

### Step 3: Update String Data Collection
**Location**: main.c, where cell data is collected (VUART reception)

**Current Code**: Uses sg_sFrame.m.StringData[cellIndex]
**New Code**: Use GetCurrentStringSlot() to get pointer to current reading location

```c
// Instead of: sg_sFrame.m.StringData[u8CellIndex] = cellData;
CellData* currentString = GetCurrentStringSlot(&sg_sFrame);
currentString[u8CellIndex] = cellData;
```

### Step 4: Update Frame Writing Logic
**Location**: main.c, in WRITE frame processing

**Changes**:
1. After collecting a complete string reading, call AdvanceBufferIndex()
2. Update frame counter and timestamp
3. When writing to SD, the entire 1024-byte frame is written (including all readings)

### Step 5: Remove Legacy StringData Array
**Location**: STORE.h, FrameMetadata structure

**Action**: Remove the temporary `CellData StringData[MAX_CELLS]` field once buffer access is working

## Benefits of This Approach
1. **Minimal Code Changes**: Existing cell data collection logic remains mostly unchanged
2. **Power-Loss Resilient**: Multiple readings preserved in single frame
3. **Efficient Storage**: Maximizes use of 896-byte buffer
4. **Simple Access**: Helper functions provide clean interface to circular buffer
5. **Backward Compatible**: Can still access most recent reading easily

## Testing Strategy
1. Verify nstrings calculation for different cell counts (20, 50, 94 cells)
2. Test circular buffer wraparound
3. Verify frame writes contain multiple readings
4. Test power-loss recovery (readings preserved)

## Risk Assessment
- **Low Risk**: Changes are localized and incremental
- **Fallback**: Can easily revert by keeping StringData array temporarily
- **Testing**: Can run side-by-side comparison with old method

## Recommendation
Proceed with Step 1 (Initialize nstrings) first, test that it works correctly, then move to Step 2 (Helper Functions), and so on. This incremental approach minimizes risk and allows for testing at each stage.