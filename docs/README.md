# ModuleCPU Documentation

## 📋 Table of Contents

- [Overview](#overview)
- [Quick Start](#quick-start)
- [Documentation Structure](#documentation-structure)
- [Key Concepts](#key-concepts)
- [Getting Help](#getting-help)

## Overview

Welcome to the comprehensive documentation for the ModuleCPU firmware - the heart of ModBatt's battery module controllers. This documentation provides detailed technical information for developers, engineers, and system integrators working with ModBatt's hierarchical battery management system.

### What is ModuleCPU?

ModuleCPU is the firmware that runs on ATmega64M1 microcontrollers within individual battery modules. Each ModuleCPU manages up to 94 lithium-ion cells, providing:

- **Real-time Cell Monitoring**: Voltage, temperature, and balancing status for each cell
- **Active Cell Balancing**: FET-based discharge balancing to maintain cell uniformity  
- **Pack Controller Communication**: CAN-based interface with the pack-level controller
- **Data Logging**: Comprehensive storage of historical cell and module data
- **Safety Protection**: Multi-layer fault detection and protective responses

### System Context

```
ModBatt Hierarchical Architecture:
┌─────────────────────────────────────────────────────────────────┐
│                Vehicle Control Unit (VCU)                       │
└─────────────────────────┬───────────────────────────────────────┘
                          │ CAN Bus
┌─────────────────────────┼───────────────────────────────────────┐
│                  Pack Controller                                │
│             (STM32WB55-based firmware)                         │
└─────────────────────────┬───────────────────────────────────────┘
                          │ CAN Bus
        ┌─────────────────┼─────────────────┐
        │                 │                 │
┌───────▼───────┐ ┌───────▼───────┐ ┌───────▼───────┐
│ Module CPU #1 │ │ Module CPU #2 │ │ Module CPU #N │
│ (This Firmware│ │               │ │  (up to 32)   │
│  ATmega64M1)  │ │               │ │               │
└───────┬───────┘ └───────┬───────┘ └───────┬───────┘
        │                 │                 │
   ┌────▼────┐       ┌────▼────┐       ┌────▼────┐
   │94 Cells │       │94 Cells │       │94 Cells │
   │Max Each │       │Max Each │       │Max Each │
   └─────────┘       └─────────┘       └─────────┘
```

## Quick Start

### For Developers
1. **[Development Setup](development/README.md)** - Set up Microchip Studio and build environment
2. **[Firmware Architecture](firmware/README.md)** - Understand the overall system design
3. **[Hardware Platform](hardware/README.md)** - Learn about the ATmega64M1 platform

### For System Integrators  
1. **[System Overview](overview/README.md)** - High-level system architecture
2. **[CAN Communication](firmware/can-communication.md)** - Interface with pack controllers
3. **[Cell Monitoring](firmware/cell-monitoring.md)** - Understanding cell data and balancing

### For Troubleshooting
1. **[Development Guide](development/README.md)** - Debugging and testing procedures
2. **[Hardware Reference](hardware/README.md)** - Pin assignments and peripheral configuration
3. **[Storage System](firmware/storage-logging.md)** - Data logging and configuration

## Documentation Structure

### 📁 [Overview](overview/)
High-level system information and architectural concepts
- **[README.md](overview/README.md)** - System overview and key features
- **[architecture.md](overview/architecture.md)** - Software architecture and design patterns

### 🔧 [Hardware](hardware/)  
Detailed ATmega64M1 platform documentation
- **[README.md](hardware/README.md)** - Complete hardware platform guide
- Microcontroller specifications, peripherals, and pin configurations
- External component integration (RTC, SD card, CAN transceivers)

### 💾 [Firmware](firmware/)
Comprehensive firmware implementation details
- **[README.md](firmware/README.md)** - Firmware architecture and core modules
- **[cell-monitoring.md](firmware/cell-monitoring.md)** - Cell monitoring and balancing systems
- **[can-communication.md](firmware/can-communication.md)** - CAN protocol implementation  
- **[storage-logging.md](firmware/storage-logging.md)** - Data storage and logging systems

### 🛠️ [Development](development/)
Development environment and tooling information
- **[README.md](development/README.md)** - Complete development setup guide
- Microchip Studio configuration, debugging, and testing procedures

## Key Concepts

### Core Technologies
- **[ATmega64M1 Microcontroller](hardware/README.md#atmega64m1-specifications)**: 8-bit AVR with integrated CAN controller
- **[Dual-Phase Operation](firmware/README.md#dual-phase-operation)**: Alternating read/write cycles for data integrity
- **[Virtual UART](firmware/cell-monitoring.md#virtual-uart-protocol)**: Bit-banged communication with cell controllers
- **[Session-Based Logging](firmware/storage-logging.md#session-management)**: Organized data storage with metadata

### Key Features
- **[Real-Time Monitoring](firmware/cell-monitoring.md)**: 300ms update cycle for up to 94 cells
- **[Active Balancing](firmware/cell-monitoring.md#balancing-control)**: Configurable discharge balancing
- **[Safety Systems](firmware/README.md#safety-system)**: Multi-layer protection and fault detection
- **[Data Logging](firmware/storage-logging.md)**: SD card storage with RTC timestamping

### Communication Protocols
- **[CAN 2.0B](firmware/can-communication.md)**: 250kbps communication with pack controller
- **[ModBatt Protocol](firmware/can-communication.md#message-definitions)**: Custom message format for battery data
- **[Cell Communication](firmware/cell-monitoring.md#cell-communication)**: 20kbps virtual UART for cell controllers

## Getting Help

### Documentation Navigation Tips
- Use the **Table of Contents** in each document for quick navigation
- Look for **code examples** and **configuration snippets** throughout
- Check **cross-references** between related topics
- Review **performance characteristics** sections for system requirements

### Technical Support
- **Hardware Issues**: Refer to [Hardware Platform Guide](hardware/README.md)
- **Firmware Development**: See [Development Environment Setup](development/README.md)
- **Communication Problems**: Check [CAN Communication Guide](firmware/can-communication.md)
- **Data Logging Issues**: Review [Storage and Logging Documentation](firmware/storage-logging.md)

### Related Documentation
- **Pack Controller Documentation**: `/Pack-Controller-EEPROM/docs/`
- **Web4 ModBatt Demo**: `/web4-modbatt-demo/docs/`
- **Shared Definitions**: See `../Shared/Shared.h` for common interfaces

---

**ModuleCPU Firmware** - Part of the ModBatt modular battery management system  
© 2023-2024 Modular Battery Technologies, Inc.