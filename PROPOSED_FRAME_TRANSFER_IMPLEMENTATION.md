# Proposed Frame Transfer Implementation

## Phase 1: Minimal Viable Implementation

Start with the simplest approach that works, then add robustness incrementally.

### Proposed Protocol (Simple)

**1. Frame Transfer Request** (Pack → Module)
```c
ECANMessageType_FrameTransferRequest
Byte 0: Module ID (which module to request from)
Byte 1-4: Frame counter (uint32_t, little-endian)
          0xFFFFFFFF = current frame in RAM
          Other value = retrieve specific frame from SD card
Bytes 5-7: Reserved
```

**2. Frame Transfer Start** (Module → Pack)
```c
ECANMessageType_FrameTransferStart
Byte 0-3: Frame counter (uint32_t, little-endian)
Byte 4-5: Total messages (uint16_t) = 129 (1 start + 128 data)
Byte 6-7: Reserved
```

**3. Frame Data** (Module → Pack, 128 messages)
```c
ECANMessageType_FrameTransferData
Byte 0-7: 8 bytes of frame data
```

Messages sent in order, starting from frame offset 0.
- Message 0: Frame bytes 0-7
- Message 1: Frame bytes 8-15
- ...
- Message 127: Frame bytes 1016-1023

**4. Frame Transfer End** (Module → Pack)
```c
ECANMessageType_FrameTransferEnd
Byte 0-3: CRC32 of entire 1024-byte frame
Byte 4-7: Reserved
```

### Benefits of This Approach
1. **Simple**: No explicit sequence numbers in data messages
2. **Fast**: Back-to-back transmission, no ACKs between segments
3. **Verifiable**: CRC32 at end validates integrity
4. **Incrementable**: Can add sequence numbers/ACKs later if needed

### Implementation Steps

#### Step 1: Add CAN Message Types
Edit `can.h`:
```c
typedef enum
{
    // ... existing messages ...
    ECANMessageType_MaxState,

    // Frame transfer (add before ECANMessageType_MAX)
    ECANMessageType_FrameTransferRequest,  // Pack → Module
    ECANMessageType_FrameTransferStart,    // Module → Pack
    ECANMessageType_FrameTransferData,     // Module → Pack
    ECANMessageType_FrameTransferEnd,      // Module → Pack

    ECANMessageType_MAX
} ECANMessageType;
```

#### Step 2: Add Transfer State in main.c
```c
// Frame transfer state
typedef enum {
    FRAME_TRANSFER_IDLE,
    FRAME_TRANSFER_SENDING_START,
    FRAME_TRANSFER_SENDING_DATA,
    FRAME_TRANSFER_SENDING_END,
} FrameTransferState;

static FrameTransferState sg_eFrameTransferState = FRAME_TRANSFER_IDLE;
static uint8_t sg_u8FrameTransferSegment = 0;  // Current segment being sent
static volatile FrameData* sg_pFrameToTransfer = NULL;  // Pointer to frame being transferred
```

#### Step 3: Add Request Handler
In `CANReceiveCallback()`:
```c
if (ECANMessageType_FrameTransferRequest == eType) {
    // Only respond if addressed to us (or broadcast)
    if (pu8Data[0] == sg_u8ModuleID || pu8Data[0] == 0xFF) {
        // Extract requested frame counter
        uint32_t requestedFrame = *(uint32_t*)&pu8Data[1];

        if (requestedFrame == 0xFFFFFFFF) {
            // Use current frame in RAM
            sg_pFrameToTransfer = &sg_sFrame;
        } else {
            // TODO: Retrieve frame from SD card
            // For now, just use current frame
            sg_pFrameToTransfer = &sg_sFrame;
        }

        // Initiate frame transfer
        sg_eFrameTransferState = FRAME_TRANSFER_SENDING_START;
        sg_u8FrameTransferSegment = 0;
    }
}

// While transferring, ignore status/cell detail requests
if (sg_eFrameTransferState != FRAME_TRANSFER_IDLE) {
    if (eType == ECANMessageType_ModuleStatusRequest ||
        eType == ECANMessageType_ModuleCellDetailRequest) {
        // Ignore these while transferring
        return;
    }

    // State change requests are still processed (critical for safety)
}
```

