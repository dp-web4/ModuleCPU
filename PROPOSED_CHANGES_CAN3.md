# Critical CAN Fix - TX MOB Not Re-enabled

## Issue Found
In `CANMOBInterrupt()`, the MOB is disabled at the start:
```c
// Line 359-360
CANIE2 &= (uint8_t)~(1 << u8MOBIndex);
CANCDMOB &= (uint8_t)~((1 << CONMOB0) | (1 << CONMOB1));
```

The RX MOB is properly re-enabled at the end of its handler:
```c
// Line 430-431
CANIE2 |= (1 << u8MOBIndex);
CANCDMOB |= (1 << CONMOB1);
```

**BUT the TX MOB is NEVER re-enabled!** The TX handler ends at line 471 without re-enabling.

## Impact
- TX MOB stays disabled after first transmission
- Module can receive but not transmit after first TX
- This explains why the module "goes quiet" - it literally can't send anymore!

## Proposed Fix
Add TX MOB re-enable at the end of TX handler:

```c
// At line 471, after the TX MOB handler, add:
else if( CANMOB_TX_IDX == u8MOBIndex )
{
    // ... existing TX handling code ...
    
    // Re-enable TX MOB (THIS IS MISSING!)
    // For TX, we just re-enable interrupts, don't set CONMOB bits
    // as TX is one-shot operation
    CANIE2 |= (1 << u8MOBIndex);
}
```

Actually, looking closer, we might not need to re-enable TX MOB for interrupts since TX is typically one-shot. But we should verify this is intentional.

## Alternative Analysis
It's possible TX MOB doesn't need re-enabling because:
1. TX is one-shot operation
2. New TX is initiated by CANMOBSet() which would set everything up fresh
3. But then why disable it in the first place?

## Recommendation
1. Either remove the disable for TX MOB at the start
2. OR add proper re-enable at the end
3. Test both approaches to see which works better

This could be THE bug causing all our CAN reliability issues!