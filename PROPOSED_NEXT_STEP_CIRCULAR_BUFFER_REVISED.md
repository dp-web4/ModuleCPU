# Revised Proposal: Circular Buffer with Partial String Handling

## Current State
- Cell data accessed directly from sg_sFrame.c buffer (896 bytes)
- Helper functions GetStringData() provide access to buffer
- Single string reading at start of buffer (legacy behavior preserved)
- System working and reporting to pack controller

## Key Change: Always Advance, Mark Invalid Cells

### 1. Define Invalid Cell Marker
**Location**: STORE.h or main.h

```c
// Invalid cell marker - set both voltage and temperature to max value
#define INVALID_CELL_VOLTAGE  0xFFFF
#define INVALID_CELL_TEMP     0x7FFF  // Max int16_t

// Or use existing discharge bit pattern if available
// #define INVALID_CELL_MARKER   0xFFFF
```

### 2. Initialize Buffer Slot with Invalid Markers
**Location**: main.c, at START of READ frame (before collecting cells)

```c
// At start of READ frame - mark ALL cells as invalid
volatile CellData* stringData = GetStringDataVolatile(&sg_sFrame);
for (uint8_t i = 0; i < sg_sFrame.m.sg_u8CellCountExpected; i++) {
    stringData[i].voltage = INVALID_CELL_VOLTAGE;
    stringData[i].temperature = INVALID_CELL_TEMP;
}

// As cells report in, they overwrite the invalid markers
// Any cells that don't report remain marked invalid
```

### 3. Always Advance After WRITE Frame
**Location**: main.c, at end of WRITE frame processing

```c
// After WRITE frame - advance buffer regardless of cell count
// This ensures we don't get stuck if cells are missing

// Note: sg_u8CellCPUCount tells us how many actually reported
// sg_u8CellCountExpected tells us how many we expected

// Store actual count for this reading (for diagnostics)
// Could add a field like actualCellCounts[nstrings] if needed

// Advance to next slot
sg_sFrame.m.currentIndex = (sg_sFrame.m.currentIndex + 1) % sg_sFrame.m.nstrings;

if (sg_sFrame.m.readingCount < sg_sFrame.m.nstrings) {
    sg_sFrame.m.readingCount++;
}

// Increment frame number for this reading (complete or partial)
sg_sFrame.m.frameNumber++;
```

### 4. Initialize nstrings Based on Cell Count
**Location**: main.c, ProcessCellExpectedCount() function

```c
void ProcessCellExpectedCount(uint8_t u8CellCountExpected)
{
    sg_sFrame.m.sg_u8CellCountExpected = u8CellCountExpected;

    // Calculate how many complete string readings fit in buffer
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

    // Clear entire buffer to invalid markers initially
    CellData* buffer = (CellData*)sg_sFrame.c;
    uint16_t totalCells = (FRAME_CELLBUFFER_SIZE / sizeof(CellData));
    for (uint16_t i = 0; i < totalCells; i++) {
        buffer[i].voltage = INVALID_CELL_VOLTAGE;
        buffer[i].temperature = INVALID_CELL_TEMP;
    }
}
```

### 5. CAN Response Handling for Invalid Cells
**Location**: main.c, MODULE_DETAIL message handler

```c
// When reporting cell data over CAN
volatile CellData* stringData = GetLatestCompleteString(&sg_sFrame);

// Check if this cell is valid
if (stringData[actualIndex].voltage == INVALID_CELL_VOLTAGE) {
    // Report invalid cell status
    // Could send special values or skip this cell
    u16Voltage = 0;  // Or some other indicator
    s16Temperature = 0;
    // Maybe set a flag in the message
} else {
    // Normal processing
    uint16_t u16RawVoltage = stringData[actualIndex].voltage;
    int16_t s16RawTemp = stringData[actualIndex].temperature;
    // ... rest of processing
}
```

## Benefits of This Approach

1. **Never Gets Stuck**: Always advances even if cells drop out
2. **Clear Invalid Data**: Invalid markers make it obvious which cells didn't report
3. **Complete History**: Can see pattern of cell dropouts over time
4. **Power Loss Safe**: Invalid markers persist in frame

## Invalid Cell Detection Examples

### Normal Operation (all cells report):
- Slot initialized with all INVALID markers
- All cells overwrite their markers with real data
- Buffer advances with complete data

### Cell 5 Not Reporting:
- Slot initialized with all INVALID markers
- Cells 0-4, 6-N overwrite with real data
- Cell 5 remains INVALID_CELL_VOLTAGE/INVALID_CELL_TEMP
- Buffer advances, preserving record that cell 5 was missing

### Multiple Cells Missing:
- Each missing cell retains its INVALID marker
- Pack controller can see exactly which cells are problematic

## Testing Considerations

1. **Verify Invalid Markers**: Check that non-reporting cells show invalid
2. **CAN Message Handling**: Ensure pack controller handles invalid cell markers
3. **Statistics**: sg_u8CellCPUCount still tracks actual cells received
4. **Buffer Analysis**: Can scan buffer to see dropout patterns

## Summary of Changes

1. Always initialize slots with invalid markers at READ start
2. Always advance buffer at WRITE end (not conditional)
3. CAN responses check for and handle invalid markers
4. Pack controller can track cell reliability over time