# Frame Transfer Protocol Specification

## Overview
Protocol for transferring 1024-byte frames from ModuleCPU to Pack Controller over CAN bus.

## Frame Structure
- **Total Size**: 1024 bytes
- **FrameMetadata**: Variable size (currently ~128 bytes)
- **Cell Data Buffer**: Remaining bytes

## CAN Message Constraints
- **Max Payload**: 8 bytes per CAN message
- **Messages Required**: 128 messages per frame (1024 / 8 = 128)

## Protocol Design

### Message Types
Add to `ECANMessageType` enum in `can.h`:
```c
// Frame transfer messages (Module → Pack)
ECANMessageType_FrameTransferStart,    // Initiate frame transfer
ECANMessageType_FrameTransferData,     // Frame data segment
ECANMessageType_FrameTransferEnd,      // Complete frame transfer

// Frame transfer control (Pack → Module)
ECANMessageType_FrameTransferRequest,  // Request frame transfer
ECANMessageType_FrameTransferAck,      // Acknowledge segment received
ECANMessageType_FrameTransferNack,     // Request retransmit
```

### Transfer Sequence

#### 1. Frame Transfer Request (Pack → Module)
Pack Controller initiates transfer:
```
Byte 0: Module ID (0 for specific module)
Byte 1-4: Frame counter or timestamp (optional filter)
```

#### 2. Frame Transfer Start (Module → Pack)
Module responds with frame metadata:
```
Byte 0: Sequence number (0)
Byte 1-2: Total segments (128 for 1024-byte frame)
Byte 3-6: Frame counter (32-bit)
Byte 7: Checksum seed (XOR of all metadata bytes)
```

#### 3. Frame Transfer Data (Module → Pack)
128 segments, each containing 8 bytes:
```
Byte 0: Sequence number (1-127)
Byte 1-7: Frame data (7 bytes per segment)
```

**Note**: First byte of each data message is sequence number, leaving 7 bytes for actual data.
- Segments 1-127: 7 bytes each = 889 bytes
- Need different approach for full 1024 bytes

### Revised Data Transfer Strategy

Since we lose 1 byte per message to sequence number:
- Use **Start message** differently to maximize data transfer
- Or use **implicit sequencing** based on CAN message order

#### Option A: Implicit Sequencing
```
Start Message:
Byte 0-3: Frame counter (32-bit)
Byte 4-7: First 4 bytes of frame data

Data Messages (147 messages):
Byte 0-7: 8 bytes of frame data each

End Message:
Byte 0-7: Last bytes + checksum
```

Total: 4 + (147 × 8) = 1180 bytes (enough for 1024-byte frame + checksum)

#### Option B: Explicit Sequencing (More Robust)
```
Start Message:
Byte 0: Total segments (high byte)
Byte 1: Total segments (low byte)
Byte 2-5: Frame counter (32-bit)
Byte 6-7: Reserved

Data Messages (128 messages):
Byte 0: Segment number (0-127)
Byte 1-7: 7 bytes of frame data

End Message:
Byte 0: 0xFF (end marker)
Byte 1-4: CRC32 checksum
Byte 5-7: Reserved
```

Total data capacity: 128 × 7 = 896 bytes ❌ **Not enough for 1024 bytes!**

#### Option C: Two-Stage Transfer (Recommended)
**Stage 1: Metadata Transfer** (critical data, with sequencing)
```
Start Message:
Byte 0-3: Frame counter
Byte 4-7: Metadata size (bytes)

Metadata Segments (variable, ~20 messages for 128-byte metadata):
Byte 0: Segment number
Byte 1-7: Metadata bytes (7 bytes per segment)
```

**Stage 2: Data Transfer** (bulk data, implicit sequencing for speed)
```
Data Messages (112 messages for 896-byte buffer):
Byte 0-7: 8 bytes of cell data each

End Message:
Byte 0-3: CRC32 of entire frame
Byte 4-7: Reserved
```

## Recommended Implementation: Hybrid Approach

### Transfer Phases

**Phase 1: Start**
```c
// ECANMessageType_FrameTransferStart
Byte 0-3: Frame counter (uint32_t)
Byte 4-5: Frame size (uint16_t) = 1024
Byte 6: Metadata size in 8-byte chunks
Byte 7: Flags (bit 0: has CRC, bits 1-7: reserved)
```

**Phase 2: Metadata** (Sequenced)
```c
// ECANMessageType_FrameTransferData with sequence
Byte 0: Segment number (0x00 - metadata start marker)
Byte 1: Chunk index (0 to N)
Byte 2-7: Metadata bytes (6 bytes per message)
```

**Phase 3: Cell Data** (Fast, implicit sequence)
```c
// ECANMessageType_FrameTransferData (no sequence byte)
Byte 0-7: Cell data (8 bytes per message)
```

**Phase 4: End**
```c
// ECANMessageType_FrameTransferEnd
Byte 0-3: CRC32 checksum
Byte 4: Status (0=success)
Byte 5-7: Reserved
```

## Error Handling

### Timeout Detection
- Pack Controller: 100ms timeout per segment
- Module: Aborts transfer if no ACK within 500ms

### Retransmission
- Pack sends NACK with failed segment number
- Module retransmits specific segment
- Max 3 retries per segment

### Abort Conditions
- CRC mismatch
- Timeout exceeded
- Unexpected sequence number
- Bus error during transfer

## Flow Control

### Without ACK (Fast Mode)
Module sends all segments back-to-back. Pack Controller validates at end.

### With ACK (Reliable Mode)
Module waits for ACK every N segments (e.g., every 16 segments).

## State Machine

### Module States
```
IDLE → TRANSFER_REQUESTED → SENDING_START → SENDING_METADATA →
SENDING_DATA → SENDING_END → WAIT_FINAL_ACK → COMPLETE/ABORT → IDLE
```

### Pack Controller States
```
IDLE → REQUEST_SENT → RECEIVING_START → RECEIVING_METADATA →
RECEIVING_DATA → RECEIVING_END → VALIDATE_CRC → ACK_SENT → IDLE
```

## Implementation Priority

1. **Phase 1**: Simple bulk transfer (no segmentation, just raw 128 × 8-byte messages)
2. **Phase 2**: Add Start/End messages with frame counter and CRC
3. **Phase 3**: Add sequence numbers and retransmission
4. **Phase 4**: Optimize with metadata/data separation

## Data Rate Calculation

At 500 kbps CAN with 50% bus utilization:
- **CAN frame overhead**: ~60 bits (arbitration, CRC, ACK, EOF)
- **Data frame**: 11-bit ID + 64 data bits + overhead ≈ 135 bits
- **Time per message**: 135 bits / 500 kbps = 270 μs
- **128 messages**: 128 × 270 μs = 34.6 ms per frame
- **Transfer rate**: ~1 frame per 35ms = ~28 frames/second

With 50% bus overhead for other traffic: ~14 frames/second achievable
