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