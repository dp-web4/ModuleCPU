# ModuleCPU Web4 Integration

**Platform**: ATmega64M1 (8-bit AVR with CAN)
**Role**: Middle management layer in hierarchical battery system
**Web4 Context**: Demonstrates trust-native coordination in embedded systems

---

## Overview

ModuleCPU can be viewed as a practical demonstration of Web4 trust-native architecture principles applied to embedded battery management. Operating as the middle tier in a three-level hierarchy (Cell→Module→Pack), it illustrates how concepts like unforgeable identity, witnessed relationships, and trust metrics can be implemented in resource-constrained hardware.

---

## Web4 Concepts as Design Patterns

The ModuleCPU architecture parallels several Web4 concepts, which can serve as useful frameworks for understanding the system design:

### 1. LCT-Style Identity

**Concept**: Each module could have an unforgeable blockchain identity similar to an LCT

**Current Implementation**:
- Module has unique 64-bit ID (hardware serial or EEPROM)
- ID transmitted in CAN announcement messages
- Pack Controller assigns module ID (1-31) upon registration

**Web4 Pattern**:
```
Hardware Serial: MODBATT-MODULE-001-ABC123
    ↓ (could be bound to)
LCT: lct:web4:entity:battery_module:ABC123
    ↓ (via API-Bridge)
Blockchain identity with unforgeable binding
```

**Current Status**: Framework ready for LCT integration via Pack Controller API-Bridge

### 2. Markov Blanket as Interface Design

**Concept**: The module's interface can be understood as a form of "Markov blanket" - hiding internal complexity

**Interfaces (Information Boundaries)**:
- **Downward (to 94 CellCPUs)**: VUART protocol
  - ModuleCPU sees: Cell temp, voltage, status
  - ModuleCPU doesn't see: Cell's I2C timing, ADC samples, internal state machine

- **Upward (to Pack Controller)**: CAN bus protocol
  - Pack sees: Module aggregated data, state, cell summary
  - Pack doesn't see: Individual cell polling sequence, VUART timing, internal algorithms

**Design Insight**: This layered hiding of complexity mirrors the Markov blanket concept - each level provides an interface that abstracts away lower-level details.

### 3. MRH-Style Relationship Tracking

**Concept**: The module maintains relationships similar to a Markov Relevancy Horizon graph

**Relationship Graph**:
```
ModuleCPU maintains awareness of:
├─ Upward: Pack Controller (CAN bus, module ID assignment)
├─ Downward: Up to 94 CellCPUs (VUART, position in chain)
└─ Siblings: Other modules (via Pack Controller broadcasts)

This forms a local "relevancy horizon" - the module's operational context
```

**Implementation**:
- Module knows its assigned ID (1-31)
- Module tracks which cells are responding (cell bitmap)
- Module receives broadcast commands affecting all modules
- Module receives targeted commands for its ID

**Web4 Parallel**: This relationship tracking parallels MRH tensor links, where each entity maintains its graph of relevant relationships.

### 4. Trust Tensor Patterns

**Concept**: Trust and value metrics could be tracked for cells and modules

**Potential T3 (Trust) Dimensions** for a cell as seen by module:
- **Talent**: Sensor accuracy (does cell report reasonable values?)
- **Training**: Calibration state (has cell been calibrated?)
- **Temperament**: Reliability (does cell respond consistently?)

**Potential V3 (Value) Dimensions**:
- **Veracity**: Data honesty (are readings within bounds?)
- **Validity**: Spec compliance (is data in valid range?)
- **Value**: Usefulness (does cell provide actionable information?)

**Current Implementation**:
- Module tracks cell communication statistics
- Module can detect anomalous readings
- Module monitors cell response timing

**Enhancement Path**: These statistics could be formalized into T3/V3 tensors and reported to Pack Controller for blockchain recording.

### 5. Entity Relationship Mechanisms

ModuleCPU's design parallels the four Web4 entity relationship types:

#### Binding (Hardware → Identity)
**Pattern**: Module serial number could be permanently bound to blockchain LCT

**Current**: Serial number in firmware/EEPROM
**Enhancement**: Binding ceremony at manufacturing creates immutable hardware→LCT association

#### Pairing (Module ↔ Pack Controller)
**Pattern**: Cryptographic pairing relationship

**Current**: CAN bus registration (module announces, pack assigns ID)
**Enhancement**:
```
Module: Generates key pair, includes public key in announcement
Pack: Challenges module
Module: Signs challenge, proves identity
Pack: Verifies signature, establishes secure channel
    ↓
Symmetric session key created for encrypted CAN messages
```

