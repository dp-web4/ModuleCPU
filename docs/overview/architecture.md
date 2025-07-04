# ModuleCPU Architecture

## Table of Contents

1. [Overview](#overview)
2. [System Architecture](#system-architecture)
3. [Hardware Architecture](#hardware-architecture)
4. [Software Architecture](#software-architecture)
5. [Data Flow Architecture](#data-flow-architecture)
6. [Communication Architecture](#communication-architecture)
7. [Storage Architecture](#storage-architecture)
8. [Power Management Architecture](#power-management-architecture)

## Overview

The ModuleCPU firmware implements a sophisticated architecture designed for real-time battery cell monitoring, intelligent balancing, and hierarchical communication within the ModBatt ecosystem. Built on the ATmega64M1 platform, it provides a robust foundation for managing up to 94 battery cells with comprehensive data logging and communication capabilities.

### Architectural Principles

**Real-Time Operation**: Deterministic response times for critical battery management functions
**Modular Design**: Clear separation of concerns with well-defined interfaces
**Resource Efficiency**: Optimized for 8-bit microcontroller constraints
**Reliability**: Robust error handling and recovery mechanisms
**Scalability**: Support for varying cell counts and module configurations

## System Architecture

### Hierarchical System Context

```
┌─────────────────────────────────────────────────────────────────┐
│                      Vehicle System                            │
└────────────────────┬────────────────────────────────────────────┘
                     │ Vehicle CAN Bus
┌────────────────────┼────────────────────────────────────────────┐
│               Pack Controller                                   │
│              (STM32WB55)                                       │
│  • Central pack management                                     │
│  • VCU communication                                          │
│  • Multi-module coordination                                   │
│  • System-level safety                                        │
└────────────────────┬────────────────────────────────────────────┘
                     │ Module CAN Network
    ┌────────────────┼────────────────┬───────────────────────────┐
    │                │                │                           │
┌───▼────────────────▼────────────────▼───────────────────────────▼──┐
│                    Module Layer                                   │
│  ┌─────────────┬─────────────┬─────────────┬─────────────────────┐  │
│  │ Module 1    │ Module 2    │ Module 3    │ Module N            │  │
│  │(ModuleCPU)  │(ModuleCPU)  │(ModuleCPU)  │(ModuleCPU)          │  │
│  │             │             │             │                     │  │
│  │ • Cell      │ • Cell      │ • Cell      │ • Cell              │  │
│  │   monitoring│   monitoring│   monitoring│   monitoring        │  │
│  │ • Local     │ • Local     │ • Local     │ • Local             │  │
│  │   balancing │   balancing │   balancing │   balancing         │  │
│  │ • Data      │ • Data      │ • Data      │ • Data              │  │
│  │   logging   │   logging   │   logging   │   logging           │  │
│  │ • Module    │ • Module    │ • Module    │ • Module            │  │
│  │   control   │   control   │   control   │   control           │  │
│  └─────────────┴─────────────┴─────────────┴─────────────────────┘  │
└───┬───────────────────────────────────────────────────────────────┘
    │
┌───▼───────────────────────────────────────────────────────────────┐
│                     Cell Layer                                   │
│  ┌─────────────┬─────────────┬─────────────┬─────────────────────┐  │
│  │ Cell 1-94   │ Cell 1-94   │ Cell 1-94   │ Cell 1-94           │  │
│  │ • Voltage   │ • Voltage   │ • Voltage   │ • Voltage           │  │
│  │ • Current   │ • Current   │ • Current   │ • Current           │  │
│  │ • Temp      │ • Temp      │ • Temp      │ • Temp              │  │
│  │ • Balance   │ • Balance   │ • Balance   │ • Balance           │  │
│  │   circuits  │   circuits  │   circuits  │   circuits          │  │
│  └─────────────┴─────────────┴─────────────┴─────────────────────┘  │
└───────────────────────────────────────────────────────────────────┘
```

### Module Controller Role

```
Module Controller Responsibilities:
┌─────────────────────────────────────────────────────────────────┐
│                    Local Module Management                      │
├─────────────────────────────────────────────────────────────────┤
│ Cell-Level Operations:                                          │
│ • Individual cell voltage and temperature monitoring            │
│ • Cell balancing control (FET + relay)                         │
│ • Local fault detection and protection                         │
│ • Cell-level data acquisition and processing                   │
├─────────────────────────────────────────────────────────────────┤
│ Module-Level Operations:                                        │
│ • Module state management and control                          │
│ • Aggregate data calculation and reporting                     │
│ • Local data storage and logging                               │
│ • Module-specific configuration management                     │
├─────────────────────────────────────────────────────────────────┤
│ System Integration:                                             │
│ • Pack controller communication via CAN                        │
│ • Real-time telemetry reporting                                │
│ • Command execution from pack controller                       │
│ • Hierarchical safety coordination                             │
└─────────────────────────────────────────────────────────────────┘
```

## Hardware Architecture

### ATmega64M1 System Block Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                   ATmega64M1 Microcontroller                   │
│                      (8MHz Internal Clock)                     │
├─────────────────────────────────────────────────────────────────┤
│                         CPU Core                               │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │         8-bit AVR RISC Processor                       │   │
│  │  • 64KB Flash Program Memory                           │   │
│  │  • 4KB SRAM Data Memory                                │   │
│  │  • 2KB EEPROM Non-volatile Storage                     │   │
│  │  • 131 CPU Instructions                                │   │
│  │  • Single Clock Cycle Execution                        │   │
│  └─────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│                      Peripheral System                         │
│  ┌─────────────┬─────────────┬─────────────┬─────────────────┐   │
│  │    ADC      │    CAN      │   Timers    │   I/O Ports     │   │
│  │  • 12-bit   │ • CAN 2.0A/B│ • Timer0/1  │ • Port A/B/C/D  │   │
│  │  • 11 chan  │ • 1Mbps max │ • 16-bit    │ • 48 I/O pins   │   │
│  │  • Diff     │ • Hardware  │ • PWM       │ • Interrupt     │   │
│  │    inputs   │   filters   │ • Capture   │   capable       │   │
│  └─────────────┴─────────────┴─────────────┴─────────────────┘   │
│  ┌─────────────┬─────────────┬─────────────┬─────────────────┐   │
│  │     SPI     │     I2C     │    UART     │   Watchdog      │   │
│  │  • Master/  │ • TWI       │ • Async     │ • Programmable  │   │
│  │    Slave    │   interface │   Serial    │   timeout       │   │
│  │  • 4MHz max │ • 400kHz    │ • Debug     │ • System reset  │   │
│  │             │   max       │   interface │   capability    │   │
│  └─────────────┴─────────────┴─────────────┴─────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### Memory Architecture

```
ATmega64M1 Memory Map:
┌─────────────────────────────────────────────────────────┐ 0x0000
│                Flash Program Memory                     │
│                      (64KB)                            │
│  ┌─────────────────────────────────────────────────────┐ │ 0x0000
│  │              Interrupt Vectors                      │ │ 0x003C
│  ├─────────────────────────────────────────────────────┤ │
│  │                Main Application                     │ │ 0x003C
│  │  • Core firmware modules                            │ │
│  │  • Communication protocols                          │ │
│  │  • Cell monitoring algorithms                       │ │
│  │  • Balancing control logic                          │ │
│  │  • Storage and logging functions                    │ │
│  ├─────────────────────────────────────────────────────┤ │ 0xF000
│  │              Configuration Data                     │ │
│  │  • Factory defaults                                 │ │
│  │  • Calibration tables                               │ │
│  │  • Module-specific parameters                       │ │
│  └─────────────────────────────────────────────────────┘ │ 0xFFFF
├─────────────────────────────────────────────────────────┤
│                    SRAM Data Memory                     │ 0x0100
│                        (4KB)                           │
│  ┌─────────────────────────────────────────────────────┐ │ 0x0100
│  │                  Register File                      │ │ 0x01FF
│  ├─────────────────────────────────────────────────────┤ │
│  │               I/O Register Space                    │ │ 0x01FF
│  ├─────────────────────────────────────────────────────┤ │ 0x0200
│  │                Global Variables                     │ │
│  │  • Cell data arrays                                 │ │
│  │  • Configuration parameters                         │ │
│  │  • Communication buffers                            │ │
│  ├─────────────────────────────────────────────────────┤ │ 0x0800
│  │                   Stack Space                       │ │
│  │  • Function call stack                              │ │
│  │  • Local variables                                  │ │
│  │  • Interrupt context                                │ │
│  └─────────────────────────────────────────────────────┘ │ 0x10FF
├─────────────────────────────────────────────────────────┤
│                  EEPROM Memory                          │ 0x0000
│                     (2KB)                              │
│  • Configuration parameters                             │ 0x07FF
│  • Calibration data                                     │
│  • Factory defaults                                     │
│  • Module identification                                │
└─────────────────────────────────────────────────────────┘
```

### Peripheral Interfaces

```c
// Hardware peripheral configuration
typedef struct {
    // ADC Configuration
    struct {
        uint8_t cell_count;             // Number of cells to monitor
        uint16_t reference_voltage;     // ADC reference (mV)
        uint8_t conversion_time;        // ADC conversion time
        bool differential_mode;         // Enable differential inputs
    } adc_config;
    
    // CAN Configuration
    struct {
        uint32_t baudrate;              // CAN bus baudrate
        uint16_t module_id;             // Module CAN identifier
        uint8_t acceptance_filters;     // Number of message filters
        bool loopback_mode;             // Loopback for testing
    } can_config;
    
    // Timer Configuration
    struct {
        uint16_t periodic_interval;     // Main periodic timer (ms)
        uint16_t watchdog_timeout;      // Watchdog timeout (ms)
        uint8_t pwm_frequency;          // PWM frequency for balancing
        bool sleep_mode_enabled;        // Enable sleep mode
    } timer_config;
    
    // Storage Configuration
    struct {
        bool sd_card_enabled;           // Enable SD card logging
        uint32_t log_interval;          // Data logging interval (ms)
        uint16_t eeprom_backup_interval; // EEPROM backup interval (min)
        bool rtc_enabled;               // Enable RTC for timestamping
    } storage_config;
} HardwareConfig;
```

## Software Architecture

### Layered Software Design

```
┌─────────────────────────────────────────────────────────────────┐
│                     Application Layer                          │
│  ┌─────────────┬─────────────┬─────────────┬─────────────────┐  │
│  │ Cell Data   │ Balance     │ Pack        │ Data Archive    │  │
│  │ Manager     │ Controller  │ Interface   │ Service         │  │
│  │             │             │             │                 │  │
│  │ • Cell      │ • Voltage   │ • CAN       │ • Historical    │  │
│  │   monitoring│   delta     │   protocol  │   data          │  │
│  │ • Fault     │   analysis  │ • State     │ • Event         │  │
│  │   detection │ • Balance   │   sync      │   logging       │  │
│  │ • Data      │   timing    │ • Telemetry │ • Configuration │  │
│  │   validation│ • Thermal   │   reporting │   backup        │  │
│  │             │   mgmt      │             │                 │  │
│  └─────────────┴─────────────┴─────────────┴─────────────────┘  │
└─────────────────┬───────────────────────────────────────────────┘
                  │ Service APIs
┌─────────────────┼───────────────────────────────────────────────┐
│                 Service Layer                                   │
│  ┌─────────────┴─────────────┬─────────────────────────────────┐  │
│  │ Communication Services    │ Storage Services                │  │
│  │                          │                                 │  │
│  │ • CAN message handler     │ • EEPROM parameter manager     │  │
│  │ • Virtual UART interface  │ • SD card file system          │  │
│  │ • Debug serial output     │ • RTC time service             │  │
│  │ • Protocol state machine  │ • Data compression/archival    │  │
│  │ • Error handling/recovery │ • Configuration management     │  │
│  └───────────────────────────┴─────────────────────────────────┘  │
└─────────────────┬───────────────────────────────────────────────┘
                  │ Hardware APIs
┌─────────────────┼───────────────────────────────────────────────┐
│            Hardware Abstraction Layer                          │
│  ┌─────────────┴─────────────┬─────────────────────────────────┐  │
│  │ Peripheral Drivers        │ System Services                 │  │
│  │                          │                                 │  │
│  │ • ADC driver              │ • Interrupt management         │  │
│  │ • CAN controller          │ • Timer services                │  │
│  │ • SPI driver              │ • Watchdog management           │  │
│  │ • I2C driver              │ • Power management              │  │
│  │ • GPIO driver             │ • Sleep mode control            │  │
│  │ • UART driver             │ • Clock management              │  │
│  └───────────────────────────┴─────────────────────────────────┘  │
└─────────────────┬───────────────────────────────────────────────┘
                  │ Register Access
┌─────────────────┼───────────────────────────────────────────────┐
│                    Hardware Layer                              │
│  ATmega64M1 Registers, External Components, Analog Circuits    │
└─────────────────────────────────────────────────────────────────┘
```

### Module State Machine

```c
// Module operational states
typedef enum {
    EMODSTATE_OFF = 0,          // Module disabled, minimal operation
    EMODSTATE_STANDBY = 1,      // Module ready, monitoring active
    EMODSTATE_ACTIVE = 3,       // Module active, full operation
    EMODSTATE_FAULT = 4,        // Module fault condition
    EMODSTATE_SERVICE = 5       // Module service/maintenance mode
} ModuleState;

// State transition management
typedef struct {
    ModuleState current_state;   // Current module state
    ModuleState requested_state; // Requested state from pack controller
    uint32_t state_entry_time;   // Time when current state was entered
    uint32_t last_transition;    // Time of last state transition
    bool transition_pending;     // Transition request pending
    uint8_t fault_code;         // Active fault code (if any)
} ModuleStateManager;

// State machine implementation
bool ProcessStateTransition(ModuleStateManager* sm, ModuleState new_state) {
    // Validate state transition
    if (!IsValidTransition(sm->current_state, new_state)) {
        return false;
    }
    
    // Check transition conditions
    if (!CheckTransitionConditions(sm->current_state, new_state)) {
        return false;
    }
    
    // Execute state exit actions
    ExecuteStateExitActions(sm->current_state);
    
    // Update state
    sm->current_state = new_state;
    sm->state_entry_time = GetSystemTime();
    sm->last_transition = sm->state_entry_time;
    sm->transition_pending = false;
    
    // Execute state entry actions
    ExecuteStateEntryActions(new_state);
    
    return true;
}
```

### Task Scheduling Architecture

```c
// Periodic task framework
typedef enum {
    EFRAMETYPE_READ,            // Data acquisition frame
    EFRAMETYPE_WRITE            // Data processing and communication frame
} EFrameType;

// Task execution framework
typedef struct {
    void (*task_function)(void);     // Task function pointer
    uint32_t period_ms;             // Task period in milliseconds
    uint32_t last_execution;        // Last execution timestamp
    uint8_t priority;               // Task priority (0 = highest)
    bool enabled;                   // Task enable flag
    const char* name;               // Task name for debugging
} TaskDescriptor;

// System task table
static const TaskDescriptor system_tasks[] = {
    // High priority tasks (every 300ms)
    {CellDataAcquisition,    300, 0, 1, true,  "CellRead"},
    {CellDataProcessing,     300, 0, 1, true,  "CellProcess"},
    {BalanceControl,         300, 0, 2, true,  "Balance"},
    {PackCommunication,      300, 0, 2, true,  "PackComm"},
    
    // Medium priority tasks (every 1000ms)
    {DataLogging,           1000, 0, 3, true,  "DataLog"},
    {HealthMonitoring,      1000, 0, 3, true,  "Health"},
    {ConfigurationSync,     1000, 0, 4, true,  "ConfigSync"},
    
    // Low priority tasks (every 5000ms)
    {SystemDiagnostics,     5000, 0, 5, true,  "Diagnostics"},
    {EEPROMBackup,         30000, 0, 6, true,  "EEPROMBackup"},
};

// Task scheduler implementation
void ExecuteScheduledTasks(void) {
    uint32_t current_time = GetSystemTime();
    
    for (uint8_t i = 0; i < ARRAY_SIZE(system_tasks); i++) {
        const TaskDescriptor* task = &system_tasks[i];
        
        if (task->enabled && 
            (current_time - task->last_execution) >= task->period_ms) {
            
            // Execute task
            task->task_function();
            
            // Update execution time
            system_tasks[i].last_execution = current_time;
            
            // Refresh watchdog
            RefreshWatchdog();
        }
    }
}
```

## Data Flow Architecture

### Cell Data Processing Pipeline

```
Data Sources → Acquisition → Processing → Storage → Communication
     ↓             ↓            ↓          ↓           ↓
┌──────────┐ ┌─────────────┐ ┌─────────┐ ┌────────┐ ┌─────────┐
│ Cell AFE │ │ ADC Driver  │ │ Filter  │ │ Local  │ │ CAN TX  │
│ Hardware │→│ & Mux       │→│ & Scale │→│ Storage│→│ Queue   │
│          │ │             │ │         │ │        │ │         │
│ • Cell   │ │ • 12-bit    │ │ • Range │ │ • RAM  │ │ • Pack  │
│   voltage│ │   ADC       │ │   check │ │   cache│ │   data  │
│ • Cell   │ │ • Multi     │ │ • Unit  │ │ • SD   │ │ • Status│
│   temp   │ │   channel   │ │   conv  │ │   card │ │   msgs  │
│ • Balance│ │ • Sample    │ │ • Fault │ │ • EEPROM│ │ • Faults│
│   status │ │   & Hold    │ │   detect│ │ backup │ │         │
└──────────┘ └─────────────┘ └─────────┘ └────────┘ └─────────┘
     ↑                                                  ↓
     └─────────────── Control Feedback ←───────────────┘
```

### Data Structure Hierarchy

```c
// Cell-level data structure
typedef struct {
    uint16_t voltage_mv;        // Cell voltage in millivolts
    int16_t temperature_dc;     // Cell temperature in 0.1°C
    uint8_t balance_state;      // Balancing status
    uint8_t fault_flags;        // Cell-specific fault flags
    uint32_t timestamp;         // Data acquisition timestamp
    uint16_t sample_count;      // Number of samples averaged
} CellData;

// Module-level data aggregation
typedef struct {
    CellData cells[TOTAL_CELL_COUNT_MAX];  // Individual cell data
    uint8_t active_cell_count;             // Number of active cells
    
    // Aggregated statistics
    uint16_t voltage_sum;                  // Total module voltage
    uint16_t voltage_min;                  // Minimum cell voltage
    uint16_t voltage_max;                  // Maximum cell voltage
    uint16_t voltage_avg;                  // Average cell voltage
    uint16_t voltage_delta;                // Max - Min voltage
    
    int16_t temperature_min;               // Minimum cell temperature
    int16_t temperature_max;               // Maximum cell temperature
    int16_t temperature_avg;               // Average cell temperature
    
    // Module status
    ModuleState state;                     // Current module state
    uint16_t fault_code;                   // Active fault conditions
    uint8_t balance_active_count;          // Number of cells balancing
    uint32_t last_update;                  // Last data update time
    
    // Performance metrics
    uint32_t cycle_count;                  // Data acquisition cycles
    uint16_t communication_errors;         // CAN communication errors
    uint8_t sd_card_status;               // SD card health status
} ModuleData;

// System configuration data
typedef struct {
    // Module identification
    uint16_t module_id;                    // Unique module identifier
    uint8_t hardware_version;              // Hardware revision
    uint16_t firmware_version;             // Firmware version
    uint32_t serial_number;                // Module serial number
    
    // Operational parameters
    uint16_t max_cell_voltage_mv;          // Maximum allowed cell voltage
    uint16_t min_cell_voltage_mv;          // Minimum allowed cell voltage
    uint16_t balance_threshold_mv;         // Voltage delta for balancing
    int16_t max_temperature_dc;            // Maximum operating temperature
    int16_t min_temperature_dc;            // Minimum operating temperature
    
    // Timing parameters
    uint16_t data_acquisition_period_ms;   // ADC sampling period
    uint16_t communication_period_ms;      // CAN transmission period
    uint16_t balance_time_limit_ms;        // Maximum balancing time
    uint32_t pack_timeout_ms;              // Pack controller timeout
    
    // Calibration data
    int16_t voltage_offset[TOTAL_CELL_COUNT_MAX];  // Per-cell voltage offset
    int16_t temperature_offset[TOTAL_CELL_COUNT_MAX]; // Per-cell temp offset
    uint16_t adc_reference_mv;             // ADC reference voltage
    
    // Configuration validation
    uint32_t checksum;                     // Configuration data checksum
    uint32_t last_modified;                // Last modification timestamp
} ModuleConfiguration;
```

## Communication Architecture

### CAN Bus Communication Stack

```
┌─────────────────────────────────────────────────────────────────┐
│                    Pack Controller                             │
│                   (STM32WB55)                                  │
└────────────────────┬────────────────────────────────────────────┘
                     │ CAN Bus Network (500kbps)
    ┌────────────────┼────────────────┬───────────────────────────┐
    │                │                │                           │
┌───▼────────┐ ┌─────▼──────┐ ┌──────▼──────┐ ┌─────────────────▼──┐
│ Module 1   │ │ Module 2   │ │ Module 3    │ │ Module N           │
│ ID: 0x501  │ │ ID: 0x502  │ │ ID: 0x503   │ │ ID: 0x500+N        │
│(ModuleCPU) │ │(ModuleCPU) │ │(ModuleCPU)  │ │(ModuleCPU)         │
└────────────┘ └────────────┘ └─────────────┘ └────────────────────┘
```

### CAN Message Protocol

```c
// CAN message ID allocation for modules
#define CAN_ID_MODULE_BASE      0x500           // Base ID for modules
#define CAN_ID_PACK_COMMAND     0x400           // Pack controller commands
#define CAN_ID_PACK_STATUS      0x410           // Pack status messages

// Module-specific message IDs
#define GET_MODULE_ID(module_num)  (CAN_ID_MODULE_BASE + module_num)

// CAN message types
typedef enum {
    CAN_MSG_MODULE_STATUS = 0,      // Module status report
    CAN_MSG_CELL_DATA = 1,          // Cell telemetry data
    CAN_MSG_FAULT_REPORT = 2,       // Fault notification
    CAN_MSG_CONFIG_ACK = 3,         // Configuration acknowledgment
    CAN_MSG_HEARTBEAT = 4           // Module heartbeat
} CANMessageType;

// CAN message structure
typedef struct {
    uint32_t id;                    // CAN message identifier
    uint8_t data[8];               // Message data payload
    uint8_t length;                // Data length (0-8 bytes)
    uint32_t timestamp;            // Message timestamp
    CANMessageType type;           // Message type classification
} CANMessage;

// Module status message format
typedef struct {
    uint8_t module_state;          // Current module state
    uint8_t active_cell_count;     // Number of active cells
    uint16_t voltage_sum;          // Total module voltage (100mV resolution)
    uint16_t voltage_delta;        // Voltage spread (10mV resolution)
    uint8_t temperature_max;       // Maximum temperature (1°C offset by 50)
    uint8_t fault_code;            // Active fault code
} __attribute__((packed)) ModuleStatusMessage;

// Cell data message format (multiple messages for all cells)
typedef struct {
    uint8_t start_cell_index;      // Starting cell number
    uint8_t cell_count;            // Number of cells in message
    uint16_t voltages[3];          // Cell voltages (1mV resolution)
    // Remaining bytes used for additional cells or temperatures
} __attribute__((packed)) CellDataMessage;
```

### Virtual UART Debug Interface

```c
// Virtual UART implementation for debugging
typedef struct {
    volatile uint8_t tx_buffer[256];    // Transmit buffer
    volatile uint8_t rx_buffer[256];    // Receive buffer
    volatile uint8_t tx_head;           // Transmit buffer head
    volatile uint8_t tx_tail;           // Transmit buffer tail
    volatile uint8_t rx_head;           // Receive buffer head
    volatile uint8_t rx_tail;           // Receive buffer tail
    uint16_t baudrate;                  // Virtual baudrate
    bool enabled;                       // Interface enable flag
} VirtualUART;

// Virtual UART timing based on bit periods
#define VUART_START_BIT_TICKS   VUART_BIT_TICKS
#define VUART_STOP_BIT_TICKS    VUART_BIT_TICKS
#define VUART_DATA_BIT_TICKS    VUART_BIT_TICKS

// Debug output functions
void DebugPrint(const char* format, ...) {
    char buffer[128];
    va_list args;
    
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    VirtualUART_SendString(buffer);
}

void DebugPrintCellData(const CellData* cell, uint8_t cell_index) {
    DebugPrint("Cell[%d]: V=%dmV, T=%d.%dC, Bal=%s, Faults=0x%02X\n",
               cell_index,
               cell->voltage_mv,
               cell->temperature_dc / 10,
               abs(cell->temperature_dc % 10),
               (cell->balance_state ? "ON" : "OFF"),
               cell->fault_flags);
}
```

## Storage Architecture

### Multi-Level Storage System

```
┌─────────────────────────────────────────────────────────────────┐
│                    Storage Hierarchy                           │
├─────────────────────────────────────────────────────────────────┤
│ Level 1: Volatile Memory (SRAM - 4KB)                          │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ • Real-time cell data cache                             │   │
│  │ • Communication buffers                                 │   │
│  │ • Working variables and calculation space               │   │
│  │ • Interrupt service routine data                        │   │
│  └─────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│ Level 2: Non-Volatile Memory (EEPROM - 2KB)                    │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ • Configuration parameters                              │   │
│  │ • Calibration data and factory settings                 │   │
│  │ • Module identification and versioning                  │   │
│  │ • Critical fault history                                │   │
│  └─────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│ Level 3: Mass Storage (SD Card - GB scale)                     │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ • Historical cell data logs                             │   │
│  │ • Event and fault logs with timestamps                  │   │
│  │ • System performance metrics                            │   │
│  │ • Configuration backup and restore data                 │   │
│  │ • Diagnostic and maintenance logs                       │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### File System Organization

```c
// SD card file system structure
typedef struct {
    char root_path[16];             // Root directory path
    char data_path[32];             // Data files directory
    char config_path[32];           // Configuration backup directory
    char log_path[32];              // Event log directory
    char maintenance_path[32];      // Maintenance data directory
} FileSystemLayout;

// Standard file naming convention
#define DATA_FILE_FORMAT        "DATA_%04d%02d%02d.CSV"     // YYYY MM DD
#define EVENT_FILE_FORMAT       "EVENT_%04d%02d%02d.LOG"    // YYYY MM DD
#define CONFIG_FILE_FORMAT      "CONFIG_%08X.BAK"          // Timestamp
#define MAINTENANCE_FILE_FORMAT "MAINT_%04d%02d.LOG"        // YYYY MM

// Data logging structure
typedef struct {
    uint32_t timestamp;             // RTC timestamp
    uint16_t sequence_number;       // Log sequence number
    uint8_t data_type;              // Type of logged data
    uint8_t data_length;            // Length of data payload
    uint8_t data[32];              // Data payload
    uint16_t checksum;              // Data integrity checksum
} __attribute__((packed)) LogEntry;

// SD card interface functions
bool SD_Initialize(void) {
    // Initialize SPI interface
    SPI_Init();
    
    // Initialize SD card
    if (!SD_Init()) {
        return false;
    }
    
    // Mount file system
    if (f_mount(&fs, "", 1) != FR_OK) {
        return false;
    }
    
    // Create directory structure
    CreateDirectoryStructure();
    
    return true;
}

bool LogCellData(const ModuleData* module_data) {
    char filename[32];
    RTC_DateTime current_time;
    
    // Get current date/time
    RTC_GetDateTime(&current_time);
    
    // Generate filename
    snprintf(filename, sizeof(filename), DATA_FILE_FORMAT,
             current_time.year, current_time.month, current_time.day);
    
    // Open file for append
    FIL file;
    if (f_open(&file, filename, FA_WRITE | FA_OPEN_APPEND) != FR_OK) {
        return false;
    }
    
    // Write CSV header if new file
    if (f_size(&file) == 0) {
        f_printf(&file, "Timestamp,ModuleID,CellIndex,Voltage,Temperature,BalanceState,Faults\n");
    }
    
    // Write cell data
    for (uint8_t i = 0; i < module_data->active_cell_count; i++) {
        const CellData* cell = &module_data->cells[i];
        f_printf(&file, "%lu,%d,%d,%d,%d,%d,0x%02X\n",
                 cell->timestamp,
                 GetModuleID(),
                 i,
                 cell->voltage_mv,
                 cell->temperature_dc,
                 cell->balance_state,
                 cell->fault_flags);
    }
    
    f_close(&file);
    return true;
}
```

## Power Management Architecture

### Power State Management

```c
// Power management states
typedef enum {
    POWER_STATE_ACTIVE,         // Full operation, all peripherals active
    POWER_STATE_IDLE,           // CPU idle, peripherals active
    POWER_STATE_STANDBY,        // Reduced operation, essential peripherals only
    POWER_STATE_SLEEP,          // Deep sleep, minimal operation
    POWER_STATE_SHUTDOWN        // Emergency shutdown
} PowerState;

// Power management configuration
typedef struct {
    PowerState current_state;    // Current power state
    PowerState requested_state;  // Requested power state
    uint32_t state_entry_time;   // Time when current state entered
    uint32_t idle_timeout;       // Timeout for idle mode entry
    uint32_t sleep_timeout;      // Timeout for sleep mode entry
    bool battery_low;            // Low battery condition
    bool thermal_limit;          // Thermal limiting active
} PowerManager;

// Power state transition implementation
void ProcessPowerManagement(PowerManager* pm) {
    uint32_t current_time = GetSystemTime();
    uint32_t idle_time = current_time - pm->state_entry_time;
    
    switch (pm->current_state) {
        case POWER_STATE_ACTIVE:
            // Check for idle conditions
            if (idle_time > pm->idle_timeout && CanEnterIdleMode()) {
                TransitionToPowerState(pm, POWER_STATE_IDLE);
            }
            break;
            
        case POWER_STATE_IDLE:
            // Check for sleep conditions
            if (idle_time > pm->sleep_timeout && CanEnterSleepMode()) {
                TransitionToPowerState(pm, POWER_STATE_SLEEP);
            }
            // Check for wake conditions
            else if (WakeConditionDetected()) {
                TransitionToPowerState(pm, POWER_STATE_ACTIVE);
            }
            break;
            
        case POWER_STATE_SLEEP:
            // Wake on interrupt or timeout
            if (WakeConditionDetected()) {
                TransitionToPowerState(pm, POWER_STATE_ACTIVE);
            }
            break;
            
        case POWER_STATE_SHUTDOWN:
            // Emergency shutdown state - requires manual recovery
            EnterEmergencyShutdown();
            break;
    }
    
    // Check for emergency conditions
    if (pm->battery_low || pm->thermal_limit) {
        TransitionToPowerState(pm, POWER_STATE_SHUTDOWN);
    }
}

// Sleep mode implementation
void EnterSleepMode(void) {
    // Disable non-essential peripherals
    DisableNonEssentialPeripherals();
    
    // Configure wake sources
    ConfigureWakeSources();
    
    // Set sleep mode
    set_sleep_mode(SLEEP_MODE_PWR_SAVE);
    
    // Enable sleep
    sleep_enable();
    
    // Enter sleep (wake on interrupt)
    sleep_cpu();
    
    // Disable sleep after wake
    sleep_disable();
    
    // Re-enable peripherals
    EnablePeripherals();
}
```

This comprehensive architecture provides the foundation for a robust, efficient, and scalable battery module controller with advanced monitoring, balancing, and communication capabilities.