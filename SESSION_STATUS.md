# Session Status - ModuleCPU VUART & Cell Detail Issues

## Date: 2025-01-12

## Primary Issues Addressed

### 1. VUART Edge-Triggered Timing Correction (ModuleCPU)
**Problem**: VUART receive reliability issues due to clock drift
**Solution Implemented**: 
- Added edge-triggered timing correction during byte reception
- Detects bit transitions and resyncs timer to mid-bit position
- Tracks min/max timing error statistics
- Changed diagnostic sync signal from PC7 to PD5 (PC7 drives flipflop clock on REV E+)

**Key Code Changes**:
- `vUART.c`: Added edge correction in INT1 ISR
- Switches to any-edge detection after start bit
- Resyncs timer on every edge: `OCR0B = TCNT0 + (VUART_BIT_TICKS/2) - VUART_BIT_TICK_OFFSET`
- Returns to falling-edge detection after byte complete

**Status**: Edge correction implemented but made VUART less reliable - needs investigation

### 2. Cell Detail Request/Response Issues (Pack Controller ↔ ModuleCPU)

**Problem**: Cell details being sent/received out of order
**Issues Found & Fixed**:

#### ModuleCPU Side:
1. **Bug**: Target was set to same as current cell, should be current+1
   - Fixed: `sg_u8CellStatusTarget = sg_u8CellStatus + 1`
2. **Bug**: Module was sending unsolicited status messages during cell detail sequence
   - Fixed: Removed automatic status send in `CellStringProcess()`
3. **Bug**: DLC mismatch - module expects 3 bytes but Pack was sending 2 for follow-up requests
   - Fixed: Pack Controller now sends 3 bytes consistently

#### Pack Controller Side:
1. **Bug**: Not setting moduleId when requesting next cell
   - Fixed: Added `detailRequest.moduleId = rxObj.bF.id.EID`
2. **Bug**: Status requests interrupting cell detail sequences
   - Added: General `waiting` flag to prevent overlapping requests
3. **Bug**: DLC was 2 bytes for follow-up requests, should be 3
   - Fixed: Changed to `CAN_DLC_3` for all cell detail requests

**Current Status**: Still seeing erratic cell sequence despite fixes

## Remaining Issues

### Cell Sequence Still Erratic
Despite all fixes, seeing patterns like:
- 11, 12, 1, 2, 4, 6, 7, 9, 10, 12 (skipping cells)
- 1, 2, 4, 5, 7, 9, 10, 12 (different skips)
- 0, 2, 4, 5, 6, 8, 10, 11 (yet another pattern)

**Theory**: Issue appears to be on Pack Controller side since:
- Module only sends when requested (verified)
- No duplicate response debug messages
- Request-wait-receive handshake should prevent overlap

## Code State

### ModuleCPU:
- VUART edge correction implemented (needs tuning)
- Diagnostic signal moved to PD5
- Cell detail response fixed to send one cell per request
- Unsolicited status sends removed

### Pack Controller:
- Added `waiting` flag to batteryModule structure
- Fixed cell detail request sequencing
- Added debug messages for tracking requests/responses
- Fixed DLC to 3 bytes for all cell detail requests

## Next Steps
1. Investigate why Pack Controller is requesting cells out of order
2. Check if CAN message queue is causing reordering
3. Verify timing between requests and responses
4. Consider if there's a race condition in the waiting flag logic

## Debug Info Added
- Pack Controller: Shows when sending status requests with waiting flag state
- Pack Controller: Shows cell detail requests and responses with raw bytes
- ModuleCPU: Cell detail requests only accepted when not already sending

## Build Commands
- ModuleCPU: Build in Microchip Studio on Windows
- Pack Controller: Build in STM32CubeIDE on Windows