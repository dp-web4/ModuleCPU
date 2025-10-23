# Unprogrammed EEPROM Handling

## Overview
Added support for testing and debugging with unprogrammed EEPROM. When EEPROM is unprogrammed (unique ID = 0xFFFFFFFF), the module uses default values and auto-discovers the cell count.

## Changes Made

### 1. New Default UID Constant
**Location**: `main.c:100`

```c
#define EEPROM_UNPROGRAMMED_DEFAULT_UID		0xBA77BABE
```

Default unique ID used when EEPROM is unprogrammed (testing/debug mode).

### 2. EEPROM Valid Flag
**Location**: `main.c:225`

```c
volatile static bool __attribute__((section(".noinit"))) sg_bEEPROMValid;
```

- Stored in `.noinit` section to survive watchdog resets
- `true`: EEPROM is programmed (normal operation)
- `false`: EEPROM is unprogrammed (testing/debug mode with defaults)

### 3. Modified ModuleControllerGetUniqueID()
**Location**: `main.c:403-419`

Now checks if EEPROM unique ID is 0xFFFFFFFF:
- If unprogrammed: Returns `EEPROM_UNPROGRAMMED_DEFAULT_UID` (0xBA77BABE)
- If programmed: Returns actual unique ID from EEPROM

### 4. EEPROM Valid Check During Initialization
**Location**: `main.c:2746-2757`

During module initialization:
```c
uint32_t u32UniqueID = ModuleControllerGetUniqueID();
if (u32UniqueID == EEPROM_UNPROGRAMMED_DEFAULT_UID)
{
    sg_bEEPROMValid = false;  // Unprogrammed EEPROM
}
else
{
    sg_bEEPROMValid = true;   // Programmed EEPROM
}
```

### 5. Cell Count Expected Initialization
**Location**: `main.c:2501-2511`

```c
if (sg_bEEPROMValid)
{
    CellCountExpectedSet(EEPROMRead(EEPROM_EXPECTED_CELL_COUNT));
}
else
{
    // EEPROM unprogrammed - start with 0 cells expected
    // This will auto-increment as cells are discovered
    CellCountExpectedSet(0);
}
```

### 6. Auto-Discovery of Cell Count
**Location**: `main.c:2311-2319`

When processing received cell data:
```c
// If EEPROM is unprogrammed, auto-discover cell count
// Update expected count if we receive more cells than expected
if (!sg_bEEPROMValid)
{
    if (sg_sFrame.m.sg_u8CellCPUCount > sg_sFrame.m.sg_u8CellCountExpected)
    {
        CellCountExpectedSet(sg_sFrame.m.sg_u8CellCPUCount);
    }
}
```

## Behavior with Unprogrammed EEPROM

### Default Values
When `sg_bEEPROMValid = false`:

| Parameter | Default Value | Behavior |
|-----------|---------------|----------|
| Unique ID | 0xBA77BABE | Fixed default for testing |
| Cells Expected | 0 | Auto-increments as cells discovered |
| Max Charge Current | 0 | Uses maximum (existing behavior) |
| Max Discharge Current | 0 | Uses maximum (existing behavior) |
| Max Charge Voltage | 0 | Uses chemistry default |

**Note**: Current thresholds already handled unprogrammed EEPROM (0xFFFF or 0x0000) by using maximums.

### Auto-Discovery Process

1. **Initial State**:
   - Module starts with 0 cells expected
   - Waits for first cell string read

2. **First Read**:
   - Receives N cells from string
   - Updates `sg_u8CellCountExpected` to N
   - Recalculates voltage limits and byte counts

3. **Subsequent Reads**:
   - If receive M cells where M > N:
     - Updates `sg_u8CellCountExpected` to M
     - Recalculates limits again
   - Only increments, never decrements

4. **Result**:
   - Cell count automatically grows to match actual string
   - Stabilizes once all cells are discovered

### Testing Benefits

**Advantages**:
- No need to program EEPROM for initial testing
- Plug in module and immediately test with any cell count
- Cell count automatically adjusts as string is built
- Default UID (0xBA77BABE) makes unprogrammed modules identifiable

