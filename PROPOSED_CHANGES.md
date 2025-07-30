# Fix Status Request Race Condition - Both Sides

## Pack Controller Changes

### 1. Add status tracking to module structure
In `/mnt/c/projects/ai-agents/Pack-Controller-EEPROM/Core/Inc/mcu.h`, add to `moduleData` structure:
```c
uint8_t statusMessagesReceived;  // Bitmask: bit0=Status1, bit1=Status2, bit2=Status3
```

### 2. Modify MCU_ProcessModuleStatus1/2/3
In `/mnt/c/projects/ai-agents/Pack-Controller-EEPROM/Core/Src/mcu.c`:

```c
// In MCU_ProcessModuleStatus1 (after line 1527):
// Track which status message was received
module[moduleIndex].statusMessagesReceived |= (1 << 0);  // Status1 received

// In MCU_ProcessModuleStatus2 (after line 1631):
module[moduleIndex].statusMessagesReceived |= (1 << 1);  // Status2 received

// In MCU_ProcessModuleStatus3 (after line 1705):
module[moduleIndex].statusMessagesReceived |= (1 << 2);  // Status3 received

// Only clear statusPending when all 3 received
if(module[moduleIndex].statusMessagesReceived == 0x07) {  // All 3 bits set
    module[moduleIndex].statusPending = false;
    module[moduleIndex].statusMessagesReceived = 0;  // Reset for next time
}
```

### 3. Reset status tracking when sending request
In `MCU_RequestModuleStatus()` (after setting statusPending = true):
```c
module[moduleIndex].statusMessagesReceived = 0;  // Clear previous status bits
```

### 4. Handle timeout with partial messages
In timeout handling (line ~338), also clear the status bits:
```c
module[index].statusMessagesReceived = 0;  // Clear any partial status
```

## Module Controller Changes

### 1. Add ignore flag
In `/mnt/c/projects/ai-agents/modulecpu/main.c`, add near other status flags (around line 240):
```c
static bool sg_bIgnoreStatusRequests = false;  // Ignore new requests while sending
```

### 2. Set flag when starting status sequence
In `SendModuleControllerStatus()` (after line 554):
```c
sg_bSendModuleControllerStatus = true;
sg_u8ControllerStatusMsgCount = 0;
sg_bIgnoreStatusRequests = true;  // Start ignoring new requests
```

### 3. Clear flag when sequence completes or fails
In `ControllerStatusMessagesSend()`:

After successful completion (line ~1817):
```c
sg_bSendModuleControllerStatus = false;
sg_bIgnoreStatusRequests = false;  // Allow new requests again
```

On send failure (in the error handling):
```c
// If we fail to send, keep ignoring requests - wait for deregistration
if( false == bSent )
{
    bStatusSendSuccessful = false;
    // DO NOT clear sg_bIgnoreStatusRequests here
    // Let pack controller timeout and deregister us
}
```

### 4. Check flag in status request handler
In CAN callback where PKT_MODULE_STATUS_REQUEST is handled:
```c
case PKT_MODULE_STATUS_REQUEST:
    if (!sg_bIgnoreStatusRequests) {
        SendModuleControllerStatus();
    }
    // else silently ignore - we're already sending
    break;
```

### 5. Clear flag on deregistration
When module is deregistered:
```c
sg_bModuleRegistered = false;
sg_bIgnoreStatusRequests = false;  // Reset all status flags
```

## Testing
1. Verify module sends all 3 status messages without restart
2. Verify pack controller waits for all 3 before clearing statusPending
3. Verify timeout still works if module fails mid-sequence
4. Verify deregistration clears all flags properly