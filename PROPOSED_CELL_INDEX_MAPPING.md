# Proposed Cell Index Mapping for ModuleCPU

## Problem Statement

The VUART cell reporting chain works in **reverse order** from what the Pack Controller expects:

### Current Situation:
1. **Physical Layout**: Cells are numbered 0-93 from module perspective
2. **VUART Chain Order**:
   - Last functional cell (furthest from module) reports FIRST
   - Each cell forwards downstream data then appends its own
   - First cell (closest to module) reports LAST

3. **Storage in StringData[]**:
   ```
   StringData[0] = Last functional cell in chain (first received)
   StringData[1] = Second-to-last functional cell
   ...
   StringData[N-1] = First cell (last received, closest to module)
   ```

4. **Pack Controller Expectation**:
   - Requests "Cell 0" expecting the first physical cell
   - Currently gets the last functional cell instead

## Example Scenarios

### Scenario 1: Full Chain (3 cells working)
```
Physical:    [Cell 0] -> [Cell 1] -> [Cell 2]
VUART order: Cell 2 reports, Cell 1 forwards+reports, Cell 0 forwards+reports
Storage:     StringData[0]=Cell2, StringData[1]=Cell1, StringData[2]=Cell0
Pack asks:   "Give me Cell 0"
Currently:   Returns Cell 2 data (WRONG!)
Should:      Return Cell 0 data
```

### Scenario 2: Partial Chain (Cell 2 dead, only 2 cells report)
```
Physical:    [Cell 0] -> [Cell 1] -> [Cell 2-DEAD]
VUART order: Cell 1 reports, Cell 0 forwards+reports
Storage:     StringData[0]=Cell1, StringData[1]=Cell0
Pack asks:   "Give me Cell 0"
Currently:   Returns Cell 1 data (WRONG!)
Should:      Return Cell 0 data
```

## Proposed Solution

### Critical Understanding: Cells Received vs Expected
**IMPORTANT**: The physical identity of the cell at StringData[0] depends on `cellsReceived`, NOT `cellsExpected`!

- If 13 cells expected but only 2 report: StringData[0] = Physical Cell 1 (the 2nd cell)
- If 13 cells expected but only 12 report: StringData[0] = Physical Cell 11 (the 12th cell)
- If all 13 report: StringData[0] = Physical Cell 12 (the 13th cell)

### Mapping Formula
When Pack Controller requests cell `RequestedIndex`, we need to map it to the correct position in StringData[]:

```c
// For MODULE_DETAIL response
uint8_t GetMappedCellIndex(uint8_t requestedIndex, uint8_t cellsReceived) {
    if (cellsReceived == 0) {
        return 0xFF;  // No cells reported
    }

    // CRITICAL: The last FUNCTIONAL cell (not last expected) is at StringData[0]
    // If only 2 cells report out of 13 expected:
    //   - StringData[0] contains Cell 1 (the 2nd physical cell)
    //   - StringData[1] contains Cell 0 (the 1st physical cell)
    //   - Cells 2-12 didn't report (chain broken after cell 1)

    // The physical cell number at StringData[0] is always (cellsReceived - 1)
    // regardless of how many were expected

    if (requestedIndex >= cellsReceived) {
        return 0xFF;  // This cell didn't report
    }

    // Map requested physical cell to StringData position
    return (cellsReceived - 1) - requestedIndex;
}
```

### Implementation in MODULE_DETAIL Handler

```c
if (sg_bSendCellStatus) {
    uint16_t u16Voltage = 0;
    int16_t s16Temperature = 0;

    // sg_u8CellStatus is the cell ID requested by Pack Controller
    uint8_t requestedCellId = sg_u8CellStatus;

    // CRITICAL: Use actual received count, not expected count!
    // sg_sFrame.sg_u8CellCPUCount tracks how many cells actually reported
    uint8_t cellsReceived = sg_sFrame.sg_u8CellCPUCount;

    // Map to actual position in StringData based on how many cells reported
    uint8_t actualIndex = GetMappedCellIndex(requestedCellId, cellsReceived);

    if (actualIndex != 0xFF && actualIndex < MAX_CELLS) {
        // Get data from the mapped position
        uint16_t u16RawVoltage = sg_sFrame.StringData[actualIndex].voltage;
        int16_t s16RawTemp = sg_sFrame.StringData[actualIndex].temperature;

        // Convert as before...
        if (!CellDataConvertVoltage(u16RawVoltage, &u16Voltage)) {
            u16Voltage = 0;
        }

        if (CellDataConvertTemperature(s16RawTemp, &s16Temperature)) {
            // Temperature is valid
        } else {
            s16Temperature = 0;
        }
    }

    // Send response with REQUESTED cell ID (not mapped index)
    pu8Response[0] = requestedCellId;  // Pack expects this to match request
    pu8Response[1] = sg_sFrame.sg_u8CellCountExpected;
    // ... rest of response
}
```

## Edge Cases to Consider

### Example 1: 13 cells expected, only 2 report (chain break after Cell 1)
```
Physical cells:  [Cell 0] -> [Cell 1] -> [Cell 2-DEAD] -> ... [Cell 12-DEAD]
Cells received:  2
StringData[0]:   Cell 1 (last functional cell, physical position 1)
StringData[1]:   Cell 0 (first cell, physical position 0)

Pack requests Cell 0: Map (2-1) - 0 = 1 → StringData[1] = Cell 0 ✓
Pack requests Cell 1: Map (2-1) - 1 = 0 → StringData[0] = Cell 1 ✓
Pack requests Cell 2: 2 >= 2, return 0xFF (didn't report) ✓
```