**Limitations**:
- Does not persist across power cycles (EEPROM not written)
- Each power-on rediscovers cell count from 0
- Should still program EEPROM for production use

## Production vs Testing Mode

### Testing Mode (EEPROM Unprogrammed)
```
EEPROM Unique ID: 0xFFFFFFFF
  ↓
ModuleControllerGetUniqueID() returns 0xBA77BABE
  ↓
sg_bEEPROMValid = false
  ↓
Cells Expected starts at 0
  ↓
Auto-discovery increments as cells found
  ↓
Useful for initial bring-up and debug
```

### Production Mode (EEPROM Programmed)
```
EEPROM Unique ID: <programmed value>
  ↓
ModuleControllerGetUniqueID() returns actual UID
  ↓
sg_bEEPROMValid = true
  ↓
Cells Expected loaded from EEPROM
  ↓
Fixed cell count, reports mismatches
  ↓
Normal operation
```

## Example Scenarios

### Scenario 1: Fresh Module with 10 Cells
```
Power On → UID=0xFFFFFFFF detected
         → sg_bEEPROMValid = false
         → Cells Expected = 0

First Read → Receive 10 cells
           → CellCountExpectedSet(10)
           → Module now expects 10 cells

Subsequent Reads → Always receive 10 cells
                 → Cell count remains at 10
```

### Scenario 2: Adding Cells During Testing
```
Initial State → Cells Expected = 5 (from previous reads)

Add 5 More Cells → Next read receives 10 cells
                 → Auto-discovery: CellCountExpectedSet(10)
                 → Module now expects 10 cells

Continue Testing → Cell count stable at 10
```

### Scenario 3: Missing Cells (Communication Issue)
```
Expected: 10 cells
Received: 8 cells (due to noise/error)

Auto-discovery → Does NOT decrement
              → Cells Expected remains at 10
              → Mismatch counter increments
              → Normal error handling applies
```

## Testing Recommendations

1. **Verify Unprogrammed Detection**:
   - Use fresh module with unprogrammed EEPROM
   - Check that unique ID reports as 0xBA77BABE
   - Verify sg_bEEPROMValid is false

2. **Test Auto-Discovery**:
   - Start with 1 cell, verify cell count = 1
   - Add cells one at a time, verify increment
   - Confirm cell count grows to actual string size

3. **Test Cell Count Stability**:
   - With N cells connected, verify count stays at N
   - Temporarily disconnect cells, verify count doesn't decrement
   - Reconnect cells, verify count remains stable

4. **Test Power Cycling**:
   - Power cycle module with unprogrammed EEPROM
   - Verify cell count resets to 0
   - Verify auto-discovery works again

5. **Compare to Programmed EEPROM**:
   - Program EEPROM with expected cell count
   - Verify sg_bEEPROMValid is true
   - Verify cell count loaded from EEPROM (not auto-discovered)

## Known Limitations

1. **No Persistence**: Cell count resets to 0 on power cycle (EEPROM not written)
2. **One-Way**: Auto-discovery only increments, never decrements
3. **No Validation**: No upper bound check on auto-discovered count
4. **Testing Only**: Should not be used in production (program EEPROM first)

## Future Enhancements (Optional)

- Add CAN message to report EEPROM unprogrammed status
- Add upper limit to auto-discovery (e.g., max 94 cells)
- Add optional EEPROM programming via CAN
- Add diagnostic counter for auto-discovery events
- Log cell count changes to SD card

## Migration Path

For modules transitioning from testing to production:

1. **Test Phase**: Use unprogrammed EEPROM, auto-discover cell count
2. **Validation**: Verify cell count stabilizes at correct value
3. **Programming**: Write EEPROM with:
   - Actual unique ID (not 0xBA77BABE)
   - Discovered cell count
   - Charge/discharge limits
   - Max charge voltage
4. **Production**: Module now operates with fixed, validated parameters

## Conclusion

This feature enables rapid prototyping and testing without requiring EEPROM programming. The module automatically adapts to the actual cell string configuration, making it ideal for:

- Initial hardware bring-up
- Debugging communication issues
- Testing with varying cell counts
- Demonstration and evaluation

For production use, EEPROM should still be programmed with validated parameters.
