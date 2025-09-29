# EEPROM Editor for ModuleCPU

A Windows GUI application for viewing and editing ModuleCPU EEPROM (.eep) files.

## Features

- **Visual EEPROM Field Editor**: Edit metadata fields with proper validation
  - Module Unique ID (32-bit hex)
  - Expected Cell Count (0-255)
  - Max Charge Current (amps)
  - Max Discharge Current (amps)
  - Sequential Count Mismatch tracking

- **Frame Counter Buffer Viewer**:
  - Visualize all 128 frame counter positions
  - Automatically identifies current position (highest value)
  - Shows wear leveling distribution
  - Displays counter values in both decimal and hex

- **Hex View**:
  - Complete 2KB EEPROM hex dump
  - Color-coded sections (metadata, frame counter, reserved)

- **File Operations**:
  - Open existing .eep files (Intel HEX format)
  - Save modified EEPROM data
  - Create new blank EEPROM

## Building

Requirements:
- .NET 6.0 SDK or later
- Windows OS

To build:
```bash
build.bat
```

Or using dotnet CLI:
```bash
dotnet build -c Release
```

## Usage

1. Run `EEPROMEditor.exe`
2. Use File menu to open an existing .eep file or create new
3. Edit fields in the Metadata Fields tab
4. View frame counter distribution in Frame Counter Buffer tab
5. Click "Apply Changes" to update the EEPROM data
6. Save the modified file using File > Save

## EEPROM Layout

Based on ModuleCPU EEPROM mapping:

- **0x0000-0x003F** (64 bytes): Metadata region
  - 0x0000: Unique ID (4 bytes)
  - 0x0004: Expected Cell Count (1 byte)
  - 0x0005: Max Charge Current (2 bytes)
  - 0x0007: Max Discharge Current (2 bytes)
  - 0x0009: Sequential Count Mismatch (2 bytes)
  - 0x000B-0x003F: Reserved

- **0x0040-0x023F** (512 bytes): Frame Counter Buffer
  - 128 positions × 4 bytes each
  - Wear leveling with rotation every 256 increments
  - ~25.9 years endurance at 2Hz operation

- **0x0240-0x07FF** (1472 bytes): Reserved for future use

## Frame Counter Wear Leveling

The frame counter uses a circular buffer approach:
- 128 positions rotate to distribute wear
- Updates every 256 frame increments
- Invalid markers (0xFFFFFFFF) indicate unused positions
- Current position identified by highest valid counter value

## File Format

Uses Intel HEX format (.eep):
- Text-based format
- Each line contains address, data, and checksum
- Compatible with AVR programming tools
- Human-readable when needed