# Modbatt CAN Protocol Definitions

This directory contains the single source of truth for all CAN protocol definitions used across the Modbatt battery management system.

## Files

### Protocol Definitions
- **CAN_ID_ALL.h** - CAN message IDs and protocol documentation
  - Module ID constants (0x00 = broadcast, 0x01-0x1F = modules, 0xFF = unregistered)
  - Complete extended frame addressing scheme
  - Registration state machine documentation
  - All CAN message IDs for all protocols

### Message Structure Definitions
- **can_frm_mod.h** - Module <-> Pack Controller message structures (0x500-0x52F)
- **can_frm_vcu.h** - VCU <-> Pack Controller message structures (0x400-0x44F)
- **can_frm_bms_diag.h** - BMS diagnostic message structures (0x220-0x228)

## Protocol Overview

### Extended Frame Addressing

ALL module protocol messages use 29-bit extended CAN frames with this format:

```
Extended ID = [11-bit Base ID << 18] | [Module ID in bits 7:0]
```

**Module ID Assignments:**
- `0x00` = Broadcast to all registered modules
- `0x01-0x1F` = Specific module IDs (1-31)
- `0xFF` = Unregistered module messages

### Message Categories

#### Module <-> Pack (0x500-0x52F)
Extended frames with module ID encoding:

**Module to Pack (moduleID = 0xFF when unregistered, 0x01-0x1F when registered):**
- 0x500 MODULE_ANNOUNCEMENT
- 0x501 MODULE_HARDWARE
- 0x502 MODULE_STATUS_1
- 0x503 MODULE_STATUS_2
- 0x504 MODULE_STATUS_3
- 0x505 MODULE_DETAIL
- 0x506 MODULE_TIME_REQUEST
- 0x507 MODULE_CELL_COMM_STATUS1
- 0x508 MODULE_CELL_COMM_STATUS2
- 0x509 MODULE_STATUS_4

**Pack to Unregistered (moduleID = 0xFF):**
- 0x510 MODULE_REGISTRATION
- 0x51D MODULE_ANNOUNCE_REQUEST

**Pack to Specific Module (moduleID = 0x01-0x1F):**
- 0x511 MODULE_HARDWARE_REQUEST
- 0x512 MODULE_STATUS_REQUEST
- 0x514 MODULE_STATE_CHANGE
- 0x515 MODULE_DETAIL_REQUEST
- 0x516 MODULE_SET_TIME
- 0x518 MODULE_DEREGISTER

**Pack to All Registered (moduleID = 0x00):**
- 0x517 MODULE_MAX_STATE
- 0x51E MODULE_ALL_DEREGISTER
- 0x51F MODULE_ALL_ISOLATE

**Frame Transfer (moduleID = specific or 0x00):**
- 0x520 FRAME_TRANSFER_REQUEST
- 0x521 FRAME_TRANSFER_START
- 0x522 FRAME_TRANSFER_DATA
- 0x523 FRAME_TRANSFER_END

#### VCU <-> Pack (0x400-0x44F)
Standard or extended frames (implementation dependent)

#### Diagnostics (0x220-0x228)
Standard or extended frames (implementation dependent)

## Module Registration Flow

1. **Unregistered Module** (moduleID = 0xFF):
   - Sends ANNOUNCEMENT on (0x500 << 18) | 0xFF
   - Listens on MOB 0 for moduleID = 0xFF
   - Listens on MOB 2 for moduleID = 0x00 (broadcasts)

2. **Pack Assigns ID**:
   - Sends REGISTRATION on (0x510 << 18) | 0xFF
   - Contains assigned moduleID (1-31) and unique ID verification

3. **Registered Module** (e.g., moduleID = 5):
   - Updates MOB 0 filter from 0xFF to 0x05
   - Listens on MOB 0 for moduleID = 0x05 (my messages)
   - Listens on MOB 2 for moduleID = 0x00 (broadcasts)
   - Transmits with moduleID = 0x05

4. **Deregistration**:
   - Receives DEREGISTER or ALL_DEREGISTER
   - Resets MOB 0 filter back to 0xFF
   - Transmits with moduleID = 0xFF again

## Hardware Filtering

### ATmega64M1 (ModuleCPU)
6 MOBs (Message Objects) with configurable filters:
- **MOB 0 (RX)**: Module-specific (0xFF unregistered, 0x01-0x1F registered)
- **MOB 1 (TX)**: Transmit with module ID
- **MOB 2 (RX)**: Broadcast (0x00) - ALWAYS ENABLED

### MCP2517FD (Pack Controller CAN Chip)
32 filters available:
- Accept unregistered: moduleID = 0xFF
- Accept registered: moduleID = 0x01-0x1F (mask to accept any)
- Transmit with appropriate moduleID

## Usage in Code

All projects should include from protocols/ directory:

```c
#include "../Pack-Controller-EEPROM/protocols/CAN_ID_ALL.h"
#include "../Pack-Controller-EEPROM/protocols/can_frm_mod.h"
#include "../Pack-Controller-EEPROM/protocols/can_frm_vcu.h"
#include "../Pack-Controller-EEPROM/protocols/can_frm_bms_diag.h"
```

## Projects Using These Definitions

- **ModuleCPU** (ATmega64M1) - Battery module controller
- **Pack-Controller-EEPROM** (STM32WB55) - Pack controller firmware
- **Pack Emulator** (C++ Builder) - Testing tool
- **modbatt-CAN** - Configuration and diagnostic tools

## Copyright

Copyright (C) 2023-2024 Modular Battery Technologies, Inc.
US Patents 11,380,942; 11,469,470; 11,575,270; others. All rights reserved
