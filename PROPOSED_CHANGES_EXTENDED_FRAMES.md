# Proposed Changes: Extended Frame Addressing

## Current Implementation

**MOB Configuration:**
- MOB 0 (RX): Filters 0x500-0x5FF (mask 0x700), receives ALL module protocol messages
- MOB 1 (TX): Transmit with module ID in bits 0-7 of extended ID
- Extended frames enabled (IDE=1)

**Message Routing:**
- All modules hear all messages
- Module-specific messages have module ID in first data byte
- Module checks data byte to see if message is for them

**Problems:**
- Inefficient: All modules process all messages
- Module protocol messages visible to all modules (including responses)
- Wastes CPU time on irrelevant messages

## Proposed Implementation

**MOB Configuration (per protocol documentation):**

### UNREGISTERED STATE:
- MOB 0 (RX): Filter extended ID bits 0-7 = 0xFF (unregistered-specific messages)
- MOB 1 (TX): Transmit with extended ID bits 0-7 = 0xFF
- MOB 2 (RX): Filter extended ID bits 0-7 = 0x00 (broadcast messages) - **ALWAYS ENABLED**

### REGISTERED STATE (e.g., assigned ID = 5):
- MOB 0 (RX): Filter extended ID bits 0-7 = 0x05 (my specific messages)
- MOB 1 (TX): Transmit with extended ID bits 0-7 = 0x05
- MOB 2 (RX): Filter extended ID bits 0-7 = 0x00 (broadcast messages) - **UNCHANGED**

### ON DEREGISTRATION:
- MOB 0 (RX): Reset filter to extended ID bits 0-7 = 0xFF
- MOB 1 (TX): Transmit with extended ID bits 0-7 = 0xFF
- MOB 2 (RX): Still enabled at 0x00 (unchanged)

**Message Addressing:**
- 0x00 = Broadcast to all registered modules
- 0x01-0x1F = Specific module (1-31)
- 0xFF = Unregistered modules only

**Benefits:**
- Hardware filtering isolates traffic
- Modules don't hear each other's responses
- Reduced interrupt load
- Cleaner protocol

## Code Changes Required

### 1. Add MOB 2 for Broadcast Reception

**New MOB definition:**
```c
static const SMOBDef sg_sMOBBroadcastReceive =
{
    CAN_RXONLY,
    false,
    0x500,          // Base ID (0x5xx range)
    0x700,          // Mask for base ID
    false,
    false,
};
```

**Configuration in CANInit():**
```c
// MOB 2 always configured for broadcast (ID = 0x00)
CANMOBSet(2, &sg_sMOBBroadcastReceive, NULL, 0);
```

### 2. Update MOB Filter Configuration

**Modify CANMOBSet() to accept module ID filter parameter:**

Currently CANMOBSet encodes the module's own ID in transmitted frames. Need to also accept a target module ID for RX filtering.

**Option A: Add parameter to SMOBDef**
```c
typedef struct
{
    uint8_t u8Mode;
    bool bReplyValid;
    uint16_t u16ID;           // Base ID (0x510, 0x517, etc.)
    uint16_t u16IDMask;       // Base ID mask
    uint8_t u8ModuleIDFilter; // Module ID to filter on (0x00, 0xFF, or assigned ID)
    uint8_t u8ModuleIDMask;   // Module ID mask (0xFF for exact match)
    bool bRTRTag;
    bool bRTRMask;
} SMOBDef;
```

**Option B: Separate function for RX MOB configuration**
```c
static void CANMOBSetRX(uint8_t u8MOBIndex,
                        const SMOBDef* psDef,
                        uint8_t u8TargetModuleID);
```

### 3. Update MOB Configuration on Registration/Deregistration

**On Registration (main.c:1309):**
```c
// Assign the new registration ID
sg_u8ModuleRegistrationID = u8RegID;

// Reconfigure MOB 0 to filter on assigned ID
CANMOBSetRX(CANMOB_RX_IDX, &sg_sMOBGenericReceive, u8RegID);
```

