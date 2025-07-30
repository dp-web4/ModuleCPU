# Claude Context for ModuleCPU

## Project Context System

**IMPORTANT**: A comprehensive context system exists at `/mnt/c/projects/ai-agents/misc/context-system/`

Quick access:
```bash
# Get overview of battery hierarchy projects
cd /mnt/c/projects/ai-agents/misc/context-system
python3 query_context.py project modulecpu

# See complete battery hierarchy
cat /mnt/c/projects/ai-agents/misc/context-system/projects/battery-hierarchy.md

# Check relationships
python3 query_context.py search "hierarchical autonomy"
```

## This Project's Role

ModuleCPU is the middle layer of the three-tier battery management hierarchy:
- CellCPU → **ModuleCPU** (this) → Pack-Controller

Module-level coordination (ATmega64M1) managing:
- Up to 94 CellCPUs via virtual UART
- CAN bus communication to Pack Controller
- SD card logging with FatFS
- Dual-phase operation for data integrity
- RTC support for timestamping

## Key Relationships
- **Integrates With**: CellCPU (downward), Pack-Controller (upward)
- **Implements**: Hierarchical coordination
- **Uses**: Shared.h definitions from /Shared directory
- **Protected By**: US Patent 11,469,470

## Design Philosophy
ModuleCPU embodies "middle management" - aggregating cell data while maintaining their autonomy, and reporting to the pack level without exposing cell-level details. This mirrors Synchronism's principle of scale-specific Markov blankets.

## Development Process - CRITICAL PROTOCOL
**IMPORTANT**: This protocol has already saved us from significant chaos. Follow it strictly:

When proposing changes:
1. **PROPOSE**: Create a PROPOSED_CHANGES.md file with analysis and options
2. **DISCUSS**: Present to the user and discuss implications  
3. **IMPLEMENT**: Only make changes after agreement

The user emphasized (July 30, 2025): "ok cool but remember our protocol - propose the changes, discuss, THEN make them"

This is especially critical when debugging timing-sensitive embedded systems where changes can have non-obvious side effects.

## Important Notes
- Variables in `.noinit` section are there to survive watchdog resets during relay switching
- These variables must be properly initialized on power-on but preserved across WDT resets
- Always consider EMI-induced resets when making changes

## Recent Safety Improvements (July 29, 2025)

### Startup Safety Fixes
1. **Fixed uninitialized `sg_eModuleControllerStateMax`**: This variable in `.noinit` section was not initialized on power-on, potentially allowing unwanted state transitions. Now initialized to `EMODSTATE_OFF`.

2. **Fixed uninitialized `sg_bModuleRegistered`**: This flag in `.noinit` section could retain `true` from previous runs. Now explicitly initialized to `false` on power-on.

3. **Added registration checks for relay/FET control**: All `RELAY_EN_ASSERT()` and `FET_EN_ASSERT()` calls now check `sg_bModuleRegistered` (bypassed when `STATE_CYCLE` is defined for testing).

These fixes prevent the module from briefly turning on relays at startup - a critical safety improvement.

### Pack Controller Integration
- Successfully integrated with Pack-Controller-EEPROM project
- Fixed various CAN message definitions and debug system
- Module announcement and registration working properly

## Recent Bug Fixes (July 30, 2025)

### Fixed Status Request Race Condition
**Problem**: Pack controller was sending multiple status requests before module could finish sending all three status messages, causing:
- Multiple Status #1 messages
- Module timeout and deregistration
- Registration/deregistration loop

**Root Cause**: Pack controller cleared `statusPending` after receiving ANY status message, but module sends Status #1, #2, #3 in sequence.

**Solution - Both Sides**:
1. **Pack Controller**: Track which messages received with bitmask, only clear `statusPending` when all 3 received
2. **Module**: Added `sg_bIgnoreStatusRequests` flag to prevent restarting sequence while sending

### Fixed Individual Deregister Handling
- Pack controller now properly sends individual deregister messages
- Module now properly receives and processes them
- Fixed message format to match other module-specific messages