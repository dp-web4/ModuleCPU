# Claude Context for ModuleCPU

## Private Context Access
- **Private Context**: `/mnt/c/projects/ai-agents/private-context/`
- **Machine Info**: `/mnt/c/projects/ai-agents/private-context/machines/windows11-laptop.md`
- **GitHub PAT**: Available in `/mnt/c/projects/ai-agents/.env`
- **Note**: Cannot cd to parent directory but can access these paths directly

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
- Dual-phase operation for data integrity
- RTC support for timestamping

## Key Relationships
- **Integrates With**: CellCPU (downward), Pack-Controller (upward)
- **Implements**: Hierarchical coordination
- **Uses**: Shared.h definitions from /Shared directory
- **Protected By**: US Patent 11,469,470

## Design Philosophy
ModuleCPU embodies "middle management" - aggregating cell data while maintaining their autonomy, and reporting to the pack level without exposing cell-level details. This mirrors Synchronism's principle of scale-specific Markov blankets.

## Build Environment
**IMPORTANT**: This project runs in WSL but uses Windows-based build tools:
- **ModuleCPU**: Uses Atmel Studio (Windows IDE) - **cannot build from WSL**
- **Pack-Controller-EEPROM emulator**: Uses Borland C++ Builder 6 (Windows) - **cannot build from WSL**
- **DO NOT** attempt to run make, gcc, or other build commands from WSL
- User must compile manually in Windows environment
- Focus on code correctness, not attempting builds

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
- **NO DEBUG OUTPUT**: The ModuleCPU does not have debug output capability (no DebugOut functions)
  - Do not add any DebugOut, DebugOutUint8, etc. calls - they don't exist
  - Debug must be done through CAN messages or LED indicators only

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

## Recent VUART Rising Edge Fix (September 18, 2025)

### Critical Signal Inversion Discovery
**Problem**: ModuleCPU was using falling edge detection for VUART start bits, but level shifters between cells (needed because cells are stacked at different voltages) INVERT the signal.

**Solution**: Changed ModuleCPU to use RISING edge detection:
- Added `VUART_RX_RISING_EDGE()` macro in vUART.h
- Changed `VUART_RX_ENABLE()` to use rising edge (ISC11=1, ISC10=1)
- Updated stop bit handling to return to rising edge detection

**Signal Flow**:
1. CellCPU transmits: HIGH (idle) → LOW (start bit)
2. Level shifter inverts: LOW (idle) → HIGH (start bit)
3. ModuleCPU now correctly detects: Rising edge

This fixed temperature data corruption and VUART communication reliability issues!

## Recent CAN TX Timeout Fix (September 21, 2025)

### Added CAN TX Timeout Mechanism
**Problem**: `sg_bBusy` could get stuck if TX interrupt didn't fire or TXOK wasn't set, blocking all CAN transmissions permanently.

**Solution**:
1. Changed `sg_bBusy` from boolean to `sg_u8Busy` timeout counter
2. Set to `CAN_TX_TIMEOUT_TICKS` (2 = 200ms) when transmission starts
3. Added `CANCheckTxStatus()` function called every 100ms tick that:
   - Polls TXOK flag directly as backup to interrupt
   - Checks for TX error flags (BERR, SERR, CERR, FERR, AERR)
   - Decrements counter and force-clears after timeout
   - Properly disables TX MOB interrupt on timeout to prevent spurious interrupts
4. Fixed timeout extension issue on retransmits - timeout no longer resets on AERG retries
5. Added diagnostic counters:
   - `sg_u16TxTimeouts` - Count of TX timeouts
   - `sg_u16TxErrors` - Count of TX errors recovered
   - `sg_u16TxOkPolled` - Count of TXOK found by polling

This ensures CAN communication can recover even if interrupts fail, preventing permanent lockups.

### Critical Bus-Off Recovery Fix
**Problem**: CAN was permanently failing even when `sg_u8Busy = 0` due to bus-off state not being recovered.

**Root Cause**: When CAN controller enters bus-off state (after too many errors), it automatically disables itself and requires manual re-enabling. The old code just cleared the interrupt flag but never re-enabled the controller.

**Solution**:
1. **Bus-Off Recovery**: Now properly re-enables CAN controller with `CANGCON = (1 << ENASTB)` when bus-off detected
2. **Health Check Function**: Added `CANCheckHealth()` called every 100ms that:
   - Checks if controller is disabled and re-enables it
   - Verifies RX MOB is still enabled and re-enables if needed
   - Monitors error passive state (TEC/REC > 127)
   - Ensures CAN stays operational even after severe errors
3. **Additional Diagnostics**:
   - `sg_u16BusOffEvents` - Count of bus-off recoveries
   - `sg_u16ErrorPassive` - Count of error passive detections

**Key Finding**: The CAN controller can become disabled due to bus-off events, and without explicit re-enabling, all CAN communication stops permanently even though `sg_u8Busy` correctly shows 0.