#### Step 4: Add Transfer Logic in Main Loop
```c
void ProcessFrameTransfer(void) {
    uint8_t buffer[8];

    switch (sg_eFrameTransferState) {
        case FRAME_TRANSFER_IDLE:
            // Nothing to do
            break;

        case FRAME_TRANSFER_SENDING_START:
            // Send start message with frame counter
            *(uint32_t*)&buffer[0] = sg_pFrameToTransfer->m.frameCounter;
            *(uint16_t*)&buffer[4] = 129;  // Total messages (1 start + 128 data + 1 end)
            buffer[6] = 0;
            buffer[7] = 0;

            if (CANSendMessage(ECANMessageType_FrameTransferStart, buffer, 8)) {
                sg_eFrameTransferState = FRAME_TRANSFER_SENDING_DATA;
                sg_u8FrameTransferSegment = 0;
            }
            break;

        case FRAME_TRANSFER_SENDING_DATA:
            if (sg_u8FrameTransferSegment < 128) {
                // Calculate offset into frame
                uint16_t offset = sg_u8FrameTransferSegment * 8;

                // Copy 8 bytes from frame
                uint8_t *frameBytes = (uint8_t*)sg_pFrameToTransfer;
                memcpy(buffer, &frameBytes[offset], 8);

                if (CANSendMessage(ECANMessageType_FrameTransferData, buffer, 8)) {
                    sg_u8FrameTransferSegment++;
                }
            } else {
                // All data sent, move to end
                sg_eFrameTransferState = FRAME_TRANSFER_SENDING_END;
            }
            break;

        case FRAME_TRANSFER_SENDING_END:
            // Calculate CRC32 of entire frame
            uint32_t crc = CalculateCRC32((uint8_t*)sg_pFrameToTransfer,
                                          sizeof(FrameData));

            *(uint32_t*)&buffer[0] = crc;
            buffer[4] = 0;
            buffer[5] = 0;
            buffer[6] = 0;
            buffer[7] = 0;

            if (CANSendMessage(ECANMessageType_FrameTransferEnd, buffer, 8)) {
                sg_eFrameTransferState = FRAME_TRANSFER_IDLE;
                sg_pFrameToTransfer = NULL;  // Clear pointer
            }
            break;
    }
}
```

#### Step 5: Call from Main Loop
```c
void main(void) {
    while(1) {
        // ... existing code ...

        // Process frame transfer if active
        ProcessFrameTransfer();

        // ... rest of main loop ...
    }
}
```

### CRC32 Implementation

Need to add a simple CRC32 function. Options:

**Option A**: Use lookup table (fast, uses 1KB ROM)
**Option B**: Bit-by-bit (slow, minimal ROM)

For embedded, recommend bit-by-bit to save flash:

```c
uint32_t CalculateCRC32(const uint8_t *data, uint16_t length) {
    uint32_t crc = 0xFFFFFFFF;

    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }

    return ~crc;
}
```

## Limitations & Future Enhancements

### Current Limitations
1. **No retransmission**: If any message lost, entire transfer fails
2. **No flow control**: Module sends as fast as possible
3. **Single-threaded**: Transfer blocks other CAN operations
4. **No priority**: All segments have same priority

### Possible Enhancements (Later)
1. Add sequence numbers to detect dropped messages
2. Add ACK/NACK mechanism for retransmission
3. Rate-limit to avoid bus saturation
4. Add priority field to interleave with status messages
5. Compress frame data before transfer

## Design Decisions (CONFIRMED)

1. **Transfer trigger**: On Pack Controller request only

2. **Frame Selection**: Request message contains:
   - Specific frame counter → retrieve from SD card
   - 0xFFFFFFFF → return current frame in RAM

3. **Priority During Transfer**:
   - **BLOCK** all outgoing messages (status, cell detail, etc.)
   - **KEEP RECEIVING** all incoming messages
   - **IGNORE** status and cell detail requests while transferring
   - **PROCESS** state change requests (critical for safety)

4. **Error handling**:
   - Pack Controller is master
   - Pack requests retransmit if CRC fails
   - Module does not auto-retry
