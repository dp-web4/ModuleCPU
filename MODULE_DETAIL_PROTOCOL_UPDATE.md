# MODULE_DETAIL Protocol Update - September 24, 2025

## Protocol Change Summary

The MODULE_DETAIL (0x505) message format has been updated to improve reliability and accuracy of cell reporting.

### What Changed

**Byte 1** of the MODULE_DETAIL response now reports:
- **OLD**: Expected cell count (`sg_u8CellCountExpected`)
- **NEW**: Actual cells that reported in last complete frame (`sg_u8LastCompleteCellCount`)

### Why This Change Was Made

1. **Accurate Cell Status**: Pack Controller needs to know how many cells actually reported, not how many were expected
2. **Frame Stability**: Using count from last complete frame provides stable data during active frame collection
3. **Proper Cell Mapping**: Enables correct reverse-order mapping for VUART chain architecture

### MODULE_DETAIL Response Format

| Byte | Content | Description |
|------|---------|-------------|
| 0 | Cell ID | Requested cell number (0-based) |
| 1 | **Cells Received** | Number of cells that reported in last complete frame |
| 2-3 | Temperature | Cell temperature in 0.01°C (int16) |
| 4-5 | Voltage | Cell voltage in mV (uint16) |
| 6 | SOC | State of charge (%) |
| 7 | SOH | State of health (%) |

### VUART Chain Cell Ordering

Due to the VUART daisy chain architecture, cells report in **reverse physical order**:

```
Physical Layout:  [Cell 0] -> [Cell 1] -> ... -> [Cell N-1]
Reporting Order:  Cell N-1 reports first, Cell 0 reports last
Storage Order:    StringData[0] = Cell N-1, StringData[N-1] = Cell 0
```

### Cell Index Mapping

When Pack Controller requests cell X:
```c
actualIndex = (cellsReceived - 1) - requestedCellId
```

Example with 13 cells reporting:
- Request Cell 0: Maps to StringData[12]
- Request Cell 12: Maps to StringData[0]

### Handling Non-Reporting Cells

If a requested cell didn't report (requestedCellId >= cellsReceived):
- Temperature = 0 (raw value)
- Voltage = 0 mV
- SOC = 0%
- SOH = 0%

Pack Controller can distinguish:
- Never received: voltage == 0 AND no timestamp
- Received zero: voltage == 0 BUT has timestamp

### Pack Emulator Updates Required

1. Use byte 1 as actual cell count, not expected count
2. Handle cell index mapping if displaying in physical order
3. Update "out of range" checks to use reported count

### Backwards Compatibility

This change is **NOT** backwards compatible. Both ModuleCPU and Pack Controller/Emulator must be updated together.

## Implementation Details

### ModuleCPU Side
- `sg_u8LastCompleteCellCount` saved at end of `CellStringProcess()`
- Provides stable count even without SD card
- Cell data mapped using reverse-order formula

### Pack Controller Side
- Parse byte 1 as cells actually reporting
- Use for range checking and data validation
- Apply reverse mapping if needed for display