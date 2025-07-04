# Development Environment Setup

## Table of Contents

1. [Overview](#overview)
2. [Required Tools](#required-tools)
3. [Microchip Studio Setup](#microchip-studio-setup)
4. [Project Configuration](#project-configuration)
5. [Build System](#build-system)
6. [Debugging Setup](#debugging-setup)
7. [Testing and Validation](#testing-and-validation)
8. [Code Quality Tools](#code-quality-tools)

## Overview

The ModuleCPU firmware is developed using Microchip Studio (formerly Atmel Studio) with the XC8 compiler toolchain. This document provides comprehensive setup instructions for establishing a complete development environment for the ATmega64M1-based module controller firmware.

### Development Stack
```
Development Environment Stack:
├── IDE: Microchip Studio 7.0
├── Compiler: Microchip XC8 v2.36
├── Debugger: Atmel-ICE or similar
├── Target: ATmega64M1 microcontroller
├── Build System: MSBuild (Visual Studio compatible)
├── Version Control: Git
└── Documentation: Markdown + diagrams
```

## Required Tools

### Essential Software

#### 1. Microchip Studio 7.0
- **Purpose**: Primary IDE for AVR development
- **Download**: [Microchip Studio](https://www.microchip.com/mplab/microchip-studio)
- **Version**: 7.0.2594 or later
- **Features**: Integrated debugger, project management, code editor

#### 2. Microchip XC8 Compiler
- **Purpose**: C compiler for AVR microcontrollers
- **Version**: v2.36 or compatible
- **License**: Free version available (optimization restrictions)
- **Installation**: Integrated with Microchip Studio

#### 3. Git for Windows
- **Purpose**: Version control system
- **Download**: [Git SCM](https://git-scm.com/download/win)
- **Configuration**: Required for source code management

### Hardware Tools

#### 1. Programming/Debugging Interface
```
Recommended Debuggers:
├── Atmel-ICE (Primary recommendation)
│   ├── Interfaces: SWD, JTAG, debugWIRE, PDI, TPI
│   ├── Voltage Range: 1.8V to 5.5V
│   ├── Current Capability: 300mA
│   └── USB Interface: High-speed USB 2.0
├── AVR Dragon (Alternative)
│   ├── Interfaces: JTAG, debugWIRE, ISP, HVPP
│   ├── Voltage Range: 1.8V to 5.5V
│   └── USB Interface: Full-speed USB
└── AVRISP mkII (ISP only)
    ├── Interface: ISP (In-System Programming)
    ├── Voltage Range: 1.8V to 5.5V
    └── USB Interface: Full-speed USB
```

#### 2. Development Hardware
- **Target Board**: ModuleCPU hardware or equivalent
- **Power Supply**: 5V regulated supply
- **CAN Interface**: CAN transceiver for network testing
- **SD Card**: For data logging testing
- **Oscilloscope**: For signal analysis (optional but recommended)

## Microchip Studio Setup

### Installation Process

1. **Download Microchip Studio**
   ```
   1. Visit: https://www.microchip.com/mplab/microchip-studio
   2. Download: Microchip Studio 7.0 Installer
   3. Size: ~800MB download
   4. Requirements: Windows 7 SP1 or later
   ```

2. **Installation Steps**
   ```
   1. Run installer as Administrator
   2. Select "Complete" installation
   3. Accept license agreements
   4. Install to default location: C:\Program Files (x86)\Atmel\Studio\7.0\
   5. Allow Windows Defender/Firewall exceptions
   6. Restart computer when prompted
   ```

3. **Initial Configuration**
   ```
   1. Launch Microchip Studio
   2. Skip online activation (optional)
   3. Configure XC8 compiler path if not auto-detected
   4. Install AVR Device Packs if prompted
   5. Configure debugger drivers
   ```

### XC8 Compiler Configuration

```c
// Project compiler settings (Release configuration)
Compiler Flags:
├── Optimization Level: -Os (Optimize for size)
├── C Standard: C99
├── Character Type: Unsigned char
├── Bit Field Type: Unsigned
├── Warning Level: All warnings (-Wall)
├── Additional Flags: -fpack-struct, -fshort-enums
└── Debug Information: Minimal (-g1 for release)

// Linker settings
Linker Flags:
├── Libraries: libm (math library)
├── Memory Model: Default
├── Stack Size: Default (handled by hardware)
└── Optimization: Remove unused sections
```

### Debug Configuration

```c
// Debug build settings
Debug Configuration:
├── Optimization Level: -Og (Optimize for debugging)
├── Debug Information: Full (-g3)
├── Preprocessor: DEBUG=1
├── Runtime Checks: Enabled
├── Stack Protection: Enabled (if available)
└── Assertions: Enabled
```

## Project Configuration

### Project Structure Setup

```
Project Organization:
ModuleCPU/
├── Source Files/
│   ├── main.c                  # Main application
│   ├── adc.c                   # ADC driver
│   ├── can.c                   # CAN communication
│   ├── vUART.c                 # Virtual UART
│   ├── STORE.c                 # Storage management
│   ├── EEPROM.c                # EEPROM functions
│   ├── SD.c                    # SD card interface
│   ├── SPI.c                   # SPI driver
│   ├── I2c.c                   # I2C driver
│   ├── rtc_mcp7940n.c         # RTC driver
│   └── debugSerial.c           # Debug interface
├── Header Files/
│   ├── main.h                  # Main definitions
│   ├── adc.h                   # ADC interface
│   ├── can.h                   # CAN interface
│   ├── can_ids.h               # CAN message IDs
│   ├── vUART.h                 # Virtual UART interface
│   ├── STORE.h                 # Storage interface
│   ├── EEPROM.h                # EEPROM interface
│   ├── SD.h                    # SD card interface
│   ├── SPI.h                   # SPI interface
│   ├── I2c.h                   # I2C interface
│   ├── rtc_mcp7940n.h         # RTC interface
│   └── debugSerial.h           # Debug interface
├── Shared/
│   └── Shared.h                # Common definitions
├── Debug/                      # Debug build output
├── Release/                    # Release build output
└── ModuleCPU.cproj            # Project file
```

### Project File Configuration

The `ModuleCPU.cproj` file contains essential project settings:

```xml
<!-- Key project properties -->
<PropertyGroup>
  <SchemaVersion>2.0</SchemaVersion>
  <ProjectVersion>7.0</ProjectVersion>
  <ToolchainName>com.microchip.xc8</ToolchainName>
  <ProjectGuid>{2285C48D-296E-43FD-A7B6-69885F64CFFD}</ProjectGuid>
  <avrdevice>ATmega64M1</avrdevice>
  <OutputType>Executable</OutputType>
  <Language>C</Language>
  <ToolchainFlavour>XC8_2.36</ToolchainFlavour>
  <KeepTimersRunning>true</KeepTimersRunning>
  <preserveEEPROM>true</preserveEEPROM>
</PropertyGroup>

<!-- Debug configuration -->
<PropertyGroup Condition=" '$(Configuration)' == 'Debug' ">
  <ToolchainSettings>
    <com.microchip.xc8>
      <com.microchip.xc8.compiler.optimization.level>Optimize debugging experience (-Og)</com.microchip.xc8.compiler.optimization.level>
      <com.microchip.xc8.compiler.optimization.DebugLevel>Default (-g2)</com.microchip.xc8.compiler.optimization.DebugLevel>
      <com.microchip.xc8.compiler.warnings.AllWarnings>True</com.microchip.xc8.compiler.warnings.AllWarnings>
    </com.microchip.xc8>
  </ToolchainSettings>
</PropertyGroup>

<!-- Release configuration -->
<PropertyGroup Condition=" '$(Configuration)' == 'Release' ">
  <ToolchainSettings>
    <com.microchip.xc8>
      <com.microchip.xc8.compiler.optimization.level>Optimize for size (-Os)</com.microchip.xc8.compiler.optimization.level>
      <com.microchip.xc8.compiler.optimization.PackStructureMembers>True</com.microchip.xc8.compiler.optimization.PackStructureMembers>
      <com.microchip.xc8.compiler.optimization.AllocateBytesNeededForEnum>True</com.microchip.xc8.compiler.optimization.AllocateBytesNeededForEnum>
    </com.microchip.xc8>
  </ToolchainSettings>
</PropertyGroup>
```

### Include Paths and Dependencies

```c
// Include path configuration
Include Directories:
├── Project Root (./)
├── Shared Directory (../Shared)
├── XC8 System Includes (automatic)
└── AVR Libc Includes (automatic)

// Library Dependencies
Libraries:
├── libm (math library)
├── avr-libc (standard C library for AVR)
└── XC8 Runtime Libraries (automatic)

// Preprocessor Definitions
Debug Build:
├── DEBUG=1
├── F_CPU=8000000UL
└── __AVR_ATmega64M1__

Release Build:
├── NDEBUG=1
├── F_CPU=8000000UL
└── __AVR_ATmega64M1__
```

## Build System

### Build Process Overview

```
Build Process Flow:
├── Preprocessing
│   ├── Macro expansion
│   ├── Include file processing
│   ├── Conditional compilation
│   └── Comment removal
├── Compilation
│   ├── C to assembly translation
│   ├── Optimization passes
│   ├── Warning generation
│   └── Object file creation
├── Assembly
│   ├── Assembly to machine code
│   ├── Symbol table generation
│   └── Relocation information
├── Linking
│   ├── Object file combination
│   ├── Library linking
│   ├── Address resolution
│   └── Executable generation
└── Output Generation
    ├── .elf file (executable)
    ├── .hex file (Intel HEX)
    ├── .lss file (listing)
    └── .map file (memory map)
```

### Memory Configuration

```c
// ATmega64M1 memory layout
Memory Configuration:
├── Flash Memory (64KB)
│   ├── Bootloader Section: 0x0000-0x0FFF (4KB)
│   ├── Application Section: 0x1000-0xFFFF (60KB)
│   └── Fuse/Lock Bits: Separate area
├── SRAM (4KB)
│   ├── Register File: 0x0000-0x001F (32 bytes)
│   ├── I/O Space: 0x0020-0x005F (64 bytes)
│   ├── Extended I/O: 0x0060-0x00FF (160 bytes)
│   └── Internal SRAM: 0x0100-0x10FF (4096 bytes)
├── EEPROM (2KB)
│   ├── Address Range: 0x0000-0x07FF
│   ├── Page Size: 8 bytes
│   └── Endurance: 100,000 cycles
└── Fuses
    ├── Low Fuse: Clock configuration
    ├── High Fuse: Boot loader and debug
    └── Extended Fuse: BOD and startup
```

### Build Commands

```bash
# Manual build commands (if needed)

# Compile individual source file
xc8-cc -mcpu=ATmega64M1 -c -fshort-enums -fpack-struct -O2 -Wall -o main.o main.c

# Link object files
xc8-cc -mcpu=ATmega64M1 -Wl,-Map=ModuleCPU.map -o ModuleCPU.elf main.o adc.o can.o ... -lm

# Generate Intel HEX file
avr-objcopy -O ihex -R .eeprom ModuleCPU.elf ModuleCPU.hex

# Generate EEPROM HEX file
avr-objcopy -j .eeprom --set-section-flags=.eeprom=alloc,load --change-section-lma .eeprom=0 -O ihex ModuleCPU.elf ModuleCPU.eep

# Generate listing file
avr-objdump -h -S ModuleCPU.elf > ModuleCPU.lss
```

## Debugging Setup

### Hardware Configuration

```
Debugger Setup (Atmel-ICE):
├── Physical Connections
│   ├── USB: Computer to Atmel-ICE
│   ├── Target Cable: 6-pin or 10-pin ISP
│   ├── Power: From target board (5V)
│   └── debugWIRE: Single-wire debug interface
├── Interface Configuration
│   ├── Interface: debugWIRE
│   ├── Clock Speed: 125kHz (for debugging)
│   ├── Target Voltage: Auto-detect
│   └── Programming Mode: ISP for initial programming
└── Software Configuration
    ├── Tool: Atmel-ICE in Microchip Studio
    ├── Device: ATmega64M1
    ├── Interface: debugWIRE
    └── Clock: 125kHz
```

### Debug Configuration Steps

1. **Hardware Setup**
   ```
   1. Connect Atmel-ICE to computer via USB
   2. Connect ISP cable to target board
   3. Ensure target board is powered (5V)
   4. Verify connection in Device Manager
   ```

2. **Software Configuration**
   ```
   1. Open ModuleCPU project in Microchip Studio
   2. Go to Project > ModuleCPU Properties
   3. Select "Tool" tab
   4. Choose "Atmel-ICE" from dropdown
   5. Select "debugWIRE" interface
   6. Set clock to 125kHz
   7. Click "Apply"
   ```

3. **Enable debugWIRE**
   ```
   1. In Microchip Studio, go to Tools > Device Programming
   2. Select Tool: Atmel-ICE, Device: ATmega64M1, Interface: ISP
   3. Click "Read" to read device signature
   4. Go to "Fuses" tab
   5. Set DWEN fuse (enable debugWIRE)
   6. Click "Program"
   7. Switch interface to debugWIRE in project properties
   ```

### Debugging Features

```c
// Debug capabilities
Available Debug Features:
├── Breakpoints
│   ├── Code breakpoints (unlimited software)
│   ├── Data breakpoints (limited hardware)
│   ├── Conditional breakpoints
│   └── Temporary breakpoints
├── Execution Control
│   ├── Step Into (F11)
│   ├── Step Over (F10)
│   ├── Step Out (Shift+F11)
│   ├── Run to Cursor (Ctrl+F10)
│   └── Continue (F5)
├── Variable Inspection
│   ├── Local variables window
│   ├── Watch window
│   ├── Memory window
│   └── Register window
├── Advanced Features
│   ├── Call stack inspection
│   ├── Memory access tracing
│   ├── Peripheral register viewing
│   └── Real-time variable updates
└── Limitations
    ├── debugWIRE affects timer accuracy
    ├── Some interrupts may be affected
    ├── RESET pin unavailable during debug
    └── Speed limited compared to free-running
```

## Testing and Validation

### Unit Testing Framework

```c
// Simple unit testing macros
#ifdef DEBUG
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            debugPrint("ASSERT FAILED: %s:%d - %s\n", __FILE__, __LINE__, #condition); \
            while(1); /* Halt for debugging */ \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            debugPrint("ASSERT FAILED: %s:%d - Expected %d, got %d\n", \
                      __FILE__, __LINE__, (expected), (actual)); \
            while(1); \
        } \
    } while(0)

// Test execution macro
#define RUN_TEST(test_func) \
    do { \
        debugPrint("Running test: %s\n", #test_func); \
        test_func(); \
        debugPrint("Test passed: %s\n", #test_func); \
    } while(0)
#else
#define TEST_ASSERT(condition)
#define TEST_ASSERT_EQUAL(expected, actual)
#define RUN_TEST(test_func)
#endif

// Example test functions
void test_ADC_Conversion(void) {
    // Test ADC conversion accuracy
    uint16_t testValue = 512; // Mid-scale
    uint16_t converted = convertADCToMillivolts(testValue);
    TEST_ASSERT(converted > 2000 && converted < 3000); // ~2.5V expected
}

void test_CAN_MessageValidation(void) {
    // Test CAN message validation
    ModuleStatusMessage msg;
    msg.cellCount = 94;
    msg.moduleState = EMODSTATE_ON;
    TEST_ASSERT(validateStatusMessage(&msg) == true);
    
    msg.cellCount = 200; // Invalid
    TEST_ASSERT(validateStatusMessage(&msg) == false);
}

// Test suite execution
void runAllTests(void) {
    #ifdef DEBUG
    debugPrint("Starting test suite...\n");
    RUN_TEST(test_ADC_Conversion);
    RUN_TEST(test_CAN_MessageValidation);
    // Add more tests...
    debugPrint("All tests completed successfully!\n");
    #endif
}
```

### Integration Testing

```c
// Integration test scenarios
Integration Test Categories:
├── Communication Tests
│   ├── CAN bus communication with pack controller
│   ├── Cell CPU communication via vUART
│   ├── SD card read/write operations
│   └── EEPROM configuration storage
├── Monitoring Tests
│   ├── Cell voltage measurement accuracy
│   ├── Temperature reading validation
│   ├── Current measurement precision
│   └── Balancing control functionality
├── Safety Tests
│   ├── Overvoltage protection response
│   ├── Undervoltage protection response
│   ├── Overtemperature protection
│   └── Communication timeout handling
├── Performance Tests
│   ├── Real-time constraint validation
│   ├── Memory usage analysis
│   ├── Power consumption measurement
│   └── Deterministic timing verification
└── Stress Tests
    ├── Extended operation testing
    ├── Error recovery scenarios
    ├── Thermal cycling
    └── EMI/EMC compliance
```

## Code Quality Tools

### Static Analysis

```bash
# Code analysis tools and commands

# PC-lint Plus (if available)
pclint-plus.exe +v -width(120,4) -i"C:\XC8\include" *.c *.h

# Cppcheck (free alternative)
cppcheck --enable=all --platform=avr8 --std=c99 *.c

# Custom analysis scripts
python code_metrics.py --complexity --coverage --style
```

### Code Formatting

```c
// Coding standards
Code Style Guidelines:
├── Indentation: 4 spaces (no tabs)
├── Line Length: 120 characters maximum
├── Naming Convention:
│   ├── Variables: camelCase (localVariable)
│   ├── Functions: camelCase (functionName)
│   ├── Constants: UPPER_CASE (MAX_CELLS)
│   ├── Types: PascalCase (CellData)
│   └── Globals: g_ prefix (g_cellData)
├── Comments:
│   ├── Function headers with purpose, parameters, returns
│   ├── Complex algorithms explained
│   ├── TODO/FIXME markers for future work
│   └── Doxygen-style documentation
└── File Organization:
    ├── License header
    ├── Include statements
    ├── Constant definitions
    ├── Type definitions
    ├── Global variables
    ├── Function prototypes
    └── Function implementations
```

### Documentation Standards

```c
/**
 * @brief Converts ADC reading to millivolts
 * 
 * This function converts a raw 10-bit ADC reading to millivolts using
 * the configured voltage reference and calibration factors.
 * 
 * @param adcReading Raw ADC value (0-1023)
 * @return Voltage in millivolts (0-5000mV)
 * 
 * @note Uses fixed-point arithmetic for efficiency
 * @see VOLTAGE_CONVERSION_FACTOR for calibration details
 */
uint16_t convertADCToMillivolts(uint16_t adcReading);
```

This development environment setup provides a complete foundation for ModuleCPU firmware development with proper tooling, debugging capabilities, and quality assurance measures.