# CAN Communication System

## Table of Contents

1. [Overview](#overview)
2. [Protocol Architecture](#protocol-architecture)
3. [Message Definitions](#message-definitions)
4. [Communication Flow](#communication-flow)
5. [Data Encoding](#data-encoding)
6. [State Management](#state-management)
7. [Error Handling](#error-handling)
8. [Performance Characteristics](#performance-characteristics)

## Overview

The ModuleCPU implements a comprehensive CAN-based communication system to interface with the pack controller within the ModBatt hierarchical battery management architecture. The system provides bidirectional communication for status reporting, command reception, and real-time data exchange.

### Key Features
- **Standard CAN 2.0B Protocol**: Compatible with automotive and industrial systems
- **Hierarchical Architecture**: Module-to-pack controller communication
- **Real-Time Operation**: Deterministic message timing and prioritization
- **Comprehensive Data Exchange**: Status, telemetry, commands, and diagnostics
- **Robust Error Handling**: Timeout detection, message validation, and fault recovery

### Communication Specifications
```
CAN Interface Specifications:
├── Protocol: CAN 2.0B (11-bit identifiers)
├── Baud Rate: 250 kbps
├── Bus Topology: Linear with 120Ω termination
├── Node Count: 1 Pack Controller + up to 32 Module Controllers
├── Message Priority: Based on CAN ID (lower = higher priority)
├── Timeout: 11.1 seconds for pack controller communication
└── Maximum Message Length: 8 bytes (standard CAN frame)
```

## Protocol Architecture

### Network Topology

```
┌─────────────────────────────────────────────────────────────────┐
│                    ModBatt CAN Network                          │
├─────────────────────────────────────────────────────────────────┤
│ Pack Controller (Master)                                        │
│ ├── ID Range: 0x510-0x51F (Commands to modules)               │
│ ├── Functions: Command, Control, Time sync, Registration       │
│ └── Timeout Monitoring: 11.1s per module                      │
├─────────────────────────────────────────────────────────────────┤
│ Module Controllers (Slaves) - Up to 32 nodes                  │
│ ├── ID Range: 0x500-0x50F (Status from modules)               │
│ ├── Functions: Status reporting, Data telemetry               │
│ └── Response Requirements: <300ms typical                     │
├─────────────────────────────────────────────────────────────────┤
│ CAN Bus Physical Layer                                          │
│ ├── Differential Signaling: CAN_H, CAN_L                      │
│ ├── Termination: 120Ω at each end                             │
│ ├── Cable: Twisted pair (automotive grade)                    │
│ └── Connector: Standard CAN connector or terminal blocks       │
└─────────────────────────────────────────────────────────────────┘
```

### Message Priority Scheme

```
CAN ID Priority (Lower ID = Higher Priority):
├── 0x500-0x50F: Module to Pack Controller Messages
│   ├── 0x500: Module Announcement (Highest Priority)
│   ├── 0x501: Hardware Details
│   ├── 0x502: Status 1 (Critical status)
│   ├── 0x503: Status 2 (Extended status)
│   ├── 0x504: Status 3 (Additional status)
│   ├── 0x505: Cell Detail Data
│   ├── 0x506: Time Request
│   ├── 0x507: Cell Communication Statistics 1
│   └── 0x508: Cell Communication Statistics 2
└── 0x510-0x51F: Pack Controller to Module Messages
    ├── 0x510: Module Registration
    ├── 0x511: Hardware Request
    ├── 0x512: Status Request
    ├── 0x514: State Change Command
    ├── 0x515: Detail Request
    ├── 0x516: Set Time
    ├── 0x517: Maximum State Setting
    ├── 0x51E: All Deregister
    └── 0x51F: All Isolate (Emergency)
```

## Message Definitions

### Module-to-Pack Controller Messages

#### 1. Module Announcement (0x500)
**Purpose**: Initial system registration and periodic keep-alive
```c
typedef struct {
    uint8_t moduleId;                          // Module address (0-31)
    uint8_t firmwareVersion;                   // Firmware version
    uint8_t hardwareVersion;                   // Hardware revision
    uint8_t cellCount;                         // Number of active cells
    uint8_t moduleState;                       // Current operating state
    uint8_t faultFlags;                        // Active fault conditions
    uint16_t sequenceNumber;                   // Message sequence counter
} ModuleAnnouncementMessage;
```

#### 2. Module Hardware Details (0x501)
**Purpose**: Detailed hardware and capability information
```c
typedef struct {
    uint16_t firmwareBuildNumber;              // Build number (e.g., 8278)
    uint8_t  manufactureId;                    // Manufacturer identifier
    uint8_t  partId;                           // Part identifier
    uint16_t hardwareCompatibility;            // Hardware compatibility code
    uint8_t  maxCellCount;                     // Maximum supported cells
    uint8_t  features;                         // Feature flags
} ModuleHardwareMessage;

// Feature flags definitions
#define FEATURE_ACTIVE_BALANCING     (1 << 0)  // Active balancing capability
#define FEATURE_TEMPERATURE_SENSING  (1 << 1)  // Temperature monitoring
#define FEATURE_CURRENT_SENSING      (1 << 2)  // Current measurement
#define FEATURE_DATA_LOGGING         (1 << 3)  // SD card logging
#define FEATURE_RTC                  (1 << 4)  // Real-time clock
```

#### 3. Module Status 1 (0x502) - Critical Status
**Purpose**: Essential operational status and safety information
```c
typedef struct {
    uint16_t cellVoltageHigh;                  // Highest cell voltage (mV)
    uint16_t cellVoltageLow;                   // Lowest cell voltage (mV)
    uint16_t cellVoltageAverage;               // Average cell voltage (mV)
    uint8_t  cellHighIndex;                    // Index of highest voltage cell
    uint8_t  cellLowIndex;                     // Index of lowest voltage cell
} ModuleStatus1Message;
```

#### 4. Module Status 2 (0x503) - Extended Status  
**Purpose**: Temperature and additional monitoring data
```c
typedef struct {
    uint16_t cellTemperatureHigh;              // Highest cell temperature
    uint16_t cellTemperatureLow;               // Lowest cell temperature
    uint16_t cellTemperatureAverage;           // Average cell temperature
    int16_t  moduleCurrent;                    // Module current (mA)
    uint8_t  activeCellCount;                  // Currently active cells
    uint8_t  balancingCellCount;               // Cells currently balancing
} ModuleStatus2Message;
```

#### 5. Module Status 3 (0x504) - Additional Status
**Purpose**: System health and diagnostic information
```c
typedef struct {
    uint32_t uptimeSeconds;                    // System uptime in seconds
    uint16_t communicationErrors;              // Cell communication error count
    uint8_t  systemTemperature;                // Module controller temperature
    uint8_t  powerSupplyVoltage;               // Supply voltage status
    uint16_t freeMemory;                       // Available RAM
    uint8_t  reserved[2];                      // Reserved for future use
} ModuleStatus3Message;
```

#### 6. Cell Detail Data (0x505)
**Purpose**: Individual cell voltage and temperature data
```c
typedef struct {
    uint8_t startCellIndex;                    // Starting cell index
    uint8_t cellCount;                         // Number of cells in this message
    struct {
        uint16_t voltage;                      // Cell voltage (mV)
        uint8_t  temperature;                  // Cell temperature (scaled)
        uint8_t  status;                       // Cell status flags
    } cellData[2];                             // Up to 2 cells per message
} CellDetailMessage;

// Cell status flags
#define CELL_STATUS_BALANCING        (1 << 0)  // Cell is balancing
#define CELL_STATUS_FAULT            (1 << 1)  // Cell has fault
#define CELL_STATUS_COMM_ERROR       (1 << 2)  // Communication error
#define CELL_STATUS_TEMP_INVALID     (1 << 3)  // Temperature reading invalid
```

### Pack Controller-to-Module Messages

#### 1. Module Registration (0x510)
**Purpose**: Assign module ID and configure operational parameters
```c
typedef struct {
    uint8_t assignedModuleId;                  // Assigned module address
    uint8_t maxAllowedState;                   // Maximum operating state
    uint16_t statusReportingInterval;          // Status message interval (ms)
    uint16_t cellDetailInterval;               // Cell detail message interval (ms)
    uint8_t enabledFeatures;                   // Feature enable flags
    uint8_t reserved;                          // Reserved
} ModuleRegistrationMessage;
```

#### 2. State Change Request (0x514)
**Purpose**: Command module to change operating state
```c
typedef struct {
    uint8_t targetModuleId;                    // Target module (0xFF = all)
    uint8_t requestedState;                    // Requested operating state
    uint8_t stateChangeReason;                 // Reason for state change
    uint8_t acknowledgementRequired;           // Require ACK response
    uint32_t timestamp;                        // Command timestamp
} StateChangeRequestMessage;

// Operating states
typedef enum {
    MODULE_STATE_OFF = 0,                      // All systems off
    MODULE_STATE_STANDBY = 1,                  // Monitoring only
    MODULE_STATE_PRECHARGE = 2,                // Preparing for operation
    MODULE_STATE_ON = 3,                       // Full operation
} ModuleOperatingState;
```

#### 3. Time Synchronization (0x516)
**Purpose**: Synchronize module real-time clocks
```c
typedef struct {
    uint32_t unixTimestamp;                    // Current Unix timestamp
    uint16_t milliseconds;                     // Subsecond precision
    uint8_t  timeZoneOffset;                   // Time zone offset (hours)
    uint8_t  daylightSaving;                   // Daylight saving flag
} TimeSynchronizationMessage;
```

## Communication Flow

### Initialization Sequence

```
1. Module Power-Up
   ├── Initialize CAN controller
   ├── Configure message filters
   ├── Start listening for registration
   └── Send Module Announcement (0x500)

2. Pack Controller Response
   ├── Receive Module Announcement
   ├── Validate module information
   ├── Assign module ID
   └── Send Module Registration (0x510)

3. Module Registration Confirmation
   ├── Receive registration message
   ├── Configure assigned parameters
   ├── Start periodic status reporting
   └── Send Module Hardware Details (0x501)

4. Normal Operation
   ├── Periodic status messages
   ├── Response to pack controller commands
   ├── Cell detail data transmission
   └── Keep-alive monitoring
```

### Periodic Communication Cycle

```c
void CANPeriodicTasks(void) {
    static uint32_t lastStatusTime = 0;
    static uint32_t lastCellDetailTime = 0;
    static uint32_t lastKeepAliveTime = 0;
    
    uint32_t currentTime = getSystemTicks();
    
    // Send status messages every 1 second
    if (currentTime - lastStatusTime >= 1000) {
        sendModuleStatus();
        lastStatusTime = currentTime;
    }
    
    // Send cell detail data every 5 seconds
    if (currentTime - lastCellDetailTime >= 5000) {
        sendCellDetailData();
        lastCellDetailTime = currentTime;
    }
    
    // Send keep-alive every 10 seconds
    if (currentTime - lastKeepAliveTime >= 10000) {
        sendModuleAnnouncement();
        lastKeepAliveTime = currentTime;
    }
    
    // Check for pack controller timeout
    checkPackControllerTimeout();
}

void sendModuleStatus(void) {
    // Send Status 1 (critical data)
    ModuleStatus1Message status1;
    populateStatus1Message(&status1);
    CANSendMessage(ECANMessageType_ModuleStatus1, 
                   (uint8_t*)&status1, sizeof(status1));
    
    // Send Status 2 (extended data)
    ModuleStatus2Message status2;
    populateStatus2Message(&status2);
    CANSendMessage(ECANMessageType_ModuleStatus2, 
                   (uint8_t*)&status2, sizeof(status2));
    
    // Send Status 3 (diagnostic data) - less frequently
    static uint8_t status3Counter = 0;
    if (++status3Counter >= 5) {  // Every 5 seconds
        status3Counter = 0;
        ModuleStatus3Message status3;
        populateStatus3Message(&status3);
        CANSendMessage(ECANMessageType_ModuleStatus3, 
                       (uint8_t*)&status3, sizeof(status3));
    }
}
```

### Command Processing

```c
void CANMessageReceived(ECANMessageType eType, uint8_t* pu8Data, uint8_t u8DataLen) {
    switch (eType) {
        case ECANMessageType_ModuleRegistration:
            processRegistrationMessage((ModuleRegistrationMessage*)pu8Data);
            break;
            
        case ECANMessageType_ModuleStateChangeRequest:
            processStateChangeRequest((StateChangeRequestMessage*)pu8Data);
            break;
            
        case ECANMessageType_ModuleCellDetailRequest:
            processCellDetailRequest((CellDetailRequestMessage*)pu8Data);
            break;
            
        case ECANMessageType_SetTime:
            processTimeSync((TimeSynchronizationMessage*)pu8Data);
            break;
            
        case ECANMessageType_AllDeRegister:
            processDeregistration();
            break;
            
        case ECANMessageType_AllIsolate:
            processEmergencyIsolation();
            break;
            
        default:
            // Unknown message type
            incrementUnknownMessageCounter();
            break;
    }
    
    // Update last communication time
    g_lastPackControllerContact = getSystemTicks();
}

void processStateChangeRequest(StateChangeRequestMessage* msg) {
    // Validate target module ID
    if (msg->targetModuleId != g_moduleId && msg->targetModuleId != 0xFF) {
        return; // Not intended for this module
    }
    
    // Validate state transition
    if (validateStateTransition(g_currentState, msg->requestedState)) {
        // Execute state change
        g_requestedState = msg->requestedState;
        g_stateChangeRequested = true;
        
        // Send acknowledgement if required
        if (msg->acknowledgementRequired) {
            sendStateChangeAcknowledgement(msg->requestedState, true);
        }
    } else {
        // Invalid state transition
        logStateTransitionError(g_currentState, msg->requestedState);
        
        if (msg->acknowledgementRequired) {
            sendStateChangeAcknowledgement(msg->requestedState, false);
        }
    }
}
```

## Data Encoding

### Voltage Encoding
```c
// Voltage encoding: mV direct representation
// Range: 0-65,535 mV (0V to 65.535V)
// Resolution: 1 mV

uint16_t encodeVoltage(uint16_t millivolts) {
    return millivolts;  // Direct encoding
}

uint16_t decodeVoltage(uint16_t encoded) {
    return encoded;     // Direct decoding
}
```

### Temperature Encoding
```c
// Temperature encoding: Offset binary with 0.5°C resolution
// Range: -40°C to +87.5°C
// Encoding: (temperature + 40) * 2
// Example: 25°C → (25 + 40) * 2 = 130

uint8_t encodeTemperature(float temperatureCelsius) {
    int16_t offset = (int16_t)((temperatureCelsius + 40.0f) * 2.0f);
    if (offset < 0) offset = 0;
    if (offset > 255) offset = 255;
    return (uint8_t)offset;
}

float decodeTemperature(uint8_t encoded) {
    return ((float)encoded / 2.0f) - 40.0f;
}
```

### Current Encoding
```c
// Current encoding: Signed 16-bit with 1 mA resolution
// Range: -32,768 mA to +32,767 mA
// Positive: Charge current
// Negative: Discharge current

int16_t encodeCurrent(float amperes) {
    int32_t milliamps = (int32_t)(amperes * 1000.0f);
    if (milliamps < -32768) milliamps = -32768;
    if (milliamps > 32767) milliamps = 32767;
    return (int16_t)milliamps;
}

float decodeCurrent(int16_t encoded) {
    return (float)encoded / 1000.0f;
}
```

## State Management

### State Transition Matrix

```c
bool validateStateTransition(ModuleOperatingState from, ModuleOperatingState to) {
    switch (from) {
        case MODULE_STATE_OFF:
            return (to == MODULE_STATE_STANDBY);
            
        case MODULE_STATE_STANDBY:
            return (to == MODULE_STATE_OFF || 
                    to == MODULE_STATE_PRECHARGE);
            
        case MODULE_STATE_PRECHARGE:
            return (to == MODULE_STATE_STANDBY || 
                    to == MODULE_STATE_ON);
            
        case MODULE_STATE_ON:
            return (to == MODULE_STATE_STANDBY || 
                    to == MODULE_STATE_OFF);
            
        default:
            return false;
    }
}

void executeStateTransition(ModuleOperatingState newState) {
    ModuleOperatingState previousState = g_currentState;
    
    // Execute state-specific actions
    switch (newState) {
        case MODULE_STATE_OFF:
            // Disable all balancing
            disableAllBalancing();
            // Stop data logging
            stopDataLogging();
            // Reduce power consumption
            enterLowPowerMode();
            break;
            
        case MODULE_STATE_STANDBY:
            // Enable monitoring only
            enableCellMonitoring();
            // Disable balancing
            disableAllBalancing();
            // Resume normal power mode
            exitLowPowerMode();
            break;
            
        case MODULE_STATE_PRECHARGE:
            // Prepare for operation
            performPrechargeSequence();
            // Start enhanced monitoring
            enableEnhancedMonitoring();
            break;
            
        case MODULE_STATE_ON:
            // Enable full operation
            enableAllFeatures();
            // Start balancing if needed
            enableCellBalancing();
            // Start data logging
            startDataLogging();
            break;
    }
    
    // Update state
    g_currentState = newState;
    
    // Log state change
    logStateChange(previousState, newState);
    
    // Notify pack controller
    sendStateChangeNotification(newState);
}
```

## Error Handling

### Communication Timeout Handling

```c
void checkPackControllerTimeout(void) {
    uint32_t timeSinceLastContact = getSystemTicks() - g_lastPackControllerContact;
    
    if (timeSinceLastContact > PACK_CONTROLLER_TIMEOUT_MS) {
        // Pack controller communication timeout
        g_packControllerTimeout = true;
        
        // Take protective action
        if (g_currentState == MODULE_STATE_ON) {
            // Transition to standby for safety
            executeStateTransition(MODULE_STATE_STANDBY);
        }
        
        // Continue attempting to reestablish communication
        sendModuleAnnouncement();
        
        // Log timeout event
        logCommunicationTimeout();
    }
}

void handleCANBusError(CANErrorType error) {
    switch (error) {
        case CAN_ERROR_BUS_OFF:
            // Bus-off condition - reinitialize CAN controller
            CANReinitialize();
            incrementBusOffCounter();
            break;
            
        case CAN_ERROR_ERROR_PASSIVE:
            // Error passive state - monitor error counters
            monitorErrorCounters();
            break;
            
        case CAN_ERROR_STUFF_ERROR:
        case CAN_ERROR_CRC_ERROR:
        case CAN_ERROR_ACK_ERROR:
            // Frame-level errors - increment counters
            incrementFrameErrorCounter(error);
            break;
            
        case CAN_ERROR_BUFFER_OVERFLOW:
            // Message buffer overflow
            clearReceiveBuffers();
            incrementOverflowCounter();
            break;
    }
    
    // Update error statistics
    updateCANErrorStatistics(error);
}
```

### Message Validation

```c
bool validateCANMessage(ECANMessageType eType, uint8_t* pu8Data, uint8_t u8DataLen) {
    // Check message length
    uint8_t expectedLength = getExpectedMessageLength(eType);
    if (u8DataLen != expectedLength) {
        incrementLengthErrorCounter();
        return false;
    }
    
    // Validate message-specific content
    switch (eType) {
        case ECANMessageType_ModuleRegistration:
            return validateRegistrationMessage((ModuleRegistrationMessage*)pu8Data);
            
        case ECANMessageType_ModuleStateChangeRequest:
            return validateStateChangeMessage((StateChangeRequestMessage*)pu8Data);
            
        case ECANMessageType_SetTime:
            return validateTimeSyncMessage((TimeSynchronizationMessage*)pu8Data);
            
        default:
            return true; // Basic validation passed
    }
}

bool validateRegistrationMessage(ModuleRegistrationMessage* msg) {
    // Check assigned module ID range
    if (msg->assignedModuleId > 31) {
        return false;
    }
    
    // Check reporting intervals
    if (msg->statusReportingInterval < 100 || 
        msg->statusReportingInterval > 60000) {
        return false;
    }
    
    // Check feature flags
    if (msg->enabledFeatures & ~SUPPORTED_FEATURES_MASK) {
        return false;
    }
    
    return true;
}
```

## Performance Characteristics

### Bus Utilization Analysis

```
CAN Bus Utilization (250 kbps):
├── Module Status Messages (per module)
│   ├── Status 1: 8 bytes @ 1 Hz = 704 bits/s
│   ├── Status 2: 8 bytes @ 1 Hz = 704 bits/s  
│   ├── Status 3: 8 bytes @ 0.2 Hz = 141 bits/s
│   └── Subtotal per module: 1,549 bits/s
├── Cell Detail Messages (per module)
│   ├── Rate: Variable, ~0.2 Hz typical
│   ├── Size: 8 bytes per message
│   └── Utilization: ~141 bits/s per module
├── Control Messages (pack controller)
│   ├── Registration: Occasional
│   ├── State changes: <1 Hz
│   ├── Time sync: ~0.1 Hz
│   └── Utilization: ~100 bits/s total
└── Total for 32 modules: ~54 kbps (21.6% of bus capacity)
```

### Message Timing Requirements

```
Real-Time Requirements:
├── Critical Status Updates: 1 second maximum
├── State Change Commands: 100ms response time
├── Emergency Isolation: <50ms propagation
├── Cell Detail Data: 5 second update cycle
├── Keep-alive Messages: 10 second intervals
└── Timeout Detection: 11.1 second limit
```

The CAN communication system provides robust, real-time communication between module controllers and the pack controller, enabling comprehensive battery management with deterministic performance and comprehensive error handling.