### Example 2: 13 cells expected, 12 report (Cell 12 dead)
```
Physical cells:  [Cell 0] -> ... -> [Cell 11] -> [Cell 12-DEAD]
Cells received:  12
StringData[0]:   Cell 11 (last functional cell)
StringData[11]:  Cell 0 (first cell)

Pack requests Cell 0:  Map (12-1) - 0 = 11 → StringData[11] = Cell 0 ✓
Pack requests Cell 11: Map (12-1) - 11 = 0 → StringData[0] = Cell 11 ✓
Pack requests Cell 12: 12 >= 12, return 0xFF (didn't report) ✓
```

### Example 3: All 13 cells working
```
Physical cells:  [Cell 0] -> ... -> [Cell 12]
Cells received:  13
StringData[0]:   Cell 12 (last cell in chain)
StringData[12]:  Cell 0 (first cell in chain)

Pack requests Cell 0:  Map (13-1) - 0 = 12 → StringData[12] = Cell 0 ✓
Pack requests Cell 12: Map (13-1) - 12 = 0 → StringData[0] = Cell 12 ✓
```

### Example 4: Single cell (only Cell 0 working)
```
Physical cells:  [Cell 0] -> [Cell 1-DEAD] -> ...
Cells received:  1
StringData[0]:   Cell 0 (only working cell)

Pack requests Cell 0: Map (1-1) - 0 = 0 → StringData[0] = Cell 0 ✓
Pack requests Cell 1: 1 >= 1, return 0xFF (didn't report) ✓
```

### Example 5: No cells reporting
```
Cells received:  0
Pack requests any cell: return 0xFF (no data available) ✓
```

## Alternative Approach: Reverse Storage Order

Instead of changing the lookup, we could change how we store data:

```c
// In vUARTRXData when storing cell data:
uint8_t storageIndex = (sg_sFrame.sg_u8CellCountExpected - 1) - sg_u8CellIndex;
if (storageIndex < MAX_CELLS) {
    *(uint32_t*)&(sg_sFrame.StringData[storageIndex]) = *((uint32_t*)sg_u8CellBufferTemp);
}
```

**Pros**: No mapping needed at read time
**Cons**: Need to know expected count during reception, breaks if count changes

## Recommendation

Use the **Mapping Formula** approach because:
1. Minimal code change (only in MODULE_DETAIL handler)
2. Storage order remains consistent with reception order
3. Works correctly with partial chains
4. Easy to understand and debug
5. No changes to VUART reception code

## Protocol for Non-Reporting Cells

### Current ModuleCPU Behavior
When Pack Controller requests a cell that didn't report:
1. ModuleCPU **always sends a response** (never ignores the request)
2. Response contains:
   - Cell ID as requested (byte 0)
   - Expected cell count (byte 1)
   - **Temperature = 0** (bytes 2-3)
   - **Voltage = 0** (bytes 4-5)
   - **SOC = 0** (byte 6)
   - **SOH = 0** (byte 7)

### Pack Controller Processing
The Pack Controller handles zero values as follows:
1. **Stores the zero values** in cellVoltages[] and cellTemperatures[]
2. **Tracks timestamp** to know when data was received
3. **Display logic**:
   - If voltage == 0.0f AND never received (no timestamp): Shows "---"
   - If voltage == 0.0f BUT has timestamp: Shows "0.000" (actual zero received)
   - This distinguishes between "never heard from" vs "heard but value is zero"

### Important Considerations
- **Always respond** to MODULE_DETAIL requests, even for non-reporting cells
- Send **actual zeros** (0x0000) not special markers
- Pack Controller uses the zero voltage to identify non-reporting cells
- Temperature of 0 raw = -55.35°C after offset, which is clearly invalid

### Implementation for Mapping
```c
if (sg_bSendCellStatus) {
    uint8_t requestedCellId = sg_u8CellStatus;
    uint8_t cellsReceived = sg_sFrame.sg_u8CellCPUCount;

    uint16_t u16Voltage = 0;
    int16_t s16Temperature = 0;

    // Check if this cell reported
    if (requestedCellId < cellsReceived) {
        // Cell did report - map and retrieve data
        uint8_t actualIndex = (cellsReceived - 1) - requestedCellId;

        if (actualIndex < MAX_CELLS) {
            uint16_t u16RawVoltage = sg_sFrame.StringData[actualIndex].voltage;
            int16_t s16RawTemp = sg_sFrame.StringData[actualIndex].temperature;

            // Convert voltage and temperature...
            CellDataConvertVoltage(u16RawVoltage, &u16Voltage);
            CellDataConvertTemperature(s16RawTemp, &s16Temperature);
        }
    }
    // else: Leave as zeros for non-reporting cell

    // Always send response
    pu8Response[0] = requestedCellId;
    pu8Response[1] = sg_sFrame.sg_u8CellCountExpected;
    pu8Response[2] = (uint8_t) s16Temperature;
    pu8Response[3] = (uint8_t) (s16Temperature >> 8);
    pu8Response[4] = (uint8_t) u16Voltage;
    pu8Response[5] = (uint8_t) (u16Voltage >> 8);
    // ... SOC and SOH calculations
}
```

## Testing Required

1. Full chain - verify all cells report correctly
2. Last cell dead - verify remaining cells map correctly
3. First cell dead - chain broken, no cells report
4. Middle cell dead - partial chain maps correctly
5. Single cell - maps to itself
6. No cells - returns appropriate error/zero data
7. **Non-reporting cells** - verify zeros are sent and Pack Controller shows "---" or "0.000" appropriately