#### Witnessing (Mutual Observation)
**Pattern**: Module witnesses cells; Pack witnesses module

**Downward Witnessing** (Module → Cells):
```
Module polls cells via VUART
    ↓ (observes responses)
Module confirms: "Cell 5 exists and responds"
    ↓ (via Pack Controller/API-Bridge)
Blockchain: Witnessing event recorded
```

**Upward Witnessing** (Pack → Module):
```
Pack receives CAN announcement from module
    ↓ (observes presence)
Pack confirms: "Module ABC123 is online"
    ↓
Blockchain: Module presence witnessed and recorded
```

#### Broadcast (Announcement)
**Pattern**: Module broadcasts state to network

**Current**: CAN message `ID_MODULE_ANNOUNCEMENT` (0x500FF for unregistered)
**Web4 Parallel**: This is analogous to Web4 broadcast - module announces presence to anyone listening

---

## Protocol Synchronization

**Critical Dependency**: ModuleCPU uses CAN protocol definitions that are maintained as a shared standard.

**Authoritative Source**: `Pack-Controller-EEPROM/protocols/`
**Local Copy**: `ModuleCPU/protocols/` (synchronized via `scripts/sync_protocols.sh`)

This separation can be understood through a Web4 lens:
- **Shared Protocol = Consensus Layer**: All modules and pack must agree on message formats
- **Local Copy = Implementation**: Each repo has a snapshot of the consensus
- **Sync Script = Consensus Update Mechanism**: Pull latest protocol changes from authoritative source

See `protocols/SYNC_NOTICE.md` for synchronization procedure.

---

## R7-Style Action Framework

CAN messages exchanged by ModuleCPU can be viewed through an R7 (Rules, Role, Request, Reference, Resource, Result, Reputation) lens:

### Example: Module Status Report

**Rules**: CAN protocol, module must be registered
**Role**: Module acting as "battery_module_coordinator" reporting to Pack
**Request**: "report_status" action targeting Pack Controller
**Reference**: Previous status messages, module's history
**Resource**: Aggregated cell data (94 cells × temp/voltage)
**Result**: Status message transmitted, ACK received
**Reputation**: Trust metrics could be updated based on:
  - Timeliness of response
  - Accuracy of data
  - Consistency with previous reports

---

## Practical Implementation Notes

### Current Capabilities

ModuleCPU firmware currently implements:
- ✅ Unique module identification
- ✅ CAN bus registration with Pack Controller
- ✅ VUART communication with up to 94 cells
- ✅ Cell data aggregation and reporting
- ✅ Module-level state management
- ✅ Error detection and reporting

### Web4 Integration Path

To fully integrate ModuleCPU into a Web4 network:

**Phase 1**: Identity Binding (via Pack Controller)
```
1. Module transmits serial number via CAN
2. Pack Controller calls API-Bridge: CreateEntity(serial)
3. API-Bridge returns LCT
4. Pack stores Module→LCT binding
5. All future CAN messages reference LCT
```

**Phase 2**: Cryptographic Pairing
```
1. Add ECDH key exchange to registration
2. Establish symmetric session key
3. Encrypt sensitive CAN data
4. Record pairing in blockchain via API-Bridge
```

**Phase 3**: Trust Tensor Tracking
```
1. Module calculates T3/V3 for each cell
2. Pack Controller aggregates module T3/V3
3. Periodic updates sent to blockchain
4. Historical trust trends tracked
```

**Phase 4**: Witnessed Presence
```
1. Every CAN message is a witnessing event
2. Pack records: "I witnessed Module 5 at timestamp T"
3. Blockchain accumulates witnessing history
4. Module's digital presence strengthens over time
```

---

## Code Examples

### 1. Module Identity (CAN Announcement)

```c
// In can.c - Module announces its presence
void TransmitModuleAnnouncement(void) {
    CANMessage msg;
    msg.id = 0x500FF;  // Announcement ID with moduleID=0xFF (unregistered)
    msg.dlc = 8;

    // Pack unique ID into message
    msg.data[0] = (g_unique_id >> 56) & 0xFF;
    msg.data[1] = (g_unique_id >> 48) & 0xFF;
    // ... (8 bytes total)

    CANTransmit(&msg);

    // In Web4 terms: This is a "broadcast" to the network
    // Pack Controller "witnesses" this announcement
    // Could trigger LCT creation via API-Bridge
}
```

### 2. Cell Witnessing (VUART Polling)

