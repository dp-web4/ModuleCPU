# Proposed: Cell String Power Down During State Transitions

## Problem
During module state transitions (OFF/STANDBY/PRECHARGE/ON), relay and FET switching can cause voltage glitches that may damage cell circuit components (CellCPU boards, temperature sensors, etc.). Currently, the cell string remains powered during these transitions.

## Proposed Solution
Add a mutex mechanism to depower the cell string before state transitions and restore power after the transition completes. This is similar to the existing SD card mutex that prevents state transitions during SD operations.

## Implementation Approach

### Option 1: Cell String State Check (Recommended)
**Approach**: Check if cell string is in OPERATIONAL state before allowing state transitions. If not, defer the transition.

**Advantages**:
- Clean separation of concerns
- Uses existing cell string power state machine
- Minimal code changes
- Natural timeout protection (cell string takes ~100ms to power up)

**Implementation**:
1. Add check in `ModuleControllerStateHandle()` after SD busy check
2. If `sg_eStringPowerState != ESTRING_OPERATIONAL`, return (defer transition)
3. Before state transition begins, force `sg_eStringPowerState = ESTRING_OFF`
4. After state transition completes, cell string state machine will naturally cycle through OFF→ON→IGNORE→OPERATIONAL

**Code Location**: `main.c:867-1114` (ModuleControllerStateHandle function)

### Option 2: Explicit Power Down/Up Calls
**Approach**: Explicitly call cell string power down before transition, power up after.

**Advantages**:
- More explicit control
- Can customize timing per state

**Disadvantages**:
- More complex
- Duplicates existing cell string state machine logic
- Requires careful timing management

## Detailed Implementation (Option 1)

```c
static void ModuleControllerStateHandle( void )
{
    EModuleControllerState eNext = sg_eModuleControllerStateTarget;

#ifndef STATE_CYCLE
    if ((eNext > sg_eModuleControllerStateMax) || (sg_eModuleControllerStateCurrent > sg_eModuleControllerStateMax) )
    {
        eNext = sg_eModuleControllerStateMax;
    }
#endif

    // Don't transition states if SD card is currently busy (read or write in progress)
    // Prevents relay switching EMI from corrupting active SPI transactions
    if (sg_bSDBusy)
    {
        // SD card operation in progress - defer state transition
        return;
    }

    // NEW: Don't transition states if cell string is not operational
    // This ensures cells are powered down before relay/FET switching to prevent glitch damage
    if (sg_eStringPowerState != ESTRING_OPERATIONAL)
    {
        // Cell string not yet operational - defer state transition
        // This includes cases where we just forced it to ESTRING_OFF below
        return;
    }

    // see if we need to transition
    if ( eNext != sg_eModuleControllerStateCurrent)
    {
        // NEW: Power down cell string before state transition
        // This will cause the state machine to cycle through OFF→ON→IGNORE→OPERATIONAL
        // taking ~100ms total (CELL_POWER_OFF_TO_ON_MS + 2 frame periods)
        sg_eStringPowerState = ESTRING_OFF;
        // On next call (after cell string becomes operational again), we'll proceed with transition
        return;

        // [Rest of state transition code follows...]
```

## Flow Diagram

### Current Behavior
```
State Command Received
    ↓
Check SD Busy → [if busy, defer]
    ↓
Execute State Transition (relays/FETs switch)
    ↓ [glitches may occur]
Cell Circuits Exposed to Glitches
```

### Proposed Behavior
```
State Command Received
    ↓
Check SD Busy → [if busy, defer]
    ↓
Check Cell String State → [if not OPERATIONAL, defer]
    ↓
Force Cell String to ESTRING_OFF
    ↓ [defer]
Wait ~100ms for Cell String Power Down
    ↓
Cell String OPERATIONAL again (but powered)
    ↓
Execute State Transition (relays/FETs switch)
    ↓ [cells depowered, protected from glitches]
Transition Complete
```

## Timing Analysis

**Total delay added**: ~100-150ms per state transition
- CELL_POWER_OFF_TO_ON_MS = 100ms (power settle time)
- 1-2 frame periods (~20-40ms) for state machine cycles
- Plus existing relay/FET switching delays (~15-20ms)

**Impact**:
- Minimal impact on state transition speed
- Acceptable for module operation (state changes are infrequent)
- Provides significant protection against glitch damage

## Edge Cases to Consider

1. **Multiple rapid state commands**:
   - Cell string will cycle OFF→ON between each transition
   - Natural rate limiting due to ~100ms cycle time
   - Good: prevents rapid switching that could damage relays

2. **Watchdog timeout during cell string power cycle**:
   - Watchdog leash is WDT_LEASH_LONG (2 seconds) during normal operation
   - Cell string cycle is ~100-150ms
   - No risk of timeout

3. **State transition from INIT**:
   - Cell string starts in ESTRING_INIT
   - First transition will wait for OPERATIONAL
   - Normal startup behavior

4. **Emergency shutdown**:
   - If immediate shutdown needed, can still force relays/FETs off
   - Cell string state is preserved in .noinit section across WDT reset

## Testing Recommendations

1. **Normal state transitions**: Verify OFF→STANDBY→ON→OFF works with cell string cycling
2. **Rapid commands**: Send multiple state commands quickly, verify proper queueing
3. **State cycle mode**: With STATE_CYCLE defined, verify automatic cycling still works
4. **CAN message timing**: Verify state status messages reflect correct timing
5. **Cell data integrity**: Verify no cell data corruption during transitions
6. **Temperature sensor protection**: Monitor for sensor damage during extended testing

## Alternative: Cell String Off Flag (Option 3)

Instead of forcing `ESTRING_OFF`, could add a separate flag:
- `sg_bCellStringOffForStateTransition`
- Set flag before state transition
- Cell string state machine checks flag and powers down
- Clear flag after transition

**Disadvantage**: Adds complexity, duplicates logic
**Advantage**: More explicit intent

## Recommendation

Implement **Option 1** (Cell String State Check) because:
- Uses existing state machine infrastructure
- Minimal code changes
- Clean separation of concerns
- Natural protection against rapid state changes
- Easy to test and verify

## Questions for User

1. Is ~100-150ms delay acceptable for state transitions?
2. Should we add diagnostic counters for cell string power cycles during state transitions?
3. Do we want to log/report when state transitions are deferred due to cell string state?
4. Should emergency shutdown bypass cell string protection?
