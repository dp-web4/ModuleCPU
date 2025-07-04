# Cell Monitoring and Balancing System

## Table of Contents

1. [Overview](#overview)
2. [Cell Monitoring Architecture](#cell-monitoring-architecture)
3. [ADC System](#adc-system)
4. [Cell Communication](#cell-communication)
5. [Balancing Control](#balancing-control)
6. [Safety Monitoring](#safety-monitoring)
7. [Data Processing](#data-processing)
8. [Performance Characteristics](#performance-characteristics)

## Overview

The ModuleCPU implements a sophisticated cell monitoring and balancing system capable of managing up to 94 individual lithium-ion cells. The system provides high-precision voltage and temperature monitoring with active balancing capabilities to maintain cell uniformity and maximize pack performance.

### Key Features
- **High-Resolution Monitoring**: 10-bit ADC with calibrated voltage measurements
- **Comprehensive Coverage**: Voltage, temperature, and balancing status for each cell
- **Active Balancing**: FET-based discharge balancing with configurable thresholds
- **Real-Time Operation**: 300ms update cycle for all cells
- **Safety Integration**: Continuous monitoring with fault detection and protection

### System Specifications
```
Cell Monitoring Specifications:
├── Maximum Cell Count: 94 cells per module
├── Voltage Resolution: 1mV (after calibration)
├── Voltage Range: 0V to 5.0V (cell range: 2.5V to 4.2V)
├── Temperature Resolution: 0.1°C equivalent
├── Update Rate: 300ms cycle (3.3Hz)
├── Measurement Accuracy: ±0.5% (after calibration)
└── Balancing Capability: 50mA to 200mA per cell
```

## Cell Monitoring Architecture

### System Block Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    Cell Monitoring System                       │
├─────────────────────────────────────────────────────────────────┤
│ Cell Array (1-94 cells)                                        │
│ ├── Cell CPU 1    ├── Cell CPU 2    ├── Cell CPU N            │
│ │   ├── Cell 1-4  │   ├── Cell 5-8  │   ├── Cell 90-94        │
│ │   ├── Voltage   │   ├── Voltage   │   ├── Voltage            │
│ │   ├── Temp      │   ├── Temp      │   ├── Temp               │
│ │   └── Balance   │   └── Balance   │   └── Balance            │
├─────────────────────────────────────────────────────────────────┤
│ Virtual UART Communication Bus                                  │
│ ├── Query Commands (ModuleCPU → Cell CPUs)                     │
│ └── Data Responses (Cell CPUs → ModuleCPU)                     │
├─────────────────────────────────────────────────────────────────┤
│ ModuleCPU Processing                                            │
│ ├── ADC System (voltage/current monitoring)                    │
│ ├── Data Aggregation and Statistics                            │
│ ├── Balance Decision Engine                                     │
│ ├── Safety Monitoring and Fault Detection                      │
│ └── Pack Controller Communication                               │
└─────────────────────────────────────────────────────────────────┘
```

### Data Flow Architecture

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Cell      │    │   Virtual   │    │   Data      │    │   Balance   │
│ Measurement │───▶│    UART     │───▶│ Processing  │───▶│   Control   │
│   (Cell     │    │ Communication│    │ & Analysis  │    │  Decision   │
│    CPUs)    │    │             │    │             │    │   Engine    │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
       │                                      │                    │
       ▼                                      ▼                    ▼
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  Individual │    │ Aggregated  │    │   Safety    │    │   Balance   │
│  Cell Data  │    │ Statistics  │    │ Monitoring  │    │ Commands    │
│             │    │             │    │             │    │             │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
```

## ADC System

### ADC Configuration and Calibration

```c
// ADC reference and precision configuration
#define CELL_VREF                    1.1       // Internal 1.1V reference
#define CELL_VOLTAGE_BITS            10        // 10-bit ADC resolution
#define ADC_MAX_VALUE                (1 << CELL_VOLTAGE_BITS)  // 1024

// Voltage divider network calibration
#define CELL_DIV_TOP                 90900     // Upper resistor (90.9kΩ)
#define CELL_DIV_BOTTOM              30100     // Lower resistor (30.1kΩ)
#define CELL_VOLTAGE_SCALE           (((float)CELL_DIV_BOTTOM) / \
                                     ((float)CELL_DIV_TOP + (float)CELL_DIV_BOTTOM))

// Measured calibration factor (accounts for component tolerances)
#define CELL_VOLTAGE_CAL             1.032

// Fixed-point arithmetic for efficiency
#define FIXED_POINT_SCALE            512       // 2^9 for fractional precision

// Pre-calculated conversion factor for runtime efficiency
#define VOLTAGE_CONVERSION_FACTOR    ((uint32_t)((CELL_VREF * 1000.0 / \
    CELL_VOLTAGE_SCALE * CELL_VOLTAGE_CAL * FIXED_POINT_SCALE) + 0.5))
```

### Voltage Measurement Process

```c
uint16_t convertADCToMillivolts(uint16_t adcReading) {
    // Convert raw ADC reading to millivolts using fixed-point arithmetic
    uint32_t millivolts = (adcReading * VOLTAGE_CONVERSION_FACTOR) / 
                         (ADC_MAX_VALUE * FIXED_POINT_SCALE);
    
    // Apply bounds checking
    if (millivolts > 5000) {        // Maximum 5.0V
        millivolts = 5000;
    }
    
    return (uint16_t)millivolts;
}

void processVoltageReading(uint8_t cellIndex, uint16_t adcValue) {
    // Convert to physical voltage
    uint16_t voltage = convertADCToMillivolts(adcValue);
    
    // Store in cell data structure
    g_cellData[cellIndex].voltage = voltage;
    g_cellData[cellIndex].lastUpdate = getSystemTicks();
    
    // Update running statistics
    updateVoltageStatistics(cellIndex, voltage);
    
    // Check safety limits
    checkVoltageLimits(cellIndex, voltage);
}
```

### Current Measurement with Filtering

```c
// Current measurement buffer for noise reduction
static int16_t sg_sCurrentBuffer[ADC_CURRENT_BUFFER_SIZE];
static uint8_t sg_u8CurrentBufferIndex = 0;

void processCurrentReading(uint16_t adcValue) {
    // Convert ADC reading to signed current (bidirectional)
    int16_t current = (int16_t)adcValue - ADC_ZERO_CURRENT_OFFSET;
    
    // Add to circular buffer
    sg_sCurrentBuffer[sg_u8CurrentBufferIndex] = current;
    sg_u8CurrentBufferIndex = (sg_u8CurrentBufferIndex + 1) % ADC_CURRENT_BUFFER_SIZE;
    
    // Calculate filtered average
    int32_t sum = 0;
    for (int i = 0; i < ADC_CURRENT_BUFFER_SIZE; i++) {
        sum += sg_sCurrentBuffer[i];
    }
    
    g_s16ModuleCurrent = (int16_t)(sum / ADC_CURRENT_BUFFER_SIZE);
    
    // Apply current-based safety checks
    checkCurrentLimits(g_s16ModuleCurrent);
}
```

## Cell Communication

### Virtual UART Protocol

The ModuleCPU communicates with individual cell controller ICs using a bit-banged UART protocol optimized for battery monitoring applications.

#### Protocol Specifications:
```c
// Virtual UART timing configuration
#define VUART_BIT_TICKS              50        // 50μs per bit (20kbps)
#define VUART_FRAME_BITS             10        // Start + 8 data + stop
#define VUART_CELL_TIMEOUT_MS        10        // Maximum response time per cell

// Cell response data structure
typedef struct {
    uint8_t voltage_low;                       // Voltage LSB
    uint8_t voltage_high;                      // Voltage MSB  
    uint8_t temperature;                       // Temperature reading
    uint8_t status_checksum;                   // Status flags + checksum
} CellResponse;
```

#### Communication State Machine:

```c
typedef enum {
    VUART_STATE_IDLE,
    VUART_STATE_START_BIT,
    VUART_STATE_DATA_BITS,
    VUART_STATE_STOP_BIT,
    VUART_STATE_COMPLETE
} VUARTState;

void vUARTStateMachine(void) {
    static VUARTState state = VUART_STATE_IDLE;
    static uint8_t bitCount = 0;
    static uint8_t rxByte = 0;
    
    switch (state) {
        case VUART_STATE_IDLE:
            if (detectStartBit()) {
                state = VUART_STATE_START_BIT;
                startBitTimer();
            }
            break;
            
        case VUART_STATE_START_BIT:
            if (bitTimerExpired()) {
                if (validateStartBit()) {
                    state = VUART_STATE_DATA_BITS;
                    bitCount = 0;
                    rxByte = 0;
                } else {
                    state = VUART_STATE_IDLE; // False start
                }
            }
            break;
            
        case VUART_STATE_DATA_BITS:
            if (bitTimerExpired()) {
                // Sample data bit
                if (readRXPin()) {
                    rxByte |= (1 << bitCount);
                }
                
                bitCount++;
                if (bitCount >= 8) {
                    state = VUART_STATE_STOP_BIT;
                }
            }
            break;
            
        case VUART_STATE_STOP_BIT:
            if (bitTimerExpired()) {
                if (validateStopBit()) {
                    processCellResponseByte(rxByte);
                    state = VUART_STATE_COMPLETE;
                } else {
                    // Frame error
                    handleFrameError();
                    state = VUART_STATE_IDLE;
                }
            }
            break;
            
        case VUART_STATE_COMPLETE:
            state = VUART_STATE_IDLE;
            break;
    }
}
```

### Cell Data Collection Process

```c
void collectCellData(void) {
    static uint8_t currentCell = 0;
    static uint32_t collectionStartTime = 0;
    
    if (currentCell == 0) {
        // Start new collection cycle
        collectionStartTime = getSystemTicks();
    }
    
    // Send query command to current cell
    sendCellQuery(currentCell);
    
    // Wait for response with timeout
    uint32_t responseTimeout = collectionStartTime + 
                               VUART_CELL_TIMEOUT_MS * (currentCell + 1);
    
    while (getSystemTicks() < responseTimeout) {
        vUARTStateMachine();
        
        if (g_cellData[currentCell].responseReceived) {
            // Process successful response
            processCellResponse(currentCell);
            break;
        }
    }
    
    // Check for timeout
    if (!g_cellData[currentCell].responseReceived) {
        handleCellTimeout(currentCell);
    }
    
    // Move to next cell
    currentCell++;
    if (currentCell >= g_u8ActiveCellCount) {
        currentCell = 0;
        // Collection cycle complete
        updateSystemStatistics();
    }
}

void processCellResponse(uint8_t cellIndex) {
    CellResponse* response = &g_cellResponses[cellIndex];
    
    // Validate checksum
    if (validateResponseChecksum(response)) {
        // Extract voltage (12-bit value)
        uint16_t voltage = ((uint16_t)response->voltage_high << 8) | 
                          response->voltage_low;
        
        // Convert to millivolts and store
        g_cellData[cellIndex].voltage = convertCellVoltage(voltage);
        
        // Extract temperature
        g_cellData[cellIndex].temperature = convertCellTemperature(response->temperature);
        
        // Extract status flags
        g_cellData[cellIndex].status = response->status_checksum & 0xF0;
        
        // Mark as successfully updated
        g_cellData[cellIndex].lastUpdate = getSystemTicks();
        g_cellData[cellIndex].responseReceived = true;
        
    } else {
        // Checksum error
        g_cellData[cellIndex].communicationErrors++;
        handleCommunicationError(cellIndex);
    }
}
```

## Balancing Control

### Balance Decision Algorithm

```c
// Balancing configuration parameters
#define BALANCE_VOLTAGE_THRESHOLD    0x40      // 64mV difference triggers balancing
#define CELL_BALANCE_DISCHARGE_THRESHOLD 0x0387 // 3.9V minimum for balancing
#define BALANCE_CURRENT_TARGET       100       // 100mA target discharge current

typedef struct {
    bool active;                               // Balancing currently active
    uint32_t startTime;                        // When balancing started
    uint32_t totalTime;                        // Total balancing time
    uint16_t targetVoltage;                    // Target voltage for this cell
} BalanceControl;

BalanceControl g_balanceControl[TOTAL_CELL_COUNT_MAX];

void updateCellBalancing(void) {
    // Only balance when module is in appropriate state
    if (g_eCurrentState != EMODSTATE_ON && g_eCurrentState != EMODSTATE_STANDBY) {
        // Disable all balancing
        disableAllBalancing();
        return;
    }
    
    // Calculate balancing requirements
    calculateBalancingNeeds();
    
    // Update individual cell balancing
    for (int i = 0; i < g_u8ActiveCellCount; i++) {
        updateCellBalance(i);
    }
    
    // Send balance commands to cell controllers
    sendBalanceCommands();
}

void calculateBalancingNeeds(void) {
    // Find voltage statistics
    uint16_t maxVoltage = 0;
    uint16_t minVoltage = 65535;
    uint32_t totalVoltage = 0;
    
    for (int i = 0; i < g_u8ActiveCellCount; i++) {
        uint16_t voltage = g_cellData[i].voltage;
        
        if (voltage > maxVoltage) maxVoltage = voltage;
        if (voltage < minVoltage) minVoltage = voltage;
        totalVoltage += voltage;
    }
    
    uint16_t averageVoltage = totalVoltage / g_u8ActiveCellCount;
    uint16_t voltageSpread = maxVoltage - minVoltage;
    
    // Check if balancing is needed
    if (voltageSpread > BALANCE_VOLTAGE_THRESHOLD) {
        // Calculate target voltage (slightly below average)
        uint16_t targetVoltage = averageVoltage - (BALANCE_VOLTAGE_THRESHOLD / 4);
        
        // Set balance targets for cells above target
        for (int i = 0; i < g_u8ActiveCellCount; i++) {
            if (g_cellData[i].voltage > targetVoltage + BALANCE_VOLTAGE_THRESHOLD) {
                // Cell needs balancing
                g_balanceControl[i].targetVoltage = targetVoltage;
                
                // Only balance if voltage is safe
                if (g_cellData[i].voltage > CELL_BALANCE_DISCHARGE_THRESHOLD) {
                    enableCellBalance(i);
                } else {
                    disableCellBalance(i);
                }
            } else {
                // Cell doesn't need balancing
                disableCellBalance(i);
            }
        }
    } else {
        // Voltage spread is acceptable, disable all balancing
        disableAllBalancing();
    }
}
```

### Balance Control Implementation

```c
void enableCellBalance(uint8_t cellIndex) {
    if (!g_balanceControl[cellIndex].active) {
        // Start new balancing session
        g_balanceControl[cellIndex].active = true;
        g_balanceControl[cellIndex].startTime = getSystemTicks();
        
        // Send enable command to cell controller
        sendCellBalanceCommand(cellIndex, BALANCE_ENABLE);
        
        // Log balancing start
        logBalanceEvent(cellIndex, BALANCE_EVENT_START);
    }
}

void disableCellBalance(uint8_t cellIndex) {
    if (g_balanceControl[cellIndex].active) {
        // End balancing session
        g_balanceControl[cellIndex].active = false;
        
        // Update total balancing time
        uint32_t sessionTime = getSystemTicks() - g_balanceControl[cellIndex].startTime;
        g_balanceControl[cellIndex].totalTime += sessionTime;
        
        // Send disable command to cell controller
        sendCellBalanceCommand(cellIndex, BALANCE_DISABLE);
        
        // Log balancing end
        logBalanceEvent(cellIndex, BALANCE_EVENT_END);
    }
}

void updateCellBalance(uint8_t cellIndex) {
    if (g_balanceControl[cellIndex].active) {
        // Check if target voltage reached
        if (g_cellData[cellIndex].voltage <= g_balanceControl[cellIndex].targetVoltage) {
            disableCellBalance(cellIndex);
            return;
        }
        
        // Check for safety conditions
        if (g_cellData[cellIndex].voltage < CELL_BALANCE_DISCHARGE_THRESHOLD) {
            disableCellBalance(cellIndex);
            triggerFault(FAULT_BALANCE_UNDERVOLTAGE);
            return;
        }
        
        // Check for maximum balancing time
        uint32_t sessionTime = getSystemTicks() - g_balanceControl[cellIndex].startTime;
        if (sessionTime > BALANCE_MAX_SESSION_TIME) {
            disableCellBalance(cellIndex);
            triggerFault(FAULT_BALANCE_TIMEOUT);
            return;
        }
        
        // Monitor balancing effectiveness
        checkBalancingEffectiveness(cellIndex);
    }
}
```

## Safety Monitoring

### Continuous Safety Checks

```c
void checkVoltageLimits(uint8_t cellIndex, uint16_t voltage) {
    // Overvoltage protection
    if (voltage > getParameter(PARAM_CELL_OVERVOLTAGE_LIMIT)) {
        triggerCellFault(cellIndex, CELL_FAULT_OVERVOLTAGE);
        
        // Immediate protective action
        disableCellBalance(cellIndex);
        if (g_eCurrentState == EMODSTATE_ON) {
            requestStateChange(EMODSTATE_STANDBY);
        }
    }
    
    // Undervoltage protection
    if (voltage < getParameter(PARAM_CELL_UNDERVOLTAGE_LIMIT)) {
        triggerCellFault(cellIndex, CELL_FAULT_UNDERVOLTAGE);
        
        // Stop any discharge operations
        disableCellBalance(cellIndex);
        if (g_s16ModuleCurrent < 0) { // Discharging
            requestStateChange(EMODSTATE_STANDBY);
        }
    }
    
    // Voltage rate of change monitoring
    checkVoltageRateOfChange(cellIndex, voltage);
}

void checkTemperatureLimits(uint8_t cellIndex, uint16_t temperature) {
    // Overtemperature protection
    if (temperature > getParameter(PARAM_CELL_OVERTEMP_LIMIT)) {
        triggerCellFault(cellIndex, CELL_FAULT_OVERTEMPERATURE);
        
        // Reduce thermal stress
        disableCellBalance(cellIndex);
        reduceOperatingCurrent();
    }
    
    // Undertemperature protection
    if (temperature < getParameter(PARAM_CELL_UNDERTEMP_LIMIT)) {
        triggerCellFault(cellIndex, CELL_FAULT_UNDERTEMPERATURE);
        
        // Restrict operations at low temperature
        restrictLowTemperatureOperation();
    }
}

void checkCommunicationTimeouts(void) {
    uint32_t currentTime = getSystemTicks();
    
    for (int i = 0; i < g_u8ActiveCellCount; i++) {
        // Check time since last successful communication
        uint32_t timeSinceUpdate = currentTime - g_cellData[i].lastUpdate;
        
        if (timeSinceUpdate > CELL_COMMUNICATION_TIMEOUT) {
            triggerCellFault(i, CELL_FAULT_COMMUNICATION);
            
            // Disable balancing for non-communicating cells
            disableCellBalance(i);
            
            // If too many cells have communication issues, go to safe state
            if (countCommunicationFaults() > MAX_COMMUNICATION_FAULTS) {
                requestStateChange(EMODSTATE_OFF);
            }
        }
    }
}
```

## Data Processing

### Statistical Analysis

```c
void updateSystemStatistics(void) {
    uint16_t maxVoltage = 0;
    uint16_t minVoltage = 65535;
    uint32_t voltageSum = 0;
    uint16_t maxTemp = 0;
    uint16_t minTemp = 65535;
    uint32_t tempSum = 0;
    
    uint8_t validCells = 0;
    
    // Calculate statistics from all valid cells
    for (int i = 0; i < g_u8ActiveCellCount; i++) {
        // Only include cells with recent valid data
        uint32_t timeSinceUpdate = getSystemTicks() - g_cellData[i].lastUpdate;
        if (timeSinceUpdate < CELL_DATA_VALIDITY_TIMEOUT) {
            validCells++;
            
            uint16_t voltage = g_cellData[i].voltage;
            uint16_t temperature = g_cellData[i].temperature;
            
            // Voltage statistics
            if (voltage > maxVoltage) {
                maxVoltage = voltage;
                g_systemStats.cellHighIndex = i;
            }
            if (voltage < minVoltage) {
                minVoltage = voltage;
                g_systemStats.cellLowIndex = i;
            }
            voltageSum += voltage;
            
            // Temperature statistics
            if (temperature > maxTemp) {
                maxTemp = temperature;
            }
            if (temperature < minTemp) {
                minTemp = temperature;
            }
            tempSum += temperature;
        }
    }
    
    // Update global statistics
    if (validCells > 0) {
        g_systemStats.voltageHigh = maxVoltage;
        g_systemStats.voltageLow = minVoltage;
        g_systemStats.voltageAverage = voltageSum / validCells;
        g_systemStats.temperatureHigh = maxTemp;
        g_systemStats.temperatureLow = minTemp;
        g_systemStats.temperatureAverage = tempSum / validCells;
        
        // Calculate derived parameters
        g_systemStats.voltageSpread = maxVoltage - minVoltage;
        g_systemStats.temperatureSpread = maxTemp - minTemp;
        
        // Update pack voltage estimate
        g_u16PackVoltage = voltageSum;
        
    } else {
        // No valid cell data available
        triggerFault(FAULT_NO_VALID_CELL_DATA);
    }
}
```

## Performance Characteristics

### Timing Analysis

```
Cell Monitoring Cycle Timing (300ms total):
├── Frame Setup and Initialization        │ 5ms
├── Cell Data Collection                   │ 250ms
│   ├── Cell Query Commands               │ 94 × 1ms = 94ms
│   ├── Response Processing               │ 94 × 1.5ms = 141ms
│   └── Timeout Handling                  │ 15ms
├── Data Processing and Statistics        │ 20ms
├── Balance Decision Calculation          │ 10ms
├── Safety Monitoring and Checks          │ 10ms
└── Communication and Logging             │ 5ms
Total Cycle Time                          │ 300ms
```

### Memory Usage

```
Memory Allocation:
├── Cell Data Array (94 cells × 8 bytes)     │ 752 bytes
├── Balance Control (94 cells × 12 bytes)    │ 1,128 bytes  
├── Communication Buffers                    │ 256 bytes
├── ADC Filtering Buffers                    │ 64 bytes
├── Statistics and State Variables           │ 128 bytes
├── Configuration Parameters                 │ 64 bytes
└── Stack and Local Variables                │ 512 bytes
Total RAM Usage                              │ ~2.9KB / 4KB available
```

The cell monitoring and balancing system provides comprehensive battery management with high precision, robust communication, and active safety monitoring. The system maintains deterministic operation while providing the flexibility needed for various battery chemistries and configurations.