```c
// In vuart.c - Module witnesses cell presence
bool PollCell(uint8_t cell_index) {
    // Request data from cell
    VUARTTransmitRequest(cell_index);

    // Receive response
    if (VUARTReceive(&cell_data)) {
        // Witnessing event: "I observe cell N exists and responds"
        UpdateCellBitmap(cell_index, PRESENT);

        // In Web4 terms:
        // - Module witnessed cell
        // - Cell's presence confirmed
        // - Trust tensor updated: +Temperament (reliable response)

        return true;
    } else {
        UpdateCellBitmap(cell_index, ABSENT);

        // Witnessing absence:
        // - Cell did not respond
        // - Trust tensor updated: -Temperament (unreliable)

        return false;
    }
}
```

### 3. Relationship Graph (MRH-style)

```c
// Module's "MRH" - who it knows about
typedef struct {
    uint8_t pack_controller_id;     // Upward link (my "God")
    uint8_t my_module_id;            // My identity (1-31, or 0xFF if unregistered)
    uint64_t cell_bitmap;            // Downward links (which cells are present)
    uint32_t last_seen_timestamp;   // When Pack last communicated
} ModuleRelationships;

// This structure mirrors an MRH graph:
// - Entity (self): my_module_id
// - Relationships:
//   - Pairing: pack_controller_id (bidirectional)
//   - Witnessing: cell_bitmap (which cells I've seen)
//   - Temporal: last_seen_timestamp (relationship freshness)
```

---

## Synchronism Patterns in Design

The ModuleCPU architecture can be understood through several patterns from the Synchronism framework:

### Scale-Based Autonomy

**Pattern**: ModuleCPU operates autonomously at the "module scale"

**Autonomous Decisions**:
- When to poll cells (timing strategy)
- How to aggregate cell data (averaging, outlier detection)
- Error handling (retry logic, timeout responses)
- State transitions (off→standby→active)

**Constraints from Above** (Pack Controller):
- Maximum allowed state (can't go to ACTIVE if pack says OFF)
- Registration required before normal operation

**Constraints from Below** (Cells):
- Must respect VUART timing
- Can only request data cells are capable of providing

### Hierarchical Information Flow

**Pattern**: Information flows up (aggregation) and down (commands)

**Upward** (94 cells → 1 module summary):
```
Cell 1: 23.5°C, 3.65V
Cell 2: 23.7°C, 3.64V
...
Cell 94: 23.6°C, 3.65V
    ↓ (aggregation)
Module: Average=23.6°C, Min=3.63V, Max=3.67V, ΔV=40mV
    ↓ (to Pack)
Pack sees: Module-level summary, not individual cells
```

**Downward** (Pack command → cell actions):
```
Pack: "All modules enter BALANCE state"
    ↓ (module interprets)
Module: Polls cells, enables balancing for cells >3.65V
    ↓ (cell executes)
Cells: Activate balancing FETs based on local voltage
```

### Emergent System Behavior

**Pattern**: System-level intelligence emerges from local decisions at each scale

**No Central Balancing Algorithm**:
- Cells decide when to balance (local voltage threshold)
- Module decides which cells to request balancing (aggregate strategy)
- Pack decides when modules can balance (system-level constraint)
- **Result**: Coordinated balancing emerges without centralized control

---

## Documentation Links

- [ModuleCPU CLAUDE.md](CLAUDE.md) - Development context and protocol resolution
- [CellCPU](https://github.com/dp-web4/CellCPU) - Subordinate layer (cells)
- [Pack-Controller-EEPROM](https://github.com/dp-web4/Pack-Controller-EEPROM) - Superior layer (pack)
- [Web4 Standard](https://github.com/dp-web4/web4) - Trust-native architecture concepts
- [Synchronism](https://github.com/dp-web4/Synchronism) - Philosophical framework

---

## Summary

ModuleCPU demonstrates how concepts from Web4 and Synchronism can inform embedded system design:

- **Identity**: Module serial → could bind to LCT
- **Relationships**: CAN pairing with Pack, VUART witnessing of cells
- **Interfaces**: Markov blanket-style information hiding
- **Hierarchy**: Scale-based autonomy with constraints from above/below
- **Trust**: Framework for tracking reliability and value
- **Emergence**: System intelligence from local decisions

While the current implementation focuses on battery management functionality, the architecture is compatible with Web4 trust-native enhancements and demonstrates patterns that align with distributed intelligence principles.

**This serves as a practical reference for how Web4 concepts might be applied in resource-constrained embedded systems.**
