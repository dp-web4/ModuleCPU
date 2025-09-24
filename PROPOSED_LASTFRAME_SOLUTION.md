# Proposed Solution: Use Last Complete Frame for MODULE_DETAIL Responses

## Problem
`sg_sFrame` is the active frame being filled with cell data. Its `sg_u8CellCPUCount` changes constantly as cells report and frames restart. When MODULE_DETAIL requests come in, we're reading from an incomplete/changing frame, causing inconsistent results.

## Solution
Create a separate "last complete frame" structure that holds a stable copy of the most recent complete frame. All MODULE_DETAIL responses read from this stable copy.

## Implementation

### 1. Add Last Complete Frame Storage

```c
// In main.c globals section (around line 239)
// Add a second frame structure for the last complete frame
volatile static FrameData sg_sLastCompleteFrame;  // Last complete frame for reporting
volatile static uint8_t sg_u8LastFrameCellCount = 0;  // How many cells reported in last complete frame
```

### 2. Copy Frame When Complete

```c
// In main.c, after writing frame to SD (around line 2178)
if ((sg_bSDCardReady) && (EMODSTATE_OFF != sg_eModuleControllerStateCurrent))
{
    sg_bSDCardReady = STORE_WriteFrame(&sg_sFrame);

    // Copy completed frame to last complete frame for stable reporting
    memcpy((void*)&sg_sLastCompleteFrame, (const void*)&sg_sFrame, sizeof(FrameData));
    sg_u8LastFrameCellCount = sg_sFrame.sg_u8CellCPUCount;
}
```

### 3. Update MODULE_DETAIL Handler

```c
if( sg_bSendCellStatus )
{
    bool bSuccess;
    uint16_t u16Voltage = 0;
    int16_t s16Temperature = 0;

    // sg_u8CellStatus is the cell ID requested by Pack Controller (0-based)
    uint8_t requestedCellId = sg_u8CellStatus;

    // Use the last complete frame count, not the active frame
    uint8_t cellsReceived = sg_u8LastFrameCellCount;

    // Map the requested cell ID to the actual position in StringData
    // The last functional cell (cellsReceived-1) is at StringData[0]
    // The first cell (Cell 0) is at StringData[cellsReceived-1]
    // Mapping: actualIndex = (cellsReceived - 1) - requestedCellId

    if (requestedCellId < cellsReceived && cellsReceived > 0)
    {
        // Cell did report - calculate mapped index
        uint8_t actualIndex = (cellsReceived - 1) - requestedCellId;

        if (actualIndex < MAX_CELLS)
        {
            // Get raw values from the LAST COMPLETE FRAME
            uint16_t u16RawVoltage = sg_sLastCompleteFrame.StringData[actualIndex].voltage;
            int16_t s16RawTemp = sg_sLastCompleteFrame.StringData[actualIndex].temperature;

            // Convert voltage to millivolts using CellDataConvertVoltage
            if (!CellDataConvertVoltage(u16RawVoltage, &u16Voltage))
            {
                u16Voltage = 0;
            }

            // Convert temperature
            if (CellDataConvertTemperature(s16RawTemp, &s16Temperature))
            {
                // Temperature is valid
            }
            else
            {
                s16Temperature = 0;
            }
        }
    }
    // else: Cell hasn't reported - leave as zeros

    // Always send the response
    pu8Response[0] = requestedCellId;
    pu8Response[1] = sg_sLastCompleteFrame.sg_u8CellCountExpected;
    // ... rest of response
}
```

## Benefits

1. **Stable Data**: MODULE_DETAIL always reads from a complete, unchanging frame
2. **Correct Cell Count**: `sg_u8LastFrameCellCount` reflects actual cells in the complete frame
3. **No Race Conditions**: Reading doesn't interfere with active frame collection
4. **Consistent Mapping**: The reverse order mapping works correctly with stable data

## Testing

1. Verify all cells report correctly when chain is complete
2. Verify partial chains map correctly
3. Verify data doesn't change mid-query during cell polling
4. Check memory usage (adds one more FrameData structure ~450 bytes)

## Alternative Approach

If memory is tight, instead of copying the entire frame, we could:
1. Only copy StringData[] and cell count when frame completes
2. Use a flag to indicate which frame buffer is "active" vs "complete" (double buffering)