**On Deregistration:**
```c
sg_u8ModuleRegistrationID = 0;
sg_bModuleRegistered = false;

// Reconfigure MOB 0 back to 0xFF (unregistered)
CANMOBSetRX(CANMOB_RX_IDX, &sg_sMOBGenericReceive, 0xFF);
```

### 4. Update RX Message ID Extraction

**Current code (can.c:497-498):**
```c
u16ID = ((uint16_t)CANIDT1) << 3;
u16ID |= CANIDT2 >> 5;
```

This extracts base ID from extended frame, which is correct. The module ID filtering is done by hardware, so no changes needed here.

### 5. Remove Module ID from Data Bytes

**Current:** Module-specific messages have module ID in first data byte.

**New:** Module ID only in extended frame, not in data.

This affects:
- main.c:1336-1344 - Module-specific message handling
- All message structures in protocol headers

## Implementation Steps

1. **Add MOB 2 broadcast receive configuration**
   - Define sg_sMOBBroadcastReceive
   - Configure in CANInit()
   - Add MOB 2 handling in interrupt

2. **Modify MOB filtering**
   - Add module ID filter parameter to SMOBDef or create separate RX config function
   - Update CANMOBSet() to configure module ID filtering in extended ID bits 0-7

3. **Update registration/deregistration**
   - Reconfigure MOB 0 filter on registration
   - Reconfigure MOB 0 filter on deregistration
   - Initialize MOB 0 to 0xFF in CANInit()

4. **Remove module ID from message data**
   - Update message handlers to not expect module ID in data[0]
   - Update protocol structures (future step, separate from MOB changes)

## Questions for Discussion

1. **MOB Configuration Approach**: Option A (add to SMOBDef) vs Option B (separate RX function)?
   - Option A: Cleaner, single configuration structure
   - Option B: Less impact on existing TX code

2. **Message Data Changes**: Should we remove module ID from data bytes in this change, or keep that separate?
   - Recommend: Keep separate. First get MOB filtering working, then update message formats.

3. **Interrupt Handling**: MOB 2 will trigger same interrupt as MOB 0/1. Need to handle both RX MOBs in CANMOBInterrupt().
   - Current code has CANMOB_RX_IDX hardcoded
   - Need to handle both MOB 0 and MOB 2 as RX

4. **Testing**: How to verify MOB filtering is working correctly?
   - Test unregistered: Should only receive 0xFF and 0x00 messages
   - Test registered: Should receive assigned ID and 0x00 messages
   - Test isolation: Module 1 shouldn't hear Module 2's responses

## Recommended Approach - AGREED

**Phase 1: MOB Configuration (this change)**
1. Add MOB 2 for broadcast (configured once in CANInit)
2. Add function `CANSetModuleIDFilter(uint8_t u8ModuleID)` to reconfigure MOB 0 filter
3. Call in registration/deregistration to update MOB 0 filter
4. Keep existing message format (module ID still in data) - no changes to message handlers

**Phase 2: Message Format (future change)**
1. Remove module ID from data bytes
2. Update all message handlers
3. Update protocol structures
4. Test integration

## Implementation Details for Phase 1

