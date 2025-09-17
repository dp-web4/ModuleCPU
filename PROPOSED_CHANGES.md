# Proposed CAN Fixes for ModuleCPU

## Problems Identified

### 1. Hardware Detail Flag Never Clears
**Issue**: `sg_bSendHardwareDetail` flag is set but never clears, causing continuous retries.

**Root Cause**: 
- In `main.c:2009-2032`, the flag only clears if `CANSendMessage()` returns true
- `CANSendMessage()` returns false if `sg_bBusy` is true
- If CAN bus is busy, the hardware detail keeps trying to send forever

### 2. Incorrect sg_bBusy Clearing in RX MOB Context
**Issue**: In `can.c:369`, sg_bBusy is incorrectly cleared in the RX MOB interrupt handler when TXOK is set.

**Location**: `can.c:365-369`
```c
// TX success?  Just clear it since this is the RX context
if( CANSTMOB & (1 << TXOK) )
{
    // Clear it
    CANSTMOB &= ~(1 << TXOK);
    sg_bBusy = false;  // <-- WRONG! This is RX context, not TX
}
```

This can cause race conditions where sg_bBusy is cleared prematurely while a transmission is still in progress.

### 3. Module Goes Quiet / Stuck in CAN ISR
**Symptoms**: 
- Module stops responding to CAN messages
- When paused, execution is in CAN interrupt code
- Hardware detail flag never clears

**Likely Cause**: The incorrect sg_bBusy handling creates a deadlock scenario where:
1. TX starts, sg_bBusy = true
2. RX MOB incorrectly clears sg_bBusy
3. New TX starts while previous TX still active
4. CAN peripheral gets confused with overlapping operations
5. System gets stuck in ISR trying to handle error conditions

## Proposed Solutions

### Fix 1: Remove Incorrect sg_bBusy Clear in RX Context
```c
// can.c:365-369
// TX success?  Just clear it since this is the RX context
if( CANSTMOB & (1 << TXOK) )
{
    // Clear it
    CANSTMOB &= ~(1 << TXOK);
    // sg_bBusy = false;  // REMOVE THIS LINE - TX completion handled in TX MOB
}
```

### Fix 2: Add Retry Counter for Hardware Detail
```c
// In main.c, add counter near sg_bSendHardwareDetail declaration:
static uint8_t sg_u8HardwareDetailRetries = 0;

// Modify main.c:2009-2032
if (sg_bSendHardwareDetail)
{
    bool bSuccess;
    
    // ... existing code to build response ...
    
    bSuccess = CANSendMessage( ECANMessageType_ModuleHardwareDetail, pu8Response, CAN_STATUS_RESPONSE_SIZE );
    
    if (bSuccess)
    {
        sg_bSendHardwareDetail = false;
        sg_u8HardwareDetailRetries = 0;  // Reset counter on success
    }
    else
    {
        // Increment retry counter
        sg_u8HardwareDetailRetries++;
        if (sg_u8HardwareDetailRetries > 100)  // Give up after 100 attempts
        {
            sg_bSendHardwareDetail = false;
            sg_u8HardwareDetailRetries = 0;
        }
    }
}
```

### Fix 3: Add Watchdog Reset for CAN Peripheral Recovery
As a safety measure, if the CAN peripheral gets stuck, the watchdog should reset the system. This is already in place but we should verify it's enabled properly.

## Testing Plan

1. **Verify Hardware Detail Sends Once**: Monitor CAN bus to confirm hardware detail message is sent exactly once when requested
2. **Stress Test**: Send rapid status requests to verify no lockups
3. **Monitor sg_bBusy**: Add debug output to track sg_bBusy state transitions
4. **Long Duration Test**: Run for extended period to verify no gradual degradation

## Risk Assessment

- **Low Risk**: Removing the incorrect sg_bBusy clear in RX context - this line is clearly wrong
- **Medium Risk**: Adding retry limit - need to ensure 100 retries is sufficient for worst-case bus congestion
- **Overall Impact**: These fixes should eliminate the module going quiet and stuck in CAN ISR issues

## Next Steps

1. Review and discuss these proposed changes
2. Implement fixes after approval
3. Test on hardware
4. Monitor for improvements in reliability

---

# Previous Content: VUART Edge-Triggered Timing Correction

[Previous VUART and Pack Controller content preserved below...]

## Current Implementation Analysis

