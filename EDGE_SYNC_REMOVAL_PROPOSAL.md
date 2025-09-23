# Proposed: Remove Edge Sync from ModuleCPU VUART

## Current State
ModuleCPU has edge sync implemented in vUART.c that:
- Detects edges during byte reception
- Resyncs timer on every edge (line 301)
- Tracks timing error statistics
- Switches between rising/falling edge detection

## Why Remove It

### 1. Consistency with CellCPU
- CellCPU had edge sync removed due to relay timing corruption
- Even though ModuleCPU doesn't relay, we want consistent behavior

### 2. Potential Issues
- If ModuleCPU ever needs to relay in future, edge sync would break it
- Edge sync adds complexity and ISR overhead
- The timing adjustment could cause issues with other time-critical code

### 3. Simpler is Better
- Fixed timing has proven reliable
- Less code to maintain
- Easier to debug

## What to Change

### In INT1_vect ISR:
1. Remove edge detection during reception
2. Disable interrupt after start bit (like original CellCPU)
3. Remove the resync code (lines 285-305)

### Specifically:
```c
// Current (line 259-261):
VUART_RX_ANY_EDGE();
// Note: interrupt remains enabled for edge detection

// Change to:
VUART_RX_DISABLE();  // Disable until next byte
```

Remove the entire edge correction block (lines 285-305)

### In Timer ISR:
Re-enable interrupt for next byte (already done at line 358-359)

## Testing Required
- Verify communication still works with all cells
- Check for any timing issues
- Monitor for increased error rates

## Alternative
Keep statistics gathering but don't adjust timer - this would let us monitor timing drift without affecting operation.