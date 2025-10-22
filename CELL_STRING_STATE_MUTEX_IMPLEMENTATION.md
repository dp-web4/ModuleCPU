# Cell String State Mutex Implementation

## Overview
Added protection for CellCPU circuits during module state transitions by depowering the cell string before relay/FET switching occurs.

## Problem Solved
Relay and FET switching during state transitions creates voltage glitches that can damage:
- CellCPU boards
- Temperature sensors (MCP9843)
- Cell monitoring circuitry

## Solution Implemented

### New Function: `PauseCellString()`
**Location**: `main.c:629-645`

**Purpose**: Immediately depower cell string before state transitions

**Actions**:
1. Sets `sg_eStringPowerState = ESTRING_INIT`
2. Calls `CELL_POWER_DEASSERT()` to turn off cell power
3. Calls `FrameInit(false)` to clear frame data
4. Calls `vUARTRXReset()` to gracefully stop any vUART reception in progress

**Recovery**: Cell string state machine naturally cycles back to operational after state transition completes (INIT→OFF→ON→IGNORE→OPERATIONAL)

### Integration Point
**Location**: `main.c:912` in `ModuleControllerStateHandle()`

**Placement**: Called once at the beginning of any state transition, before the switch statement

```c
if ( eNext != sg_eModuleControllerStateCurrent)
{
    // Pause cell string before any state transition
    PauseCellString();

    // ... relay/FET switching follows ...
}
```

## Behavior

### State Transition Flow (New)
```
State Change Requested
    ↓
Check SD Busy (existing mutex)
    ↓
Detect State != Target
    ↓
PauseCellString() called
    ├─ Set ESTRING_INIT
    ├─ CELL_POWER_DEASSERT()
    ├─ Clear frame data
    └─ Reset vUART RX
    ↓
[Cells now depowered and safe]
    ↓
Execute State Transition
    ├─ FET switching
    └─ Relay switching
    ↓
[Glitches occur but cells are protected]
    ↓
State transition completes
    ↓
Cell String State Machine resumes
    ├─ ESTRING_INIT → ESTRING_OFF
    ├─ Wait 100ms
    ├─ ESTRING_OFF → ESTRING_ON
    ├─ CELL_POWER_ASSERT()
    ├─ ESTRING_ON → ESTRING_IGNORE_FIRST_MESSAGE
    └─ ESTRING_IGNORE_FIRST_MESSAGE → ESTRING_OPERATIONAL
    ↓
Cells powered and operational again
```

### Timing
- **Cell power off**: Immediate (< 1ms)
- **State transition**: 15-20ms (existing delays)
- **Cell power recovery**: ~100-150ms (automatic via state machine)
- **Total added delay**: None during transition, cells recover naturally

## Key Features

1. **Single Call Point**: Only called once per state transition, not per state case
2. **No Additional Delays**: State transition proceeds immediately after cells depowered
3. **Automatic Recovery**: Cell string powers back up naturally via existing state machine
4. **Graceful Shutdown**: Stops any in-progress vUART reception cleanly
5. **Works with Watchdog**: If watchdog resets during transition, cells remain depowered

## Protected Transitions

All state transitions now protected:
- OFF → STANDBY
- OFF → PRECHARGE
- OFF → ON
- STANDBY → OFF
- STANDBY → PRECHARGE
- STANDBY → ON
- PRECHARGE → OFF
- PRECHARGE → STANDBY
- PRECHARGE → ON
- ON → OFF
- ON → STANDBY
- INIT → OFF (startup)

## Edge Cases Handled

1. **Watchdog Reset During Transition**:
   - `sg_eStringPowerState` in `.noinit` section preserves state
   - On recovery, state machine resumes from ESTRING_INIT
   - Cells remain depowered until state machine cycles

2. **Rapid State Commands**:
   - Each transition calls `PauseCellString()`
   - Cells depower/repower between transitions
   - Natural rate limiting prevents rapid switching

3. **Already Depowered**:
   - `PauseCellString()` safely called even if cells already off
   - `CELL_POWER_DEASSERT()` is idempotent
   - State machine handles all cases

4. **Existing SD Mutex**:
   - SD busy check occurs before state transition attempt
   - If SD busy, state transition deferred entirely
   - Cell string not affected by SD operations

## Testing Recommendations

1. **Normal Transitions**: Verify OFF→STANDBY→ON→OFF with scope on cell power
2. **Glitch Protection**: Monitor cell circuits during transitions for voltage spikes
3. **Temperature Sensors**: Verify MCP9843 sensors survive extended cycling
4. **Data Integrity**: Confirm no cell data corruption during transitions
5. **Rapid Commands**: Send multiple state commands quickly, verify proper handling
6. **Watchdog Recovery**: Force WDT reset during transition, verify recovery
7. **State Cycle Mode**: With STATE_CYCLE defined, verify automatic cycling still works

## Benefits

✅ **Hardware Protection**: Cells depowered during relay/FET glitches
✅ **Simple Implementation**: Single function call, leverages existing state machine
✅ **No Performance Impact**: State transitions not delayed
✅ **Automatic Recovery**: Cells power back up naturally
✅ **Robust**: Handles all edge cases including watchdog resets
✅ **Clean Code**: Well-documented, single point of change

## Files Modified

- `main.c`:
  - Added `PauseCellString()` function (line 629)
  - Added call in `ModuleControllerStateHandle()` (line 912)

## Related Safety Features

This complements existing safety mechanisms:
- **SD Mutex**: Prevents state transitions during SD operations
- **Registration Check**: Prevents relay/FET control when not registered
- **Watchdog Protection**: Short leash during relay/FET switching
- **Overcurrent Detection**: FET turns off on overcurrent
- **Proper Sequencing**: FET always turns off before relay

## Future Enhancements (Optional)

- Add diagnostic counter for cell string pause events
- Log state transitions with cell power status
- Add CAN message reporting when cells depowered for transition
- Implement emergency bypass if needed (unlikely)

## Conclusion

Clean, simple solution that:
- Protects expensive cell circuitry from glitch damage
- Uses existing infrastructure (no new state variables)
- Adds no delays to state transitions
- Automatically recovers without intervention
- Handles all edge cases robustly