The current VUART receive implementation:
1. **Start bit detection**: INT1 interrupt on falling edge (start bit)
2. **Initial timing**: Sets timer to `VUART_BIT_TICKS + VUART_BIT_START_OFSET` to sample middle of first data bit
3. **Subsequent bits**: Timer fires every `VUART_BIT_TICKS - VUART_BIT_TICK_OFFSET` to sample remaining bits
4. **Fixed timing**: No adjustment during byte reception - relies on initial synchronization

### Current Timing Constants
- `VUART_BIT_START_OFSET = 9`: Added to first bit timing
- `VUART_BIT_TICK_OFFSET = 7`: Subtracted from subsequent bit timings

## Problem Statement

Current implementation has no mechanism to correct for:
- Clock frequency differences between sender and receiver
- Timing drift during multi-byte transfers
- Jitter in the signal
- Temperature-induced clock variations

## Proposed Solution: Edge-Triggered Timing Correction

### 1. Core Concept
During data bit reception, detect edges (transitions) and use them to resynchronize the sampling timer to maintain center-of-bit sampling.

### 2. Implementation Approach

#### A. Add Edge Detection During Bit Reception
```c
// In TIMER0_COMPB ISR, after sampling the bit:
bool bData = IS_PIN_RX_ASSERTED();

// Detect edge (transition from previous bit)
if (bData != sg_bCell_mc_rxPriorState) {
    // Edge detected - calculate timing error
    uint8_t currentTimer = TCNT0;
    uint8_t expectedTimer = OCR0B - (VUART_BIT_TICKS/2);
    int8_t timingError = currentTimer - expectedTimer;
    
    // Apply correction if significant
    if (abs(timingError) > TIMING_TOLERANCE) {
        // Adjust next sample time to recenter
        OCR0B = TCNT0 + (VUART_BIT_TICKS/2) - (timingError/2);
    }
    
    // Update statistics
    if (timingError < sg_minTimingError) sg_minTimingError = timingError;
    if (timingError > sg_maxTimingError) sg_maxTimingError = timingError;
}
```

#### B. New Variables Required
```c
// Timing correction statistics
static int8_t sg_minTimingError;     // Minimum timing error seen
static int8_t sg_maxTimingError;     // Maximum timing error seen
static uint16_t sg_edgeCorrections;  // Count of corrections applied
static uint8_t sg_lastEdgeTimer;     // Timer value at last edge

// Configuration
#define TIMING_TOLERANCE 3    // Only correct if error > 3 timer ticks
#define MAX_CORRECTION 5      // Maximum correction per edge (prevent oscillation)
```

#### C. Statistics Reset
```c
// In vUARTInitReceive() or when starting new reception:
sg_minTimingError = 127;
sg_maxTimingError = -128;
sg_edgeCorrections = 0;
```

### 3. Alternative Approaches Considered

#### Option A: Full PLL-style tracking (Complex)
- Continuously adjust sampling rate based on average edge timing
- More complex but potentially more accurate
- Risk: May overcorrect for noise

#### Option B: Simple edge resync (Proposed above - Moderate)
- Adjust timing only when edges detected
- Balance between simplicity and effectiveness
- Lower risk of instability

#### Option C: Edge validation before correction (Conservative)
- Only correct if multiple consecutive edges show similar error
- Most stable but may miss short-term corrections
- Best for noisy environments

### 4. Benefits
1. **Improved reliability**: Better tolerance for clock differences
2. **Self-correcting**: Automatically compensates for drift
3. **Diagnostic data**: Error statistics help identify problematic modules
4. **Backwards compatible**: No protocol changes required

### 5. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Edge noise causes false corrections | Data corruption | Add edge validation/filtering |
| Overcorrection causes oscillation | Timing instability | Limit maximum correction per edge |
| Extra processing in ISR | Missed bits | Keep correction logic minimal |
| Statistics overflow | Memory corruption | Use appropriate data types |

### 6. Testing Plan
1. Test with known clock offset (±5% frequency difference)
2. Test with temperature cycling (clock drift)
3. Test with noise injection on RX line
4. Verify statistics collection accuracy
5. Performance testing (ISR execution time)

## Recommendation

Implement **Option B (Simple edge resync)** as it provides:
- Good improvement in reliability
- Moderate complexity
- Low risk of instability
- Easy to debug with statistics

---

# Previous Content: Persistent Module Records with Registration Status

