# Proposed Fix for ModuleCPU Initialization Issues

## Problems Identified

### 1. Max Cells Reporting 170 (0xAA)
**Root Cause**: In `MODULE_CELL_COMM_STATUS` message, `sg_sFrame.sg_u8CellCPUCountMost` is being sent with value 170 (0xAA).

**Analysis**: 
- After full init, `memset(&sg_sFrame, 0, sizeof(sg_sFrame))` clears everything to 0
- Code then sets `sg_u8CellCPUCountFewest = 0xff` but doesn't reset `sg_u8CellCPUCountMost`
- However, memset should have set it to 0, not 0xAA
- The 0xAA pattern suggests uninitialized memory or corruption

**Likely issue**: The StringData array might contain uninitialized data that's being misinterpreted. When we disabled clearing StringData in partial init to preserve cell data for MODULE_DETAIL responses, we may have exposed an initialization issue.

### 2. Module Voltage Stuck at 29.25V
**Root Cause**: MODULE_STATUS_1 reports voltage as 1950 (0x079E) in 0.015V units = 29.25V

**Analysis**:
- The voltage calculation uses ADC reading: `u32Voltage = (1 << ADC_BITS) - 1 - sg_sFrame.ADCReadings[EADCTYPE_STRING].u16Reading`
- Then multiplies by `sg_i16VoltageStringPerADC` and adds `sg_i32VoltageStringMin`
- 29.25V seems like a suspiciously round number (1950 * 0.015)

## Proposed Solutions

### Fix 1: Properly Initialize sg_u8CellCPUCountMost
In `FrameInit()` after the memset in full init:
```c
if (bFullInit || (FRAME_VALID_SIG != sg_sFrame.validSig)) {
    memset(&sg_sFrame, 0, sizeof(sg_sFrame));
    sg_sFrame.frameBytes = sizeof(sg_sFrame);
    sg_sFrame.validSig = FRAME_VALID_SIG;
    sg_sFrame.moduleUniqueId = ModuleControllerGetUniqueID();
    sg_sFrame.sg_u8CellFirstI2CError = 0xff;
    sg_sFrame.sg_u8CellCPUCountFewest = 0xff;
    sg_sFrame.sg_u8CellCPUCountMost = 0;  // ADD THIS LINE - explicitly init to 0
    CellCountExpectedSet(EEPROMRead(EEPROM_EXPECTED_CELL_COUNT));
}
```

### Fix 2: Check for Invalid ADC Reading
The ADC might not be initialized properly. Add validation:
```c
// In MODULE_STATUS_1 voltage calculation
if (!sg_sFrame.ADCReadings[EADCTYPE_STRING].bValid) {
    // ADC not valid, use cell voltage total if available
    if (sg_sFrame.sg_u32CellVoltageTotal > 0) {
        u32Voltage = sg_sFrame.sg_u32CellVoltageTotal / 15;
    } else {
        u32Voltage = 0;  // No valid voltage available
    }
} else {
    // existing ADC calculation code
}
```

### Fix 3: Initialize StringData Properly on Full Init
Since StringData is now preserved across partial inits, we should ensure it's properly initialized on full init:
```c
if (bFullInit || (FRAME_VALID_SIG != sg_sFrame.validSig)) {
    memset(&sg_sFrame, 0, sizeof(sg_sFrame));
    // ... other initialization ...
    
    // Initialize StringData to known safe values
    for (int i = 0; i < MAX_CELLS; i++) {
        sg_sFrame.StringData[i].voltage = 0;
        sg_sFrame.StringData[i].temperature = TEMPERATURE_BASE;  // 0°C in raw format
    }
}
```

## Alternative Hypothesis

The 0xAA pattern (10101010 binary) is often used as:
1. **Stack guard pattern** - to detect stack overflows
2. **Uninitialized heap pattern** - in debug builds
3. **EEPROM default** - unprogrammed EEPROM often reads as 0xFF or 0xAA

Given that sg_u8CellCPUCountMost shows 170 (0xAA) instead of an expected value, this might indicate:
- Memory corruption overwriting the value
- Reading from wrong memory location
- Uninitialized EEPROM being read somewhere

## Recommended Action

1. **First**: Add explicit initialization of `sg_u8CellCPUCountMost = 0` after memset
2. **Second**: Verify ADC initialization and add validation 
3. **Third**: Add diagnostic CAN message to report these values during init
4. **Monitor**: Check if 0xAA appears elsewhere to identify pattern

## Testing Plan

1. Flash the fix
2. Power cycle the module completely
3. Check initial MODULE_CELL_COMM_STATUS message
4. Verify voltage reading in MODULE_STATUS_1
5. Ensure MODULE_DETAIL still works correctly with preserved cell data