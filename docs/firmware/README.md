# ModuleCPU Firmware Architecture

## Table of Contents

1. [Overview](#overview)
2. [System Architecture](#system-architecture)
3. [Core Modules](#core-modules)
4. [Control Flow](#control-flow)
5. [Communication Architecture](#communication-architecture)
6. [Data Structures](#data-structures)
7. [State Management](#state-management)
8. [Task Scheduling](#task-scheduling)

## Overview

The ModuleCPU firmware implements a comprehensive battery module controller that manages up to 94 individual cells within a single battery module. It operates on an ATmega64M1 microcontroller and provides real-time monitoring, balancing, and communication capabilities.

### Key Characteristics
- **Deterministic Operation**: Fixed 300ms control cycle with predictable execution
- **Dual-Phase Architecture**: Alternating read and write phases for data integrity
- **Event-Driven Design**: Timer-based interrupts drive all major operations
- **Safety-First Approach**: Multiple layers of protection and fault detection
- **Low Power Design**: Optimized for minimal standby current consumption

### Firmware Specifications
```
Build Information:
├── Firmware Build Number: 8278
├── Manufacture ID: 0x02
├── Part ID: 0x03
├── Hardware Compatibility: 0x0000
├── Target Platform: ATmega64M1
├── Compiler: Microchip XC8 v2.36
├── C Standard: C99
└── Optimization: -Os (size), -Og (debug)
```

## System Architecture

### Layered Design

```
┌─────────────────────────────────────────────────────────────────┐
│                     Application Layer                           │
│  ┌─────────────┬─────────────┬─────────────┬─────────────────┐  │
│  │   Main      │   Cell      │   Balance   │   Safety        │  │
│  │ Controller  │ Monitoring  │ Management  │   System        │  │
│  └─────────────┴─────────────┴─────────────┴─────────────────┘  │
└─────────────────────────┬───────────────────────────────────────┘
                          │ Application APIs
┌─────────────────────────┼───────────────────────────────────────┐
│                     Middleware Layer                            │
│  ┌─────────────┬─────────────┬─────────────┬─────────────────┐  │
│  │   Virtual   │   Storage   │    CAN      │     Debug       │  │
│  │    UART     │  Manager    │  Protocol   │   Services      │  │
│  └─────────────┴─────────────┴─────────────┴─────────────────┘  │
└─────────────────────────┬───────────────────────────────────────┘
                          │ Driver APIs
┌─────────────────────────┼───────────────────────────────────────┐
│                Hardware Abstraction Layer                       │
│  ┌─────────────┬─────────────┬─────────────┬─────────────────┐  │
│  │     ADC     │     CAN     │     SPI     │   I2C/Timer     │  │
│  │   Driver    │   Driver    │   Driver    │    Drivers      │  │
│  └─────────────┴─────────────┴─────────────┴─────────────────┘  │
└─────────────────────────┬───────────────────────────────────────┘
                          │ Register Access
┌─────────────────────────┼───────────────────────────────────────┐
│                     Hardware Layer                              │
│  ATmega64M1 + MCP7940N RTC + SD Card + External Components     │
└─────────────────────────────────────────────────────────────────┘
```

### File Organization

```
ModuleCPU/
├── main.c                      # Main control loop and initialization
├── main.h                      # System definitions and constants
├── adc.c/.h                    # ADC driver for cell monitoring
├── can.c/.h                    # CAN communication driver
├── can_ids.h                   # CAN message ID definitions
├── vUART.c/.h                  # Virtual UART implementation
├── debugSerial.c/.h            # Debug communication interface
├── STORE.c/.h                  # Data storage management
├── EEPROM.c/.h                 # EEPROM configuration storage
├── SD.c/.h                     # SD card interface
├── SPI.c/.h                    # SPI bus driver
├── I2c.c/.h                    # I2C bus driver
├── rtc_mcp7940n.c/.h          # Real-time clock driver
└── ../Shared/Shared.h          # Common definitions
```

## Core Modules

### 1. Main Controller (main.c)

**Central system orchestrator and periodic task manager**

#### System Configuration:
```c
// System timing configuration
#define CPU_SPEED                    8000000    // 8MHz internal oscillator
#define PERIODIC_INTERRUPT_RATE_MS   100       // 100ms base timing
#define PERIODIC_CALLBACK_RATE_MS    300       // 300ms control cycle
#define PACK_CONTROLLER_TIMEOUT_MS   11100     // 11.1s timeout

// Maximum cell capacity
#define TOTAL_CELL_COUNT_MAX         94        // Current modules support 94 cells

// Frame types for dual-phase operation
typedef enum {
    EFRAMETYPE_READ,                           // Data acquisition phase
    EFRAMETYPE_WRITE,                          // Data reporting/storage phase
} EFrameType;
```

#### State Management:
```c
// Module operating states
typedef enum {
    EMODSTATE_OFF = 0,                         // All systems off
    EMODSTATE_STANDBY,                         // Ready, monitoring only
    EMODSTATE_PRECHARGE,                       // Preparing for operation
    EMODSTATE_ON,                              // Full operation with balancing
    
    EMODSTATE_COUNT,
    EMODSTATE_INIT,                            // Initial assessment state
} EModuleControllerState;

// Current operating state
EModuleControllerState g_eCurrentState = EMODSTATE_OFF;
```

#### Main Control Loop:
```c
int main(void) {
    // Hardware initialization
    systemInit();
    peripheralInit();
    
    // Initialize all subsystems
    FrameInit(true);                           // Full system initialization
    
    // Enable global interrupts
    sei();
    
    // Main control loop
    while (1) {
        // Service watchdog timer
        WatchdogReset();
        
        // Process background tasks
        if (g_bServiceNeeded) {
            // Clear service flag
            g_bServiceNeeded = false;
            
            // Execute frame-specific operations
            processPeriodicTasks();
            
            // Handle communication timeouts
            checkTimeouts();
            
            // Update system status
            updateSystemStatus();
        }
        
        // Enter sleep mode to save power
        enterSleepMode();
    }
}
```

### 2. ADC Manager (adc.c)

**Cell voltage and temperature monitoring system**

#### ADC Configuration:
```c
// ADC reference and scaling
#define CELL_VREF                    1.1       // 1.1V internal reference
#define CELL_VOLTAGE_BITS            10        // 10-bit ADC resolution
#define FIXED_POINT_SCALE            512       // 9-bit fractional precision

// Voltage divider calibration
#define CELL_DIV_TOP                 90900     // Upper resistor (Ohms)
#define CELL_DIV_BOTTOM              30100     // Lower resistor (Ohms)
#define CELL_VOLTAGE_CAL             1.032     // Measured calibration factor

// Pre-calculated conversion factor
#define VOLTAGE_CONVERSION_FACTOR    ((uint32_t)((CELL_VREF * 1000.0 / \
    CELL_VOLTAGE_SCALE * CELL_VOLTAGE_CAL * FIXED_POINT_SCALE) + 0.5))
```

#### Cell Monitoring Process:
```c
void ADCCallback(EADCType eType, uint16_t u16Reading) {
    static uint8_t cellIndex = 0;
    
    switch (eType) {
        case EADC_CELL_VOLTAGE:
            // Convert raw ADC reading to millivolts
            uint32_t millivolts = (u16Reading * VOLTAGE_CONVERSION_FACTOR) / 
                                 (ADC_MAX_VALUE * FIXED_POINT_SCALE);
            
            // Store cell voltage
            g_cellData[cellIndex].voltage = (uint16_t)millivolts;
            
            // Update statistics
            updateVoltageStatistics(cellIndex, millivolts);
            
            // Move to next cell
            cellIndex = (cellIndex + 1) % g_u8ActiveCellCount;
            break;
            
        case EADC_CELL_TEMPERATURE:
            // Process temperature reading
            processTemperatureReading(cellIndex, u16Reading);
            break;
            
        case EADC_MODULE_CURRENT:
            // Process current measurement
            processCurrentReading(u16Reading);
            break;
    }
}
```

### 3. Virtual UART (vUART.c)

**Bit-banged UART for cell CPU communication**

#### Protocol Specifications:
```c
// Virtual UART timing (50μs per bit = 20kbps)
#define VUART_BIT_TICKS              50        // Timer ticks per bit

// Frame structure: [START][8 DATA BITS][STOP]
// Each cell response: 4 bytes (voltage + temperature + status + checksum)
```

#### Implementation:
```c
// UART state machine
typedef enum {
    VUART_IDLE,
    VUART_START_BIT,
    VUART_DATA_BITS,
    VUART_STOP_BIT
} VUARTState;

void vUARTRXStart(void) {
    // Initialize reception
    g_vUartState = VUART_START_BIT;
    g_vUartBitCount = 0;
    g_vUartRxByte = 0;
    
    // Start bit timing timer
    startBitTimer(VUART_BIT_TICKS);
}

void vUARTRXData(uint8_t u8rxDataByte) {
    // Process received byte
    if (g_vUartState == VUART_DATA_BITS) {
        // Store data bit
        if (readRXPin()) {
            g_vUartRxByte |= (1 << g_vUartBitCount);
        }
        
        g_vUartBitCount++;
        
        // Check if byte complete
        if (g_vUartBitCount >= 8) {
            // Process complete byte
            processCellResponse(g_vUartRxByte);
            g_vUartState = VUART_STOP_BIT;
        }
    }
}
```

### 4. CAN Communication (can.c)

**Interface with pack controller and system**

#### Message Protocol:
```c
// CAN message structure for module communication
typedef struct {
    uint16_t cellCount;                        // Number of active cells
    uint16_t voltageHigh;                      // Highest cell voltage (mV)
    uint16_t voltageLow;                       // Lowest cell voltage (mV)
    uint16_t voltageAverage;                   // Average cell voltage (mV)
    uint16_t temperatureHigh;                  // Highest cell temperature
    uint16_t temperatureLow;                   // Lowest cell temperature
    uint8_t  moduleState;                      // Current operating state
    uint8_t  faultFlags;                       // Active fault conditions
} ModuleStatusMessage;
```

#### Communication Flow:
```c
void CANSendModuleStatus(void) {
    ModuleStatusMessage status;
    
    // Populate status message
    status.cellCount = g_u8ActiveCellCount;
    status.voltageHigh = g_u16CellVoltageHigh;
    status.voltageLow = g_u16CellVoltageLow;
    status.voltageAverage = g_u16CellVoltageAverage;
    status.temperatureHigh = g_u16CellTemperatureHigh;
    status.temperatureLow = g_u16CellTemperatureLow;
    status.moduleState = g_eCurrentState;
    status.faultFlags = g_u8FaultFlags;
    
    // Send via CAN
    CANTransmit(CAN_ID_MODULE_STATUS, (uint8_t*)&status, sizeof(status));
}
```

### 5. Storage Manager (STORE.c)

**Data logging and configuration management**

#### Storage Architecture:
```c
// Storage destinations
typedef enum {
    STORE_EEPROM,                              // Configuration parameters
    STORE_SD_CARD,                             // Historical data logging
    STORE_FLASH,                               // Firmware parameters
} StorageType;

// Data logging structure
typedef struct {
    uint32_t timestamp;                        // RTC timestamp
    uint16_t cellVoltages[TOTAL_CELL_COUNT_MAX]; // All cell voltages
    uint16_t cellTemperatures[TOTAL_CELL_COUNT_MAX]; // All temperatures
    uint16_t moduleCurrent;                    // Module current
    uint8_t  moduleState;                      // Operating state
    uint8_t  balanceStatus[TOTAL_CELL_COUNT_MAX]; // Balancing active flags
} DataLogEntry;
```

#### Logging Process:
```c
void logPeriodicData(void) {
    DataLogEntry entry;
    
    // Get timestamp from RTC
    entry.timestamp = RTCGetCurrentTime();
    
    // Copy cell data
    for (int i = 0; i < g_u8ActiveCellCount; i++) {
        entry.cellVoltages[i] = g_cellData[i].voltage;
        entry.cellTemperatures[i] = g_cellData[i].temperature;
        entry.balanceStatus[i] = g_cellData[i].balancing;
    }
    
    // Add module-level data
    entry.moduleCurrent = g_u16ModuleCurrent;
    entry.moduleState = g_eCurrentState;
    
    // Write to SD card
    SDWriteLogEntry(&entry);
}
```

## Control Flow

### Periodic Task Execution

```
Timer Interrupt (100ms base rate)
├── Increment system tick counter
├── Set service needed flag
└── Wake from sleep mode

Main Loop Service (300ms cycle)
├── Frame Type Determination
│   ├── READ Frame (Frames 0, 2, 4...)
│   │   ├── Send cell query commands
│   │   ├── Collect vUART responses
│   │   ├── Process voltage/temperature data
│   │   └── Update cell statistics
│   └── WRITE Frame (Frames 1, 3, 5...)
│       ├── Send module status via CAN
│       ├── Log data to SD card
│       ├── Process balance commands
│       └── Update EEPROM if needed
├── Safety Monitoring
│   ├── Check voltage limits
│   ├── Check temperature limits
│   ├── Check communication timeouts
│   └── Update fault flags
├── State Management
│   ├── Process pack controller commands
│   ├── Validate state transitions
│   └── Update operating mode
└── Power Management
    ├── Refresh watchdog timer
    ├── Check for sleep conditions
    └── Enter low power mode
```

### Dual-Phase Operation

The firmware implements a dual-phase architecture to ensure data integrity and deterministic operation:

#### READ Phase (Data Collection):
```c
void executeReadFrame(void) {
    // Initiate cell data collection
    for (int i = 0; i < g_u8ActiveCellCount; i++) {
        // Send query to cell CPU via vUART
        sendCellQuery(i);
        
        // Wait for response with timeout
        if (waitForCellResponse(i, CELL_RESPONSE_TIMEOUT)) {
            // Process and validate response
            processCellData(i, g_cellResponses[i]);
        } else {
            // Handle communication timeout
            handleCellTimeout(i);
        }
    }
    
    // Update aggregated statistics
    calculateCellStatistics();
}
```

#### WRITE Phase (Data Output):
```c
void executeWriteFrame(void) {
    // Send status to pack controller
    CANSendModuleStatus();
    
    // Log data if logging enabled
    if (g_bLoggingEnabled) {
        logPeriodicData();
    }
    
    // Process balance commands
    if (g_eCurrentState == EMODSTATE_ON) {
        updateCellBalancing();
    }
    
    // Update configuration if changed
    if (g_bConfigChanged) {
        saveConfiguration();
        g_bConfigChanged = false;
    }
}
```

## Communication Architecture

### Multi-Protocol Interface

```
┌─────────────────────────────────────────────────────────────────┐
│                    ModuleCPU Communication                       │
├─────────────────────────────────────────────────────────────────┤
│ Pack Controller Interface                                       │
│ ├── Protocol: CAN 2.0B                                         │
│ ├── Baud Rate: 250 kbps                                        │
│ ├── Message Types: Status, Commands, Diagnostics               │
│ └── Timeout: 11.1 seconds                                      │
├─────────────────────────────────────────────────────────────────┤
│ Cell CPU Interface                                              │
│ ├── Protocol: Virtual UART (bit-banged)                        │
│ ├── Baud Rate: 20 kbps (50μs per bit)                         │
│ ├── Frame Format: 1 start + 8 data + 1 stop                   │
│ └── Cell Response: 4 bytes per cell                            │
├─────────────────────────────────────────────────────────────────┤
│ Debug Interface                                                 │
│ ├── Protocol: UART                                             │
│ ├── Baud Rate: 9600 bps                                        │
│ └── Usage: Development and troubleshooting                     │
├─────────────────────────────────────────────────────────────────┤
│ External Storage                                                │
│ ├── SD Card: SPI interface for data logging                    │
│ ├── EEPROM: Internal for configuration                         │
│ └── RTC: I2C interface for timestamping                        │
└─────────────────────────────────────────────────────────────────┘
```

### Message Flow Diagram

```
Pack Controller ←→ ModuleCPU ←→ Cell CPUs (1-94)
      │               │              │
   CAN Bus         ADC/vUART      Individual
   250kbps         20kbps each    Cell ICs
      │               │              │
  ┌─────────┐    ┌─────────────┐    ┌──────────┐
  │Commands │    │   Status    │    │ Voltage  │
  │ States  │    │  Telemetry  │    │   Temp   │
  │Timeouts │    │   Faults    │    │ Balance  │
  └─────────┘    └─────────────┘    └──────────┘
```

## Data Structures

### Core System Data

```c
// Global cell data array
typedef struct {
    uint16_t voltage;                          // Cell voltage in millivolts
    uint16_t temperature;                      // Cell temperature (scaled)
    uint8_t  balancing;                        // Balancing active flag
    uint8_t  status;                           // Cell status flags
    uint32_t lastUpdate;                       // Last successful update
} CellData;

CellData g_cellData[TOTAL_CELL_COUNT_MAX];

// System statistics
typedef struct {
    uint16_t voltageHigh;                      // Highest cell voltage
    uint16_t voltageLow;                       // Lowest cell voltage  
    uint16_t voltageAverage;                   // Average cell voltage
    uint16_t temperatureHigh;                  // Highest temperature
    uint16_t temperatureLow;                   // Lowest temperature
    uint16_t temperatureAverage;               // Average temperature
    uint8_t  cellHighIndex;                    // Index of highest voltage cell
    uint8_t  cellLowIndex;                     // Index of lowest voltage cell
} SystemStatistics;

SystemStatistics g_systemStats;
```

### Configuration Parameters

```c
// Module configuration stored in EEPROM
typedef struct {
    uint8_t  cellCount;                        // Number of active cells
    uint16_t balanceThreshold;                 // Voltage difference for balancing
    uint16_t balanceTargetVoltage;             // Target voltage for balancing
    uint16_t overVoltageLimit;                 // Overvoltage protection limit
    uint16_t underVoltageLimit;                // Undervoltage protection limit
    uint16_t overTemperatureLimit;             // Overtemperature protection
    uint16_t underTemperatureLimit;            // Undertemperature protection
    uint8_t  moduleAddress;                    // CAN address for this module
    uint32_t configurationCRC;                 // Configuration integrity check
} ModuleConfiguration;
```

The ModuleCPU firmware provides comprehensive battery module management with deterministic operation, robust communication, and comprehensive safety features. The dual-phase architecture ensures data integrity while the layered design enables maintainable and extensible code.