# EEPROM Memory Map - ModuleCPU

## Overview
- **Total Size**: 2048 bytes (2KB) - ATmega64M1 EEPROM
- **Endurance**: 100,000 write cycles per cell
- **Organization**: Divided into functional regions with wear leveling for frequently updated data

## Memory Map

### Metadata Region (0x0000 - 0x003F) - 64 bytes
Reserved for module configuration and parameters that rarely change.

| Address | Size | Parameter | Description |
|---------|------|-----------|-------------|
| 0x0000 | 4 bytes | UNIQUE_ID | Module unique identifier |
| 0x0004 | 1 byte | EXPECTED_CELL_COUNT | Number of cells module expects |
| 0x0005 | 2 bytes | MAX_CHARGE_CURRENT | Maximum charging current limit |
| 0x0007 | 2 bytes | MAX_DISCHARGE_CURRENT | Maximum discharge current limit |
| 0x0009 | 2 bytes | SEQUENTIAL_COUNT_MISMATCH | Count mismatch tracking |
| 0x000B | 53 bytes | *Reserved* | Available for future metadata |

**Usage**: 11 bytes used, 53 bytes available

### Frame Counter Region (0x0040 - 0x023F) - 512 bytes
Wear-leveled storage for frequently updated frame counter.

| Address | Size | Description |
|---------|------|-------------|
| 0x0040 - 0x023F | 512 bytes | Circular buffer for frame counter wear leveling |

**Wear Leveling Strategy**:
- Frame counter stored as 4-byte value
- Rotates through 128 positions (512 / 4 = 128)
- Each position can handle 100,000 writes
- At 2Hz frame rate: 128 × 100,000 / 7200 = 1,777 hours (74 days) per position
- Total endurance: 128 positions × 74 days = **25.9 years** continuous operation

### Reserved Region (0x0240 - 0x07FF) - 1472 bytes
Available for future features.

**Potential Uses**:
- Extended statistics storage
- Calibration data
- Error logs
- Configuration profiles
- Cell balancing history

## Frame Counter Implementation Details

### Storage Format
```
Each counter entry (4 bytes):
[Count_MSB][Count_High][Count_Low][Count_LSB]

Counter rotation:
- Position 0: 0x0040 - 0x0043
- Position 1: 0x0044 - 0x0047
- ...
- Position 127: 0x023C - 0x023F
```

### Wear Leveling Algorithm
1. **Initialization**: Scan all 128 positions to find highest counter value
2. **Increment**: Add 1 to current counter value
3. **Write Strategy**:
   - Write to next position when lower byte rolls over (every 256 frames)
   - This reduces wear by 256× on each EEPROM cell
4. **Recovery**: On power-up, scan all positions, find highest valid value

### Write Optimization
- Only update changed bytes (typically just LSB)
- Batch updates when possible
- Use byte-wise comparison to minimize unnecessary writes

## Usage Guidelines

### Best Practices
1. **Metadata Updates**: Minimize writes to metadata region (configuration data)
2. **Frame Counter**: Automatic wear leveling handles frequent updates
3. **Future Expansion**: Keep additions aligned to 4-byte boundaries for efficiency
4. **Validation**: Use checksums or signatures for critical data

### Example Access Patterns
```c
// Read module unique ID
uint32_t id =
    ((uint32_t)EEPROMRead(EEPROM_UNIQUE_ID) << 24) |
    ((uint32_t)EEPROMRead(EEPROM_UNIQUE_ID + 1) << 16) |
    ((uint32_t)EEPROMRead(EEPROM_UNIQUE_ID + 2) << 8) |
    ((uint32_t)EEPROMRead(EEPROM_UNIQUE_ID + 3));

// Write expected cell count
EEPROMWrite(EEPROM_EXPECTED_CELL_COUNT, cellCount);
```

## Endurance Calculations

### Current Usage
- **Metadata**: ~1 write per configuration change (essentially unlimited)
- **Frame Counter**: 7200 writes/hour at 2Hz
  - Per-byte endurance: 100,000 / 7200 = 13.9 hours
  - With 512-byte rotation: 13.9 × 128 × 256/256 = 1,777 hours
  - **Total: 25.9 years continuous operation**

### Safety Margins
- Actual EEPROM endurance often exceeds 100,000 cycles
- Temperature derating applied (typical 85°C operation)
- Wear indicator can be implemented using reserved metadata bytes