## Better Solution: Persistent Module Records
Instead of removing modules from the array, keep them and track registration status. This handles intermittent modules gracefully.

## Implementation

### 1. Add registration status to module structure
In `/mnt/c/projects/ai-agents/Pack-Controller-EEPROM/Core/Inc/bms.h`:
```c
typedef struct {
  // ... existing fields ...
  bool        isRegistered;     // Module currently registered
  // ... rest of fields ...
}batteryModule;
```

### 2. Modify deregistration to just mark as unregistered
In PCU_Tasks timeout handling (~line 357):
```c
// Instead of removing from array:
// Mark module as deregistered
module[index].isRegistered = false;

// Don't shift array or change pack.moduleCount
// Continue to next module without adjusting index
```

### 3. Update registration logic
In MCU_RegisterModule (~line 1040):
```c
// Check if module already exists (registered or not)
moduleIndex = MAX_MODULES_PER_PACK; // Invalid index
for(index = 0; index < MAX_MODULES_PER_PACK; index++){
    if(module[index].uniqueId == announcement.moduleUniqueId){
        moduleIndex = index;
        break;
    }
}

if(moduleIndex < MAX_MODULES_PER_PACK){
    // Existing module - just mark as registered
    module[moduleIndex].isRegistered = true;
    module[moduleIndex].faultCode.commsError = 0;
    // ... reset timeouts etc
    
    if(debugMessages & DBG_MSG_ANNOUNCE){
        sprintf(tempBuffer,"MCU INFO - Module RE-REGISTERED: Index=%d, ID=%02x, UID=%08x",
                moduleIndex, module[moduleIndex].moduleId, announcement.moduleUniqueId);
        serialOut(tempBuffer);
    }
}
else {
    // New module - find first empty slot
    for(index = 0; index < MAX_MODULES_PER_PACK; index++){
        if(module[index].uniqueId == 0){  // Empty slot
            moduleIndex = index;
            break;
        }
    }
    
    if(moduleIndex < MAX_MODULES_PER_PACK){
        // Initialize new module
        module[moduleIndex].moduleId = moduleIndex + 1;  // ID = index + 1
        module[moduleIndex].uniqueId = announcement.moduleUniqueId;
        module[moduleIndex].isRegistered = true;
        // ... rest of initialization
        
        // Update pack.moduleCount to highest registered module
        pack.moduleCount = 0;
        for(int i = 0; i < MAX_MODULES_PER_PACK; i++){
            if(module[i].isRegistered && module[i].uniqueId != 0){
                pack.moduleCount++;
            }
        }
    }
}
```

### 4. Update all module iterations to check registration
Everywhere we iterate through modules:
```c
for(index = 0; index < MAX_MODULES_PER_PACK; index++){
    if(!module[index].isRegistered || module[index].uniqueId == 0) continue;
    // ... process module
}
```

### 5. Update module finding functions
```c
uint8_t MCU_ModuleIndexFromId(uint8_t moduleId){
    for(index = 0; index < MAX_MODULES_PER_PACK; index++){
        if(module[index].moduleId == moduleId && module[index].isRegistered)
            return index;
    }
    return MAX_MODULES_PER_PACK; // Not found
}
```

## Benefits
1. Module IDs never change once assigned
2. Handles intermittent connections gracefully
3. No array shifting complications
4. Module history is preserved
5. Can track metrics across connect/disconnect cycles
6. Simpler logic overall

## Module Counting
```c
// Add to pack structure:
uint8_t totalModules;    // Count of all modules ever seen (non-empty slots)
uint8_t activeModules;   // Count of currently registered modules

// Update counts whenever registration changes:
void MCU_UpdateModuleCounts(void){
    pack.totalModules = 0;
    pack.activeModules = 0;
    
    for(int i = 0; i < MAX_MODULES_PER_PACK; i++){
        if(module[i].uniqueId != 0){
            pack.totalModules++;
            if(module[i].isRegistered){
                pack.activeModules++;
            }
        }
    }
}
```

When reporting to host controller:
- Total modules = all modules that have ever connected (pack.totalModules)
- Active modules = currently registered modules (pack.activeModules)
- This helps diagnose intermittent connection issues

## Notes
- pack.moduleCount can be deprecated or repurposed
- Module slots are reused based on unique ID
- Empty slots have uniqueId = 0
- Host sees both "modules that exist" and "modules currently online"