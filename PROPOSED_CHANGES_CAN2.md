# Additional CAN Fix - Remove TX Handling from RX MOB

## Issue
We're still checking and clearing TXOK status bit in the RX MOB handler. This doesn't make sense - the RX MOB should only handle RX events, not TX status.

## Current Code (can.c lines 364-376)
```c
if( CANMOB_RX_IDX == u8MOBIndex )
{
    // TX success?  Just clear it since this is the RX context
    if( CANSTMOB & (1 << TXOK) )
    {
        // Clear it
        CANSTMOB &= ~(1 << TXOK);
        // [comments about not clearing sg_bBusy]
    }
    // RX success?
    if( CANSTMOB & (1 << RXOK) )
    {
        // ... handle RX
```

## Problem Analysis
1. **RX MOB shouldn't see TX status**: If we're in the RX MOB interrupt handler, we shouldn't be seeing TX status bits
2. **Possible hardware quirk**: Maybe status bits can leak between MOBs? 
3. **Could cause confusion**: Clearing TX bits in wrong context might interfere with actual TX operations

## Proposed Fix

### Option 1: Remove TX handling entirely from RX MOB
```c
if( CANMOB_RX_IDX == u8MOBIndex )
{
    // RX success?
    if( CANSTMOB & (1 << RXOK) )
    {
        // Clear it
        CANSTMOB &= ~(1 << RXOK);
        // ... rest of RX handling
```

### Option 2: Add assertion/debug to understand why this happens
```c
if( CANMOB_RX_IDX == u8MOBIndex )
{
    // This should NEVER happen - RX MOB shouldn't see TX status
    if( CANSTMOB & (1 << TXOK) )
    {
        // Log error or increment error counter
        sg_u32UnexpectedTxInRx++;  // Debug counter
        // Clear it to prevent stuck interrupt
        CANSTMOB &= ~(1 << TXOK);
    }
```

## Recommendation
Go with **Option 1** - completely remove TX handling from RX MOB context. TX status should only be handled in TX MOB handler.

## Risk Assessment
- **Low risk**: TX handling belongs in TX MOB only
- **Possible side effect**: If there's a hardware quirk causing TX bits to appear in RX MOB, not clearing them might cause stuck interrupts
- **Mitigation**: Monitor for any interrupt lockups after change