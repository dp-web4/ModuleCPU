# Storage and Data Logging System

## Table of Contents

1. [Overview](#overview)
2. [Storage Architecture](#storage-architecture)
3. [SD Card Management](#sd-card-management)
4. [EEPROM Configuration](#eeprom-configuration)
5. [Data Structures](#data-structures)
6. [Session Management](#session-management)
7. [Real-Time Clock](#real-time-clock)
8. [Performance Characteristics](#performance-characteristics)

## Overview

The ModuleCPU implements a comprehensive storage and data logging system that provides persistent storage for configuration parameters, real-time data logging, and historical data analysis. The system uses multiple storage technologies optimized for different data types and access patterns.

### Key Features
- **Multi-Tier Storage**: EEPROM for configuration, SD card for bulk data logging
- **Session-Based Logging**: Organized data storage with session management
- **Real-Time Timestamping**: MCP7940N RTC for accurate time synchronization
- **Data Integrity**: Checksums, validation, and error recovery mechanisms
- **Efficient Organization**: Sector-based storage with metadata management

### Storage Specifications
```
Storage System Specifications:
├── EEPROM (Internal)
│   ├── Capacity: 2KB (2,048 bytes)
│   ├── Usage: Configuration parameters
│   ├── Endurance: 100,000 write cycles
│   └── Access Time: Single byte read/write
├── SD Card (External)
│   ├── Capacity: Up to 32GB (SDHC)
│   ├── Usage: Data logging and historical storage
│   ├── Interface: SPI (up to 25MHz)
│   └── File System: Raw sector access
└── RTC (MCP7940N)
    ├── Accuracy: ±20ppm (±1.7 minutes/month)
    ├── Battery Backup: Coin cell or supercapacitor
    ├── Features: Alarm, square wave output
    └── Interface: I2C (400kHz)
```

## Storage Architecture

### Hierarchical Storage Design

```
┌─────────────────────────────────────────────────────────────────┐
│                    Storage Hierarchy                            │
├─────────────────────────────────────────────────────────────────┤
│ Configuration Storage (EEPROM - 2KB)                           │
│ ├── Module Identification                                      │
│ ├── Operating Parameters                                       │
│ ├── Calibration Data                                          │
│ └── Fault History                                             │
├─────────────────────────────────────────────────────────────────┤
│ Volatile Storage (SRAM - 4KB)                                  │
│ ├── Current Frame Data                                         │
│ ├── Cell Data Arrays                                          │
│ ├── System Statistics                                         │
│ └── Communication Buffers                                     │
├─────────────────────────────────────────────────────────────────┤
│ Bulk Storage (SD Card - up to 32GB)                           │
│ ├── Session-Based Data Logging                                │
│ ├── Historical Cell Data                                      │
│ ├── Event Logs                                               │
│ └── Diagnostic Information                                    │
└─────────────────────────────────────────────────────────────────┘
```

### Data Flow Architecture

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Cell      │    │   Frame     │    │   Session   │    │   SD Card   │
│ Monitoring  │───▶│    Data     │───▶│ Management  │───▶│   Storage   │
│             │    │ Assembly    │    │             │    │             │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
       │                   │                   │                   │
       ▼                   ▼                   ▼                   ▼
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  Real-Time  │    │ Aggregated  │    │  Timestamped│    │ Persistent  │
│    Data     │    │   Frame     │    │   Session   │    │   Archive   │
│             │    │    Data     │    │    Data     │    │             │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
```

## SD Card Management

### Sector-Based Storage System

The ModuleCPU uses a raw sector-based approach for optimal performance and deterministic behavior:

```c
// SD Card storage organization
#define SECTOR_SIZE                      512    // Standard SD card sector size
#define SECTORS_PER_FRAME               ((sizeof(FrameData) + SECTOR_SIZE - 1) / SECTOR_SIZE)
#define FRAME_BUFFER_SIZE               (SECTORS_PER_FRAME * SECTOR_SIZE)

// Storage layout constants
#define GLOBAL_STATE_SECTOR             0       // Sector 0: Global state information
#define SESSION_MAP_START_SECTOR        1       // Start of session mapping area
#define DATA_START_SECTOR               100     // Start of actual data storage
```

### SD Card Interface Implementation

```c
// SD Card initialization and management
bool SDInit(void) {
    // Initialize SPI interface
    SPIInit(SPI_MODE_0, SPI_CLOCK_DIV_8);  // ~3MHz for initialization
    
    // SD Card initialization sequence
    if (!sdCardReset()) {
        return false;
    }
    
    if (!sdCardInitialize()) {
        return false;
    }
    
    // Switch to high-speed mode after initialization
    SPISetClockDivider(SPI_CLOCK_DIV_2);   // ~12MHz for data transfer
    
    // Verify card capacity and characteristics
    uint32_t sectorCount, blockSize;
    if (!SDGetSectorCount(&sectorCount) || !SDGetBlockSize(&blockSize)) {
        return false;
    }
    
    // Initialize storage management system
    return STORE_Init();
}

// Raw sector read operation
bool SDRead(uint32_t u32Sector, uint8_t *pu8Buffer, uint32_t u32SectorCount) {
    // Validate parameters
    if (!pu8Buffer || u32SectorCount == 0) {
        return false;
    }
    
    // Send CMD17 (single block read) or CMD18 (multiple block read)
    uint8_t command = (u32SectorCount == 1) ? CMD17 : CMD18;
    
    if (!sdSendCommand(command, u32Sector)) {
        return false;
    }
    
    // Read data blocks
    for (uint32_t i = 0; i < u32SectorCount; i++) {
        if (!sdReadDataBlock(pu8Buffer + (i * SECTOR_SIZE))) {
            return false;
        }
    }
    
    // Stop transmission if multiple blocks
    if (u32SectorCount > 1) {
        sdSendCommand(CMD12, 0);
    }
    
    return true;
}

// Raw sector write operation  
bool SDWrite(uint32_t u32Sector, uint8_t *pu8Buffer, uint32_t u32SectorCount) {
    // Validate parameters
    if (!pu8Buffer || u32SectorCount == 0) {
        return false;
    }
    
    // Send CMD24 (single block write) or CMD25 (multiple block write)
    uint8_t command = (u32SectorCount == 1) ? CMD24 : CMD25;
    
    if (!sdSendCommand(command, u32Sector)) {
        return false;
    }
    
    // Write data blocks
    for (uint32_t i = 0; i < u32SectorCount; i++) {
        if (!sdWriteDataBlock(pu8Buffer + (i * SECTOR_SIZE))) {
            return false;
        }
        
        // Wait for write completion
        if (!sdWaitForWriteCompletion()) {
            return false;
        }
    }
    
    // Stop transmission if multiple blocks
    if (u32SectorCount > 1) {
        sdSendCommand(CMD12, 0);
    }
    
    return true;
}
```

### Storage Management System

```c
// Global state structure (stored in sector 0)
typedef struct __attribute__((aligned(4))) {
    uint64_t lastUpdateTimestamp;              // Last system update time
    uint32_t checksum;                         // Data integrity checksum
    uint32_t firstSessionSector;               // First session data sector
    uint32_t lastSessionSector;                // Last session data sector
    uint32_t sessionCount;                     // Total number of sessions
    uint32_t newSessionSector;                 // Next available sector
    uint32_t activeSessionMapSector;           // Session mapping sector
    uint32_t activeSessionMapOffset;           // Offset within mapping sector
    uint8_t cellDataDescriptor[16];            // Cell data format descriptor
    uint8_t frameDataDescriptor[32];           // Frame data format descriptor
    uint8_t cellCount;                         // Number of cells in module
    uint8_t cellStructuresPerFrame;            // Cells per frame
    uint8_t lifetimeStats[384];                // Lifetime statistics
} GlobalState;

bool STORE_Init(void) {
    GlobalState globalState;
    
    // Read global state from sector 0
    if (!SDRead(GLOBAL_STATE_SECTOR, (uint8_t*)&globalState, 1)) {
        // Initialize new storage system
        memset(&globalState, 0, sizeof(globalState));
        globalState.firstSessionSector = DATA_START_SECTOR;
        globalState.newSessionSector = DATA_START_SECTOR;
        globalState.cellCount = g_u8ActiveCellCount;
        
        // Calculate checksum
        globalState.checksum = calculateChecksum(&globalState, 
                                               sizeof(globalState) - sizeof(uint32_t));
        
        // Write initial global state
        if (!SDWrite(GLOBAL_STATE_SECTOR, (uint8_t*)&globalState, 1)) {
            return false;
        }
    } else {
        // Validate existing global state
        uint32_t calculatedChecksum = calculateChecksum(&globalState, 
                                                      sizeof(globalState) - sizeof(uint32_t));
        if (calculatedChecksum != globalState.checksum) {
            // Corrupted global state - attempt recovery
            return recoverGlobalState();
        }
    }
    
    // Initialize session management
    g_currentSession.startSector = globalState.newSessionSector;
    g_currentSession.frameCount = 0;
    g_currentSession.active = false;
    
    return true;
}
```

## EEPROM Configuration

### Configuration Parameter Storage

```c
// EEPROM memory map
#define EEPROM_SIZE                     2048    // 2KB total capacity

// Parameter addresses
#define EEPROM_UNIQUE_ID                0x0000  // Module unique identifier
#define EEPROM_EXPECTED_CELL_COUNT      0x0004  // Expected number of cells
#define EEPROM_MAX_CHARGE_CURRENT       0x0005  // Maximum charge current limit
#define EEPROM_MAX_DISCHARGE_CURRENT    0x0007  // Maximum discharge current limit
#define EEPROM_SEQUENTIAL_COUNT_MISMATCH 0x0009 // Error counter
#define EEPROM_CALIBRATION_DATA         0x0010  // ADC calibration data
#define EEPROM_FAULT_HISTORY            0x0100  // Fault log storage

// Configuration data structure
typedef struct {
    uint32_t uniqueId;                         // Module unique identifier
    uint8_t  expectedCellCount;                // Expected number of cells
    uint16_t maxChargeCurrent;                 // Maximum charge current (mA)
    uint16_t maxDischargeCurrent;              // Maximum discharge current (mA)
    uint16_t balanceThreshold;                 // Cell balance threshold (mV)
    uint16_t overVoltageLimit;                 // Overvoltage protection (mV)
    uint16_t underVoltageLimit;                // Undervoltage protection (mV)
    int16_t  overTemperatureLimit;             // Overtemperature limit (°C * 10)
    int16_t  underTemperatureLimit;            // Undertemperature limit (°C * 10)
    uint32_t configurationCRC;                 // Configuration integrity check
} ModuleConfiguration;

// EEPROM access functions
void EEPROMWrite(uint16_t u16Address, uint8_t u8Data) {
    // Wait for previous write to complete
    while (EECR & (1 << EEPE));
    
    // Set address and data registers
    EEAR = u16Address;
    EEDR = u8Data;
    
    // Write sequence (atomic operation)
    cli();                                     // Disable interrupts
    EECR |= (1 << EEMPE);                     // Enable master write
    EECR |= (1 << EEPE);                      // Start write
    sei();                                     // Re-enable interrupts
}

uint8_t EEPROMRead(uint16_t u16Address) {
    // Wait for previous write to complete
    while (EECR & (1 << EEPE));
    
    // Set address register
    EEAR = u16Address;
    
    // Start read
    EECR |= (1 << EERE);
    
    // Return data
    return EEDR;
}

// Configuration management
bool loadConfiguration(ModuleConfiguration* config) {
    uint8_t* configBytes = (uint8_t*)config;
    
    // Read configuration from EEPROM
    for (int i = 0; i < sizeof(ModuleConfiguration) - sizeof(uint32_t); i++) {
        configBytes[i] = EEPROMRead(EEPROM_UNIQUE_ID + i);
    }
    
    // Validate configuration CRC
    uint32_t calculatedCRC = calculateCRC32(configBytes, 
                                          sizeof(ModuleConfiguration) - sizeof(uint32_t));
    uint32_t storedCRC = 0;
    for (int i = 0; i < sizeof(uint32_t); i++) {
        storedCRC |= (EEPROMRead(EEPROM_UNIQUE_ID + 
                                sizeof(ModuleConfiguration) - sizeof(uint32_t) + i) << (i * 8));
    }
    
    if (calculatedCRC != storedCRC) {
        // Load default configuration
        loadDefaultConfiguration(config);
        saveConfiguration(config);
        return false;
    }
    
    return true;
}
```

## Data Structures

### Frame Data Structure

The core data logging structure captures comprehensive module state:

```c
#define FRAME_VALID_SIG                 0xBA77DA7A  // Frame validity signature
#define MAX_CELLS                       108         // Maximum cells per module

// Cell data structure (4-byte aligned for efficient transfer)
typedef struct __attribute__((aligned(4))) {
    uint16_t voltage;                          // Cell voltage in millivolts
    int16_t  temperature;                      // Cell temperature (°C * 10)
} CellData;

// ADC reading structure
typedef struct {
    bool     bValid;                           // Reading validity flag
    uint16_t u16Reading;                       // Raw ADC value
} SADCReading;

// Complete frame data structure (4-byte aligned)
typedef struct __attribute__((aligned(4))) {
    // Frame validity and metadata
    uint32_t validSig;                         // Frame validity signature
    uint16_t frameBytes;                       // Actual frame size in bytes
    uint64_t timestamp;                        // RTC timestamp
    
    // Session-persistent variables
    uint8_t  sg_u8WDTCount;                   // Watchdog reset count
    uint8_t  sg_u8CellCPUCountFewest;         // Minimum cells detected
    uint8_t  sg_u8CellCPUCountMost;           // Maximum cells detected
    uint8_t  sg_u8CellCountExpected;          // Expected cell count
    
    // Current measurements (session statistics)
    uint16_t u16maxCurrent;                    // Maximum current (0.02A units)
    uint16_t u16minCurrent;                    // Minimum current (0.02A units)
    uint16_t u16avgCurrent;                    // Average current (0.02A units)
    uint8_t  sg_u8CurrentBufferIndex;         // Current buffer index
    
    // Voltage measurements (for validation)
    int32_t  sg_i32VoltageStringMin;          // Minimum string voltage (mV)
    int32_t  sg_i32VoltageStringMax;          // Maximum string voltage (mV)
    int16_t  sg_i16VoltageStringPerADC;       // Voltage per ADC count
    
    // Frame-specific variables (reset each frame)
    bool     bDischargeOn;                     // Discharge state
    uint16_t sg_u16CellCPUI2CErrors;          // Cell communication errors
    uint8_t  sg_u8CellFirstI2CError;          // First error cell index
    uint16_t sg_u16BytesReceived;             // Total bytes received
    uint8_t  sg_u8CellCPUCount;               // Active cell count
    uint8_t  sg_u8MCRXFramingErrors;          // RX framing errors
    
    // Processed data (calculated at frame start)
    uint16_t u16frameCurrent;                  // Current frame current
    int16_t  sg_s16HighestCellTemp;           // Highest cell temperature
    int16_t  sg_s16LowestCellTemp;            // Lowest cell temperature
    int16_t  sg_s16AverageCellTemp;           // Average cell temperature
    uint16_t sg_u16HighestCellVoltage;        // Highest cell voltage (mV)
    uint16_t sg_u16LowestCellVoltage;         // Lowest cell voltage (mV)
    uint16_t sg_u16AverageCellVoltage;        // Average cell voltage (mV)
    uint32_t sg_u32CellVoltageTotal;          // Total cell voltage (mV)
    
    int32_t  sg_i32VoltageStringTotal;        // Total string voltage (15mV units)
    
    // ADC readings array
    SADCReading ADCReadings[EADCTYPE_COUNT];   // All ADC channel readings
    
    // Cell data array
    CellData StringData[MAX_CELLS];            // Individual cell data
} FrameData;

// Compile-time size validation
#define SECTORS_PER_FRAME               ((sizeof(FrameData) + SECTOR_SIZE - 1) / SECTOR_SIZE)
#define FRAME_BUFFER_SIZE               (SECTORS_PER_FRAME * SECTOR_SIZE)

// Static assertion to ensure proper sizing
#define STATIC_ASSERT(COND,MSG) typedef char static_assertion_##MSG[(COND)?1:-1]
STATIC_ASSERT(FRAME_BUFFER_SIZE >= sizeof(FrameData), frame_buffer_too_small);
```

### Frame Data Processing

```c
void prepareFrameData(FrameData* frame) {
    // Set frame validity signature
    frame->validSig = FRAME_VALID_SIG;
    frame->frameBytes = sizeof(FrameData);
    
    // Get timestamp from RTC
    frame->timestamp = RTCGetCurrentTimestamp();
    
    // Copy current cell data
    for (int i = 0; i < g_u8ActiveCellCount; i++) {
        frame->StringData[i].voltage = g_cellData[i].voltage;
        frame->StringData[i].temperature = g_cellData[i].temperature;
    }
    
    // Copy ADC readings
    for (int i = 0; i < EADCTYPE_COUNT; i++) {
        frame->ADCReadings[i] = g_ADCReadings[i];
    }
    
    // Calculate aggregated statistics
    calculateFrameStatistics(frame);
    
    // Update session statistics
    updateSessionStatistics(frame);
}

void calculateFrameStatistics(FrameData* frame) {
    uint16_t maxVoltage = 0;
    uint16_t minVoltage = 65535;
    uint32_t voltageSum = 0;
    int16_t maxTemp = -32768;
    int16_t minTemp = 32767;
    int32_t tempSum = 0;
    
    uint8_t validCells = 0;
    
    // Calculate statistics from valid cell data
    for (int i = 0; i < g_u8ActiveCellCount; i++) {
        if (frame->StringData[i].voltage > 0) {  // Valid voltage reading
            validCells++;
            
            uint16_t voltage = frame->StringData[i].voltage;
            int16_t temperature = frame->StringData[i].temperature;
            
            // Voltage statistics
            if (voltage > maxVoltage) maxVoltage = voltage;
            if (voltage < minVoltage) minVoltage = voltage;
            voltageSum += voltage;
            
            // Temperature statistics
            if (temperature > maxTemp) maxTemp = temperature;
            if (temperature < minTemp) minTemp = temperature;
            tempSum += temperature;
        }
    }
    
    // Store calculated statistics
    if (validCells > 0) {
        frame->sg_u16HighestCellVoltage = maxVoltage;
        frame->sg_u16LowestCellVoltage = minVoltage;
        frame->sg_u16AverageCellVoltage = voltageSum / validCells;
        frame->sg_u32CellVoltageTotal = voltageSum;
        
        frame->sg_s16HighestCellTemp = maxTemp;
        frame->sg_s16LowestCellTemp = minTemp;
        frame->sg_s16AverageCellTemp = tempSum / validCells;
        
        frame->sg_u8CellCPUCount = validCells;
    }
}
```

## Session Management

### Session-Based Data Organization

```c
// Session management structure
typedef struct {
    uint32_t sessionId;                        // Unique session identifier
    uint32_t startSector;                      // Starting sector for session data
    uint32_t currentSector;                    // Current write position
    uint32_t frameCount;                       // Number of frames in session
    uint64_t sessionStartTime;                 // Session start timestamp
    bool     active;                           // Session active flag
} SessionInfo;

SessionInfo g_currentSession;

bool STORE_StartNewSession(void) {
    // End current session if active
    if (g_currentSession.active) {
        STORE_EndSession();
    }
    
    // Get timestamp for new session
    uint64_t currentTime = RTCGetCurrentTimestamp();
    
    // Initialize new session
    g_currentSession.sessionId = generateSessionId(currentTime);
    g_currentSession.startSector = findNextAvailableSector();
    g_currentSession.currentSector = g_currentSession.startSector;
    g_currentSession.frameCount = 0;
    g_currentSession.sessionStartTime = currentTime;
    g_currentSession.active = true;
    
    // Update global state
    GlobalState globalState;
    if (loadGlobalState(&globalState)) {
        globalState.sessionCount++;
        globalState.newSessionSector = g_currentSession.startSector;
        globalState.lastUpdateTimestamp = currentTime;
        
        saveGlobalState(&globalState);
    }
    
    // Log session start
    logSessionEvent(SESSION_START, g_currentSession.sessionId);
    
    return true;
}

bool STORE_WriteFrame(volatile FrameData* frame) {
    if (!g_currentSession.active) {
        // Start new session if none active
        if (!STORE_StartNewSession()) {
            return false;
        }
    }
    
    // Prepare frame data
    FrameData frameBuffer;
    memcpy(&frameBuffer, (void*)frame, sizeof(FrameData));
    prepareFrameData(&frameBuffer);
    
    // Align to sector boundary
    uint8_t sectorBuffer[FRAME_BUFFER_SIZE];
    memset(sectorBuffer, 0, FRAME_BUFFER_SIZE);
    memcpy(sectorBuffer, &frameBuffer, sizeof(FrameData));
    
    // Write frame to SD card
    bool writeSuccess = SDWrite(g_currentSession.currentSector, 
                               sectorBuffer, SECTORS_PER_FRAME);
    
    if (writeSuccess) {
        // Update session info
        g_currentSession.currentSector += SECTORS_PER_FRAME;
        g_currentSession.frameCount++;
        
        // Check for session size limits
        if (g_currentSession.frameCount >= MAX_FRAMES_PER_SESSION) {
            STORE_EndSession();
        }
    }
    
    return writeSuccess;
}

bool STORE_EndSession(void) {
    if (!g_currentSession.active) {
        return false;
    }
    
    // Mark session as inactive
    g_currentSession.active = false;
    
    // Update session metadata
    SessionMetadata metadata;
    metadata.sessionId = g_currentSession.sessionId;
    metadata.startSector = g_currentSession.startSector;
    metadata.frameCount = g_currentSession.frameCount;
    metadata.sessionStartTime = g_currentSession.sessionStartTime;
    metadata.sessionEndTime = RTCGetCurrentTimestamp();
    metadata.checksum = calculateSessionChecksum(&metadata);
    
    // Write session metadata
    writeSessionMetadata(&metadata);
    
    // Update global state
    GlobalState globalState;
    if (loadGlobalState(&globalState)) {
        globalState.lastSessionSector = g_currentSession.currentSector - 1;
        globalState.lastUpdateTimestamp = metadata.sessionEndTime;
        
        saveGlobalState(&globalState);
    }
    
    // Log session end
    logSessionEvent(SESSION_END, g_currentSession.sessionId);
    
    return true;
}
```

## Real-Time Clock

### MCP7940N RTC Interface

```c
// MCP7940N register definitions
#define MCP7940N_I2C_ADDRESS            0x6F    // 7-bit I2C address
#define MCP7940N_SECONDS_REG            0x00    // Seconds register
#define MCP7940N_MINUTES_REG            0x01    // Minutes register
#define MCP7940N_HOURS_REG              0x02    // Hours register
#define MCP7940N_DAY_REG                0x03    // Day of week register
#define MCP7940N_DATE_REG               0x04    // Date register
#define MCP7940N_MONTH_REG              0x05    // Month register
#define MCP7940N_YEAR_REG               0x06    // Year register
#define MCP7940N_CONTROL_REG            0x07    // Control register

// Control register bits
#define MCP7940N_ST_BIT                 (1 << 7) // Start oscillator bit
#define MCP7940N_12_24_BIT              (1 << 6) // 12/24 hour format bit
#define MCP7940N_VBATEN_BIT             (1 << 3) // Battery enable bit

// RTC management functions
bool RTCInit(void) {
    // Initialize I2C interface
    I2CInit();
    
    // Check if RTC is running
    uint8_t seconds = RTCReadRegister(MCP7940N_SECONDS_REG);
    if (!(seconds & MCP7940N_ST_BIT)) {
        // Start oscillator
        seconds |= MCP7940N_ST_BIT;
        RTCWriteRegister(MCP7940N_SECONDS_REG, seconds);
    }
    
    // Configure for 24-hour format
    uint8_t control = RTCReadRegister(MCP7940N_CONTROL_REG);
    control &= ~MCP7940N_12_24_BIT;    // Clear for 24-hour format
    control |= MCP7940N_VBATEN_BIT;    // Enable battery backup
    RTCWriteRegister(MCP7940N_CONTROL_REG, control);
    
    return true;
}

uint64_t RTCGetCurrentTimestamp(void) {
    RTCDateTime dateTime;
    
    // Read current time from RTC
    if (RTCReadDateTime(&dateTime)) {
        // Convert to Unix timestamp
        return convertToUnixTimestamp(&dateTime);
    }
    
    return 0; // Error reading RTC
}

bool RTCReadDateTime(RTCDateTime* dateTime) {
    uint8_t rtcData[7];
    
    // Read all time registers in one transaction
    if (!I2CReadMultiple(MCP7940N_I2C_ADDRESS, MCP7940N_SECONDS_REG, rtcData, 7)) {
        return false;
    }
    
    // Convert BCD to binary
    dateTime->seconds = bcdToBinary(rtcData[0] & 0x7F);
    dateTime->minutes = bcdToBinary(rtcData[1] & 0x7F);
    dateTime->hours = bcdToBinary(rtcData[2] & 0x3F);
    dateTime->dayOfWeek = bcdToBinary(rtcData[3] & 0x07);
    dateTime->date = bcdToBinary(rtcData[4] & 0x3F);
    dateTime->month = bcdToBinary(rtcData[5] & 0x1F);
    dateTime->year = bcdToBinary(rtcData[6]) + 2000;
    
    return true;
}

bool RTCSetDateTime(RTCDateTime* dateTime) {
    uint8_t rtcData[7];
    
    // Convert binary to BCD
    rtcData[0] = binaryToBcd(dateTime->seconds) | MCP7940N_ST_BIT;
    rtcData[1] = binaryToBcd(dateTime->minutes);
    rtcData[2] = binaryToBcd(dateTime->hours);
    rtcData[3] = binaryToBcd(dateTime->dayOfWeek);
    rtcData[4] = binaryToBcd(dateTime->date);
    rtcData[5] = binaryToBcd(dateTime->month);
    rtcData[6] = binaryToBcd(dateTime->year - 2000);
    
    // Write all time registers
    return I2CWriteMultiple(MCP7940N_I2C_ADDRESS, MCP7940N_SECONDS_REG, rtcData, 7);
}
```

## Performance Characteristics

### Storage Performance Analysis

```
Storage Performance Metrics:
├── EEPROM Access
│   ├── Read Time: 2-4 CPU cycles per byte
│   ├── Write Time: 3.3ms per byte (typical)
│   ├── Endurance: 100,000 write cycles
│   └── Usage: Configuration parameters only
├── SD Card Access
│   ├── Sequential Write: 2-10 MB/s (class dependent)
│   ├── Random Access: 0.5-2 MB/s
│   ├── Sector Write Time: 5-20ms typical
│   └── Power Consumption: 50-200mA during access
├── RTC Operations
│   ├── Read Time: 1-2ms (I2C transaction)
│   ├── Write Time: 1-2ms (I2C transaction)
│   ├── Accuracy: ±20ppm (±52 seconds/month)
│   └── Power Consumption: 1µA typical (battery backup)
└── Frame Logging Rate
    ├── Frame Size: ~1,200 bytes (3 sectors)
    ├── Write Time: 15-60ms per frame
    ├── Maximum Rate: 16-66 frames/second
    └── Typical Rate: 3.3 frames/second (300ms cycle)
```

### Memory Usage Analysis

```
Storage Memory Usage:
├── Frame Data Structure: 1,192 bytes
├── Global State: 512 bytes (1 sector)
├── Session Metadata: 64 bytes per session
├── Configuration Data: 64 bytes (EEPROM)
├── Buffer Memory: 1,536 bytes (3 sectors aligned)
└── Total RAM Usage: ~3KB for storage system
```

The storage and data logging system provides comprehensive data persistence with real-time logging capabilities, configuration management, and robust data integrity features. The system is optimized for deterministic operation while providing the flexibility needed for long-term data analysis and system diagnostics.