### New Function
```c
void CANSetModuleIDFilter(uint8_t u8ModuleID)
{
    uint8_t savedCANGIE;

    // Disable CAN interrupts during MOB reconfiguration
    savedCANGIE = CANGIE;
    CANGIE &= ~(1 << ENIT);

    // Set MOB 0 page
    CANPAGE = CANMOB_RX_IDX << 4;

    // Temporarily disable MOB 0
    CANIE2 &= ~(1 << CANMOB_RX_IDX);
    CANCDMOB &= ~((1 << CONMOB0) | (1 << CONMOB1));

    // Update extended ID filter (bits 0-7 = module ID)
    // Keep base ID (bits 18-28) unchanged at 0x500/0x700 mask
    uint32_t u32MessageID = (uint32_t)u8ModuleID;
    u32MessageID |= (((uint32_t)0x500) & 0x7ff) << 18;

    CANIDT4 = (u32MessageID << 3);
    CANIDT3 = u32MessageID >> 5;
    CANIDT2 = u32MessageID >> 13;
    CANIDT1 = u32MessageID >> 21;

    // Set mask: bits 0-7 must match exactly (0xFF), bits 18-28 use 0x700 mask
    CANIDM4 = (1 << IDEMSK);  // Extended frame mask
    CANIDM4 |= 0xFF << 3;     // Module ID bits 0-7 must match exactly
    CANIDM3 = 0xFF >> 5;      // Continue module ID mask
    CANIDM2 = 0x700 << 5;     // Base ID mask
    CANIDM1 = 0x700 >> 3;

    // Re-enable RX mode
    CANCDMOB = 8 | (1 << CONMOB1) | (1 << IDE);  // 8 bytes, RX mode, extended frame
    CANIE2 |= (1 << CANMOB_RX_IDX);

    // Restore CAN interrupts
    CANGIE = savedCANGIE;
}
```

### CANInit() Changes
```c
void CANInit(void)
{
    // ... existing init code ...

    // Setup MOB 0 for unregistered (0xFF) reception
    CANMOBSet(CANMOB_RX_IDX, &sg_sMOBGenericReceive, NULL, 0);
    CANSetModuleIDFilter(0xFF);  // Start unregistered

    // Setup MOB 2 for broadcast (0x00) reception - ALWAYS ENABLED
    CANMOBSet(2, &sg_sMOBGenericReceive, NULL, 0);
    CANSetModuleIDFilter(0x00);  // Broadcast

    // ... rest of init ...
}
```

Wait, that won't work - CANSetModuleIDFilter() sets MOB 0, not MOB 2. Need separate approach for MOB 2.

### Better Approach: Add MOB index parameter

```c
void CANSetModuleIDFilter(uint8_t u8MOBIndex, uint8_t u8ModuleID)
{
    // Same code but parameterized on u8MOBIndex
    CANPAGE = u8MOBIndex << 4;
    CANIE2 &= ~(1 << u8MOBIndex);
    // ... rest same ...
    CANIE2 |= (1 << u8MOBIndex);
}
```

### Usage
```c
// In CANInit():
CANSetModuleIDFilter(CANMOB_RX_IDX, 0xFF);  // MOB 0: unregistered
CANSetModuleIDFilter(2, 0x00);               // MOB 2: broadcast

// In registration:
CANSetModuleIDFilter(CANMOB_RX_IDX, u8RegID);  // MOB 0: my ID

// In deregistration:
CANSetModuleIDFilter(CANMOB_RX_IDX, 0xFF);     // MOB 0: back to unregistered
```

### Interrupt Handler Changes
```c
ISR(CAN_INT_vect, ISR_BLOCK)
{
    // ... existing code ...

    uint8_t sit = CANSIT2;

    // Check MOB 0 (module-specific RX)
    if(sit & (1 << CANMOB_RX_IDX)) {
        CANMOBInterrupt(CANMOB_RX_IDX);
        CANMOBSet(CANMOB_RX_IDX, &sg_sMOBGenericReceive, NULL, 0);
    }

    // Check MOB 2 (broadcast RX)
    if(sit & (1 << 2)) {
        CANMOBInterrupt(2);
        CANMOBSet(2, &sg_sMOBGenericReceive, NULL, 0);
    }

    // Check TX MOB
    if(sit & (1 << CANMOB_TX_IDX)) {
        CANMOBInterrupt(CANMOB_TX_IDX);
    }

    // ... rest of interrupt handler ...
}
```

This keeps changes minimal and focused on MOB filtering only.