### Bus-Off Recovery Improvements (Latest)
**Problem**: Module was rapidly cycling in/out of bus-off state, causing intermittent communication.

**Root Cause**: After bus-off recovery, the module immediately tried to transmit again, hitting bus errors and going back to bus-off. This indicates a physical bus problem (termination, baudrate, grounding).

**Solution**:
1. **Recovery Delay**: After bus-off, wait 1 second (10 ticks) before allowing transmissions
2. **Error Counter Monitoring**: Track TEC/REC values to detect rapid error accumulation
3. **Diagnostic Functions**: Added `CANGetTEC()` and `CANGetREC()` to monitor error counters

**Physical Layer Checklist**:
- Verify 120Ω termination resistors at both ends of CAN bus
- Confirm 500kbps baudrate on all nodes (CANBT1=0x02, CANBT2=0x04, CANBT3=0x12 for 8MHz)
- Check ground connections between nodes
- Verify CAN_H and CAN_L differential pair routing

### TX-Only Error Detection (September 21, 2025)
**Problem**: One module showing CANTEC=0x1C (28 decimal), CANREC=0, indicating transmit-only errors without receiving bus activity. This points to a hardware issue with the CAN transceiver.

**Solution - Adaptive Error Handling**:
1. **TX-Only Error Detection**: Added detection for when CANTEC increases but CANREC stays at 0
   - `sg_u8TxOnlyErrorCount` tracks consecutive TX-only error occurrences
   - Resets when any RX activity detected (CANREC > 0)
2. **Adaptive Backoff**: Implemented exponential backoff for persistent TX errors
   - After 3 consecutive TX-only errors, applies backoff delays
   - Exponential increase: 200ms, 400ms, 800ms, 1.6s (capped)
   - Also applies 500ms backoff for rapid error increases (>10 errors/tick)
3. **New Diagnostic Functions**:
   - `CANGetTxOnlyErrorCount()` - Returns count of TX-only errors
   - `CANGetTxBackoffDelay()` - Returns current backoff delay in ticks

**Hardware Issue Indicators**:
- CANTEC increasing with CANREC=0 suggests TX driver/transceiver failure
- Module-specific issue (other modules work fine) confirms hardware problem
- Software workaround mitigates but doesn't fully solve hardware failures

## Recent CAN Reliability Fixes (September 17, 2025)

### Critical CAN Interrupt Issues Fixed
1. **Hardware Detail Flag**: Now clears unconditionally after send attempt (was getting stuck if CAN busy)
2. **sg_bBusy Race Condition**: Removed incorrect clearing in RX MOB context (lines 369-375 commented out)
3. **TX Status in RX Context**: Removed entire TX status handling from RX MOB (was in wrong context)
4. **CAN MOB Flow Documentation**: Added comprehensive documentation explaining TX/RX MOB re-enabling mechanism
   - TX MOB is one-shot, not re-enabled in interrupt (gets re-enabled by CANMOBSet on next TX)
   - RX MOB is continuous, re-enabled in interrupt handler

### Key Discoveries
- TX MOB doesn't need re-enabling in interrupt because it's re-enabled by CANMOBSet when next message sent
- This "worked" despite looking wrong - now properly documented
- Module was getting stuck when hardware detail flag never cleared

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

## Protocol Dependency Resolution (October 18, 2025)

### CAN Protocol Synchronization
**Problem**: ModuleCPU had hard file system dependency on Pack-Controller-EEPROM for CAN protocol definitions via:
```c
#include "../Pack-Controller-EEPROM/protocols/CAN_ID_ALL.h"
```

This prevented ModuleCPU from being built or released independently.

**Solution**: Protocol Copy with Sync Script
1. **Local Copy**: Created `protocols/` directory with copies of protocol headers
2. **Sync Script**: `scripts/sync_protocols.sh` automates protocol synchronization
3. **Updated Include**: Changed `can_ids.h` to use local `protocols/CAN_ID_ALL.h`
4. **Documentation**: `protocols/SYNC_NOTICE.md` documents sync procedure

**Protocol Files** (copied from Pack-Controller-EEPROM):
- `CAN_ID_ALL.h` - All CAN message IDs (module, VCU, diagnostics)
- `can_frm_mod.h` - Module <-> Pack message structures
- `README.md` - Protocol documentation

**Sync Procedure** (for future protocol updates):
```bash
cd ModuleCPU
./scripts/sync_protocols.sh
# Review changes, test build, commit
```

**Authoritative Source**: `Pack-Controller-EEPROM/protocols/` is the single source of truth.
All protocol changes must be made there first, then synced to ModuleCPU.

This change enables ModuleCPU to be:
- Built standalone without Pack-Controller-EEPROM repo present
- Released as independent open-source repository
- Kept in sync with protocol updates via simple script