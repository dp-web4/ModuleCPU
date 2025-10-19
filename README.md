# ModuleCPU

**ModBatt Battery Management System - Module Controller**

## Overview

ModuleCPU is the middle-tier controller in ModBatt's hierarchical battery management system, coordinating up to 128 individual CellCPUs while reporting aggregated data to the Pack Controller. It demonstrates how distributed intelligence principles can be applied to embedded battery management at the module scale.

**Implementation Status**: ✅ **Production-ready** - Complete firmware with CAN bus communication, VUART cell management, and hierarchical coordination

### Quick Facts

- **Target Platform**: ATmega64M1 microcontroller (8-bit AVR with CAN)
- **Manages**: Up to 128 CellCPUs via Virtual UART daisy-chain
- **Uplink**: CAN 2.0B bus to Pack Controller (250kbps)
- **Features**: Dual-phase operation, RTC timestamping, robust error handling
- **Role**: Middle management - aggregates cell data, coordinates without micromanaging

## Web4 Ecosystem Context

ModuleCPU can be viewed as a practical demonstration of trust-native architecture patterns applied to embedded battery management:

**Philosophical Framework**: [Synchronism](https://github.com/dp-web4/Synchronism) - Patterns of scale-based coordination and distributed intelligence
**Technical Standard**: [Web4](https://github.com/dp-web4/web4) - Concepts like unforgeable identity and witnessed relationships
**Implementation**: [CellCPU](https://github.com/dp-web4/CellCPU) → **ModuleCPU** (this) → [Pack-Controller](https://github.com/dp-web4/Pack-Controller-EEPROM)

### Design Patterns

ModuleCPU's architecture reflects several patterns that align with Web4 and Synchronism concepts:

- **Identity**: Module serial number framework ready for LCT binding
- **Interface Boundaries**: Well-defined information hiding between layers (CAN upward, VUART downward)
- **Relationship Tracking**: Module maintains awareness of Pack Controller and 94 cells
- **Hierarchical Coordination**: Operates autonomously within constraints from Pack
- **Information Aggregation**: Compresses 188 cell data points into module-level summaries
- **Emergent Intelligence**: System-level balancing from distributed local decisions

See [WEB4_INTEGRATION.md](WEB4_INTEGRATION.md) for how Web4 concepts map to embedded design.
See [SYNCHRONISM_PRINCIPLES.md](SYNCHRONISM_PRINCIPLES.md) for architectural patterns and philosophy.

### Ecosystem Links

- [Synchronism Whitepaper](https://github.com/dp-web4/Synchronism) - Theoretical framework
- [Web4 Standard](https://github.com/dp-web4/web4) - Trust-native architecture concepts
- [ModBatt Web4 Examples](https://github.com/dp-web4/web4/blob/main/docs/modbatt_implementation_examples.md) - Implementation walkthroughs

**This demonstrates how concepts from distributed intelligence theory can inform practical embedded system design.**

---

## Key Features

### 🔋 Cell Management
- **Capacity**: Manages up to 128 CellCPUs in daisy-chain topology
- **Communication**: Virtual UART at 20kbps for cell polling
- **Monitoring**: Aggregates temperature, voltage, and status from all cells
- **Coordination**: Enables balancing, tracks cell health, detects faults

### 📡 Pack Communication
- **Protocol**: CAN 2.0B extended frames with module ID addressing
- **Registration**: Dynamic ID assignment (1-31) from Pack Controller
- **Reporting**: Module-level status, aggregated cell data, error conditions
- **Commands**: State changes, balancing control, time synchronization

### ⚙️ Core Capabilities
- **Dual-Phase Operation**: Separate read/write phases for data integrity
- **RTC Support**: Real-time clock for timestamping
- **Error Handling**: Robust timeout and recovery mechanisms
- **State Management**: OFF/STANDBY/ACTIVE/BALANCE states with safety checks

### 🎯 Performance
- **Memory**: 64KB Flash, 4KB SRAM
- **Response Time**: <50ms for full cell poll cycle (94 cells)
- **CAN Reliability**: Timeout recovery, bus-off handling, error counting
- **Cell Tracking**: Bitmap and statistics for each cell position

## Protocol Synchronization

**Important**: ModuleCPU uses shared CAN protocol definitions maintained in the Pack-Controller-EEPROM repository.

**Authoritative Source**: [Pack-Controller-EEPROM/protocols/](https://github.com/dp-web4/Pack-Controller-EEPROM/tree/main/protocols)
**Local Copy**: `protocols/` (synchronized via `scripts/sync_protocols.sh`)

To update protocols after changes in Pack-Controller:
```bash
./scripts/sync_protocols.sh
# Review changes, test build, commit
```

See `protocols/SYNC_NOTICE.md` for synchronization procedure and versioning.

## System Integration

### ModBatt Architecture

```
Vehicle Control Unit (VCU)
    ↓ CAN Bus
Pack Controller (STM32WB55)
    ↓ CAN Bus (250kbps, extended frames)
ModuleCPU (ATmega64M1) ←─→ 128 × CellCPU (ATtiny45)
    ↓ Virtual UART (20kbps)     ↓
Battery Module             Li-ion Cells
```

### Information Flow

**Upward (Aggregation)**:
```
94 cells × (temp + voltage) = 188 data points
    ↓ (Module aggregates)
Module summary: avg/min/max temps, voltage spread, status bitmap
    ↓ (via CAN)
Pack Controller receives compressed module-level view
```

**Downward (Commands)**:
```
Pack: "Modules enter BALANCE state" (broadcast or targeted)
    ↓ (Module interprets)
Module: Polls cells, coordinates balancing based on voltages
    ↓ (Cells execute)
Cells: Activate balancing FETs based on local decisions
```

## Build Environment

**Important**: This project runs in WSL but uses Windows-based build tools.

- **IDE**: Atmel Studio 7.0+ (Windows)
- **Build**: Must compile in Windows environment
- **WSL**: Used for version control and documentation only
- **Focus**: Code correctness and architecture

See [CLAUDE.md](CLAUDE.md) for development context and recent improvements.

## Recent Enhancements

### Protocol Dependency Resolution (October 2025)
- ✅ Added local `protocols/` directory with sync script
- ✅ Resolved Pack-Controller build dependency
- ✅ ModuleCPU now builds standalone

### CAN Reliability Improvements (September 2025)
- ✅ TX timeout mechanism with backup polling
- ✅ Bus-off recovery with health monitoring
- ✅ Adaptive backoff for persistent errors
- ✅ Diagnostic counters for error tracking

### VUART Edge Detection Fix (September 2025)
- ✅ Rising edge detection for level-shifted signals
- ✅ Improved temperature data reliability
- ✅ Better daisy-chain communication

See [CLAUDE.md](CLAUDE.md) for complete development history and technical details.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development workflow, coding standards, and pull request guidelines.

## License and Patents

### Software License

This software is licensed under the **GNU Affero General Public License v3.0 or later** (AGPL-3.0-or-later).

See [LICENSE](LICENSE) for full license text.
See [LICENSE_SUMMARY.md](LICENSE_SUMMARY.md) for software vs. hardware licensing details.

### Patents

Protected by US Patents:
- **11,469,470**: Hierarchical battery management architecture
- **11,380,942**: Distributed cell controller system
- **11,575,270**: Related pack control systems

**Patent Grant**: Limited patent grant for software use under AGPL-3.0. See [PATENTS.md](PATENTS.md).

**Hardware Licensing**: Separate from software. See `docs/legal/ModBatt_Product_License.pdf`.

### Copyright

© 2023-2025 Modular Battery Technologies, Inc.

**Open Source Release**: October 2025 - Part of the Web4 trust-native ecosystem demonstration.

---

## Related Projects

This repository can be viewed as demonstrating patterns of distributed trust and verifiable provenance in embedded systems.

**Related**: [Web4](https://github.com/dp-web4/web4) (trust architecture) • [Synchronism](https://github.com/dp-web4/Synchronism) (conceptual framing)

---

**Note**: This firmware demonstrates hierarchical coordination patterns in embedded systems. Whether viewed through the lens of Web4/Synchronism concepts or simply as sound battery management architecture, the design patterns are production-tested and functional.
