# ATmega64M1 Hardware Platform

## Table of Contents

1. [Overview](#overview)
2. [Microcontroller Specifications](#microcontroller-specifications)
3. [Memory Architecture](#memory-architecture)
4. [Peripheral System](#peripheral-system)
5. [CAN Controller](#can-controller)
6. [Analog System](#analog-system)
7. [Power Management](#power-management)
8. [Pin Configuration](#pin-configuration)

## Overview

The ModuleCPU firmware runs on the ATmega64M1 microcontroller, a specialized 8-bit AVR designed for automotive and industrial applications. This platform provides integrated CAN communication, high-resolution ADC capabilities, and robust operation suitable for battery management systems.

### Key Hardware Advantages

**Integrated CAN Controller**
- Native CAN 2.0A/B support with hardware message filtering
- Up to 1 Mbps operation for high-speed communication
- 15 message objects with individual configuration
- Automatic error handling and bus-off recovery

**High-Resolution ADC**
- 12-bit successive approximation ADC
- 11 single-ended or 5 differential input channels
- Built-in gain amplifier for small signal measurement
- Low noise and high accuracy for precise cell monitoring

**Automotive-Grade Design**
- Extended temperature range (-40°C to +125°C)
- High noise immunity and EMI tolerance
- Robust I/O pins with ESD protection
- Automotive qualification (AEC-Q100)

## Microcontroller Specifications

### ATmega64M1 Core Features

```
ATmega64M1 Technical Specifications:
┌─────────────────────────────────────────────────────────────────┐
│                        Core Architecture                       │
├─────────────────────────────────────────────────────────────────┤
│ CPU:                  8-bit AVR Enhanced RISC Architecture     │
│ Operating Frequency:  Up to 16MHz (8MHz internal RC)           │
│ Performance:          16 MIPS at 16MHz                         │
│ Package:             64-pin TQFP (14mm x 14mm)                 │
│ Operating Voltage:    2.7V to 5.5V                            │
│ Operating Temperature: -40°C to +125°C (automotive grade)      │
├─────────────────────────────────────────────────────────────────┤
│                         Memory System                          │
├─────────────────────────────────────────────────────────────────┤
│ Flash Memory:         64KB (32K x 16-bit instructions)         │
│ SRAM:                4KB (data memory)                          │
│ EEPROM:              2KB (non-volatile storage)                │
│ Program Counter:      15-bit (addressing 32K instructions)     │
├─────────────────────────────────────────────────────────────────┤
│                      Instruction Set                           │
├─────────────────────────────────────────────────────────────────┤
│ Instructions:         131 powerful instructions                │
│ Execution:           Most instructions execute in 1 cycle      │
│ Addressing Modes:     13 different addressing modes            │
│ Multiply Unit:        8-bit x 8-bit → 16-bit (2 cycles)       │
└─────────────────────────────────────────────────────────────────┘
```

### Performance Characteristics

```c
// CPU performance metrics
#define CPU_FREQUENCY_MAX       16000000UL  // 16MHz maximum external
#define CPU_FREQUENCY_INTERNAL  8000000UL   // 8MHz internal RC (typical)
#define CPU_FREQUENCY_DEFAULT   8000000UL   // Default ModuleCPU setting

// Instruction timing
#define INSTRUCTION_CYCLE_NS    125         // 125ns per cycle @ 8MHz
#define MULTIPLY_CYCLES         2           // Hardware multiply timing
#define BRANCH_CYCLES           2           // Branch instruction timing
#define CALL_RETURN_CYCLES      4           // Function call/return timing

// Memory access times
#define FLASH_ACCESS_CYCLES     2           // Flash memory access
#define SRAM_ACCESS_CYCLES      1           // SRAM access
#define EEPROM_ACCESS_CYCLES    4           // EEPROM access (+ wait states)

// Interrupt response
#define INTERRUPT_LATENCY_MIN   4           // Minimum interrupt latency (cycles)
#define INTERRUPT_LATENCY_MAX   7           // Maximum interrupt latency (cycles)
#define CONTEXT_SAVE_CYCLES     20          // Context save/restore overhead
```

### Clock System Configuration

```c
// Clock system configuration for ModuleCPU
void SystemClock_Config(void) {
    // Configure internal 8MHz RC oscillator
    CLKPR = (1 << CLKPCE);          // Enable clock prescaler change
    CLKPR = 0x00;                   // No prescaling, full 8MHz
    
    // Alternative: External crystal configuration
    #ifdef USE_EXTERNAL_CRYSTAL
        // Configure for external 16MHz crystal
        CLKPR = (1 << CLKPCE);      // Enable clock prescaler change
        CLKPR = 0x01;               // Divide by 2, 8MHz from 16MHz crystal
    #endif
    
    // Configure peripheral clocks
    // Timer0 prescaler: /8 (1MHz timer clock)
    TCCR0B = (1 << CS01);
    
    // Timer1 prescaler: /256 (31.25kHz timer clock)
    TCCR1B = (1 << CS12);
    
    // CAN clock: derived from system clock
    // ADC clock: system clock / 64 (125kHz ADC clock)
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1);
}

// Precise timing functions
void DelayMicroseconds(uint16_t us) {
    // Precise delay using timer
    uint16_t timer_counts = (uint32_t)us * CPU_FREQUENCY_DEFAULT / 1000000UL;
    
    // Use Timer0 for short delays
    TCNT0 = 0;
    while (TCNT0 < (timer_counts / 8)) {
        // Wait for timer overflow
    }
}

void DelayMilliseconds(uint16_t ms) {
    for (uint16_t i = 0; i < ms; i++) {
        DelayMicroseconds(1000);
    }
}
```

## Memory Architecture

### Memory Map and Organization

```
ATmega64M1 Memory Map:
┌─────────────────────────────────────────────────────────┐ 0x0000
│                Flash Program Memory                     │
│                      (64KB)                            │
│  ┌─────────────────────────────────────────────────────┐ │ 0x0000
│  │              Interrupt Vector Table                 │ │ 0x002A
│  ├─────────────────────────────────────────────────────┤ │
│  │                Application Code                     │ │ 0x002A
│  │  • main() function and system initialization       │ │
│  │  • Cell monitoring and balancing algorithms        │ │
│  │  • CAN communication protocol handlers             │ │
│  │  • Data logging and storage functions              │ │
│  │  • Interrupt service routines                      │ │
│  ├─────────────────────────────────────────────────────┤ │ 0xE000
│  │              Configuration and Data                 │ │
│  │  • Factory default parameters                      │ │
│  │  • Calibration tables and constants                │ │
│  │  • String constants and lookup tables              │ │
│  └─────────────────────────────────────────────────────┘ │ 0xFFFF
├─────────────────────────────────────────────────────────┤
│                    SRAM Data Memory                     │ 0x0100
│                        (4KB)                           │
│  ┌─────────────────────────────────────────────────────┐ │ 0x0100
│  │              Register File + I/O                    │ │ 0x01FF
│  ├─────────────────────────────────────────────────────┤ │
│  │               Global Variables                      │ │ 0x0200
│  │  • Cell data arrays (94 cells max)                 │ │
│  │  • Module configuration structure                  │ │
│  │  • Communication buffers                           │ │
│  │  • Balancing control variables                     │ │
│  ├─────────────────────────────────────────────────────┤ │ 0x0800
│  │                  Heap Space                         │ │
│  │  • Dynamic memory allocation (if used)             │ │
│  │  • Temporary calculation buffers                   │ │
│  ├─────────────────────────────────────────────────────┤ │ 0x0C00
│  │                  Stack Space                        │ │
│  │  • Function call stack                              │ │
│  │  • Local variables                                  │ │
│  │  • Interrupt context storage                       │ │
│  └─────────────────────────────────────────────────────┘ │ 0x10FF
├─────────────────────────────────────────────────────────┤
│                  EEPROM Memory                          │ 0x0000
│                     (2KB)                              │
│  ┌─────────────────────────────────────────────────────┐ │ 0x0000
│  │              Configuration Header                   │ │ 0x0010
│  │  • Magic number and version information            │ │
│  │  • Configuration validity flags                    │ │
│  ├─────────────────────────────────────────────────────┤ │
│  │              Module Configuration                   │ │ 0x0010
│  │  • Module ID and hardware version                  │ │
│  │  • Cell count and operational parameters           │ │
│  │  • Voltage and temperature thresholds              │ │
│  │  • Balancing configuration                         │ │
│  ├─────────────────────────────────────────────────────┤ │ 0x0100
│  │              Calibration Data                       │ │
│  │  • ADC calibration coefficients                    │ │
│  │  • Temperature sensor calibration                  │ │
│  │  • Per-cell offset and gain corrections            │ │
│  ├─────────────────────────────────────────────────────┤ │ 0x0400
│  │              Factory Defaults                       │ │
│  │  • Original factory configuration                  │ │
│  │  • Fallback parameters                             │ │
│  │  • Manufacturing data                              │ │
│  ├─────────────────────────────────────────────────────┤ │ 0x0600
│  │              Fault History                          │ │
│  │  • Critical fault log                              │ │
│  │  • System reset history                            │ │
│  │  • Error counters                                  │ │
│  └─────────────────────────────────────────────────────┘ │ 0x07FF
└─────────────────────────────────────────────────────────┘
```

### Memory Usage Optimization

```c
// Memory allocation strategy for ModuleCPU
typedef struct {
    // Critical cell data (must be in fast access SRAM)
    CellData cells[TOTAL_CELL_COUNT_MAX];       // ~2.3KB for 94 cells
    
    // Module state and configuration (frequently accessed)
    ModuleConfiguration config;                  // ~512 bytes
    ModuleState current_state;                   // 4 bytes
    uint32_t system_time;                       // 4 bytes
    
    // Communication buffers
    CANMessage can_tx_buffer[8];                // ~144 bytes
    CANMessage can_rx_buffer[8];                // ~144 bytes
    uint8_t uart_debug_buffer[128];             // 128 bytes
    
    // Working memory for calculations
    uint16_t adc_readings[16];                  // 32 bytes
    uint8_t temp_calculation_buffer[64];        // 64 bytes
    
    // Total SRAM usage: ~3.4KB (85% of available 4KB)
} MemoryLayout;

// EEPROM layout definition
typedef struct {
    // Configuration header (16 bytes)
    uint32_t magic_number;                      // 0xDEADBEEF
    uint16_t version;                           // Configuration version
    uint16_t size;                              // Configuration size
    uint32_t checksum;                          // CRC32 checksum
    uint32_t timestamp;                         // Last update time
    
    // Module configuration (240 bytes)
    ModuleConfiguration module_config;
    
    // Calibration data (256 bytes)
    int16_t cell_voltage_offset[TOTAL_CELL_COUNT_MAX];  // Per-cell offset
    int16_t cell_temp_offset[TOTAL_CELL_COUNT_MAX];     // Per-cell temp offset
    uint16_t adc_reference_calibration;         // ADC reference voltage
    
    // Factory defaults (512 bytes)
    ModuleConfiguration factory_defaults;
    uint8_t manufacturing_data[256];            // Manufacturing test data
    
    // Fault history (1KB)
    FaultHistoryEntry fault_log[32];            // 32 fault entries
    uint16_t reset_count;                       // System reset counter
    uint16_t error_counters[16];                // Various error counters
    
    // Reserved space (remainder)
    uint8_t reserved[256];                      // Future expansion
    
} EEPROMLayout;

// Memory protection and validation
bool ValidateMemoryIntegrity(void) {
    // Check stack pointer validity
    uint16_t sp = SP;
    if (sp < 0x0200 || sp > 0x10FF) {
        return false;   // Stack pointer out of valid range
    }
    
    // Check for stack overflow
    extern uint8_t __heap_start;
    if (sp < (uint16_t)&__heap_start + 64) {
        return false;   // Stack collision with heap
    }
    
    // Validate critical data structures
    if (!ValidateConfigurationChecksum()) {
        return false;   // Configuration corrupted
    }
    
    return true;
}
```

## Peripheral System

### Timer/Counter System

```c
// Timer configuration for ModuleCPU
typedef struct {
    uint8_t timer_id;               // Timer number (0, 1)
    uint16_t prescaler;             // Clock prescaler value
    uint16_t compare_value;         // Compare match value
    bool overflow_interrupt;        // Enable overflow interrupt
    bool compare_interrupt;         // Enable compare match interrupt
    void (*callback)(void);         // Interrupt callback function
} TimerConfig;

// Timer0 configuration (8-bit timer for system tick)
void Timer0_Init(void) {
    // Configure Timer0 for 100ms periodic interrupt
    TCCR0A = (1 << WGM01);          // CTC mode
    TCCR0B = (1 << CS01);           // Prescaler /8
    OCR0A = PERIODIC_COMPARE_A_RELOAD;  // Compare value for 100ms
    TIMSK0 = (1 << OCIE0A);         // Enable compare interrupt
}

// Timer0 interrupt service routine
ISR(TIMER0_COMPA_vect) {
    static uint16_t tick_counter = 0;
    
    // Increment system tick counter
    tick_counter++;
    
    // Set periodic task flag every 300ms (3 ticks)
    if (tick_counter >= PERIODIC_CALLBACK_RATE_TICKS) {
        tick_counter = 0;
        periodicTaskFlag = true;
        
        // Toggle frame type for alternating read/write operations
        currentFrameType = (currentFrameType == EFRAMETYPE_READ) ? 
                          EFRAMETYPE_WRITE : EFRAMETYPE_READ;
    }
    
    // Refresh watchdog timer
    #ifdef WDT_ENABLE
        wdt_reset();
    #endif
}

// Timer1 configuration (16-bit timer for precise timing)
void Timer1_Init(void) {
    // Configure Timer1 for virtual UART bit timing
    TCCR1A = 0;                     // Normal mode
    TCCR1B = (1 << CS12);           // Prescaler /256
    TIMSK1 = (1 << TOIE1);          // Enable overflow interrupt
}

// Precise delay using Timer1
void DelayPrecise(uint16_t microseconds) {
    uint16_t timer_counts = (uint32_t)microseconds * 
                           (CPU_FREQUENCY_DEFAULT / 256) / 1000000UL;
    
    TCNT1 = 0xFFFF - timer_counts;  // Load timer for desired delay
    TIFR1 = (1 << TOV1);            // Clear overflow flag
    
    while (!(TIFR1 & (1 << TOV1))); // Wait for overflow
}
```

### SPI Interface

```c
// SPI configuration for SD card interface
typedef struct {
    uint8_t mode;                   // SPI mode (0-3)
    uint8_t bit_order;              // MSB or LSB first
    uint8_t clock_divider;          // SPI clock divider
    bool interrupt_enabled;         // SPI interrupt enable
} SPIConfig;

void SPI_Init(void) {
    // Configure SPI pins
    DDRB |= (1 << PB2) | (1 << PB1) | (1 << PB0);  // MOSI, SCK, SS as outputs
    DDRB &= ~(1 << PB3);                            // MISO as input
    
    // Configure SPI control register
    SPCR = (1 << SPE) |             // Enable SPI
           (1 << MSTR) |            // Master mode
           (1 << SPR1);             // Clock rate fck/64 (125kHz @ 8MHz)
    
    // Set SS high (deselect all devices)
    PORTB |= (1 << PB0);
}

uint8_t SPI_TransferByte(uint8_t data) {
    // Start transmission
    SPDR = data;
    
    // Wait for transmission complete
    while (!(SPSR & (1 << SPIF)));
    
    // Return received data
    return SPDR;
}

void SPI_TransferBlock(uint8_t* tx_data, uint8_t* rx_data, uint16_t length) {
    for (uint16_t i = 0; i < length; i++) {
        uint8_t tx_byte = tx_data ? tx_data[i] : 0xFF;
        uint8_t rx_byte = SPI_TransferByte(tx_byte);
        if (rx_data) {
            rx_data[i] = rx_byte;
        }
    }
}
```

### I2C/TWI Interface

```c
// I2C configuration for RTC communication
typedef enum {
    I2C_SUCCESS = 0,
    I2C_ERROR_START,
    I2C_ERROR_SLAW,
    I2C_ERROR_DATA,
    I2C_ERROR_STOP
} I2CResult;

void I2C_Init(void) {
    // Set SCL frequency to 100kHz
    TWBR = ((CPU_FREQUENCY_DEFAULT / 100000UL) - 16) / 2;
    TWSR = 0;                       // Prescaler = 1
    
    // Enable TWI
    TWCR = (1 << TWEN);
}

I2CResult I2C_Start(void) {
    // Send start condition
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    
    // Wait for start condition to be transmitted
    while (!(TWCR & (1 << TWINT)));
    
    // Check status
    if ((TWSR & 0xF8) != 0x08) {
        return I2C_ERROR_START;
    }
    
    return I2C_SUCCESS;
}

I2CResult I2C_SendAddress(uint8_t address) {
    // Load address into data register
    TWDR = address;
    TWCR = (1 << TWINT) | (1 << TWEN);
    
    // Wait for transmission
    while (!(TWCR & (1 << TWINT)));
    
    // Check status
    uint8_t status = TWSR & 0xF8;
    if (status != 0x18 && status != 0x40) {
        return I2C_ERROR_SLAW;
    }
    
    return I2C_SUCCESS;
}

I2CResult I2C_SendData(uint8_t data) {
    // Load data into data register
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    
    // Wait for transmission
    while (!(TWCR & (1 << TWINT)));
    
    // Check status
    if ((TWSR & 0xF8) != 0x28) {
        return I2C_ERROR_DATA;
    }
    
    return I2C_SUCCESS;
}

void I2C_Stop(void) {
    // Send stop condition
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    
    // Wait for stop condition to be executed
    while (TWCR & (1 << TWSTO));
}
```

## CAN Controller

### CAN Controller Architecture

```c
// CAN controller configuration
typedef struct {
    uint32_t baudrate;              // CAN bus baudrate
    uint8_t sample_point;           // Sample point percentage
    bool loopback_mode;             // Loopback mode for testing
    bool listen_only_mode;          // Listen-only mode
    bool automatic_retransmission;  // Automatic retransmission
    uint8_t error_warning_limit;    // Error warning limit
} CANConfig;

// CAN message object configuration
typedef struct {
    uint8_t mob_number;             // Message object number (0-14)
    uint32_t identifier;            // CAN identifier
    uint32_t mask;                  // Acceptance mask
    uint8_t data_length;            // Data length code
    bool extended_id;               // Extended identifier flag
    bool remote_frame;              // Remote frame flag
    bool receive_mode;              // Receive mode (true) or transmit (false)
} CANMessageObject;

void CAN_Init(void) {
    // Reset CAN controller
    CANGCON = (1 << SWRES);
    
    // Wait for reset completion
    while (CANGCON & (1 << SWRES));
    
    // Configure CAN timing for 500kbps
    // Assuming 8MHz system clock
    CANBT1 = 0x02;                  // BRP = 2 (4MHz CAN clock)
    CANBT2 = 0x04;                  // SJW = 1, PRS = 4
    CANBT3 = 0x13;                  // PHS2 = 2, PHS1 = 4
    // Total: 1 + 4 + 4 + 2 = 11 TQ, 4MHz/11 ≈ 364kbps
    
    // Enable CAN controller
    CANGCON = (1 << ENASTB);
    
    // Wait for CAN controller to be enabled
    while (!(CANGSTA & (1 << ENFG)));
    
    // Configure message objects
    ConfigureCANMessageObjects();
    
    // Enable CAN interrupts
    CANGIE = (1 << ENIT) | (1 << ENRX) | (1 << ENTX);
}

void ConfigureCANMessageObjects(void) {
    // Configure MOb 0 for receiving pack controller commands
    SelectMessageObject(0);
    CANCDMOB = (1 << CONMOB1);      // Receive mode
    CANIDT1 = (CAN_ID_PACK_COMMAND >> 3);  // Set identifier
    CANIDT2 = (CAN_ID_PACK_COMMAND << 5);
    CANIDM1 = 0xFF;                 // Exact match mask
    CANIDM2 = 0xE0;
    
    // Configure MOb 1 for transmitting module status
    SelectMessageObject(1);
    CANCDMOB = 0;                   // Disabled initially
    CANIDT1 = (GetModuleCANID() >> 3);
    CANIDT2 = (GetModuleCANID() << 5);
    
    // Configure additional message objects as needed
    for (uint8_t mob = 2; mob < 15; mob++) {
        SelectMessageObject(mob);
        CANCDMOB = 0;               // Disable unused MObs
    }
}

bool CAN_SendMessage(const CANMessage* message) {
    // Find available transmit MOb
    uint8_t mob = FindAvailableTransmitMOb();
    if (mob == 0xFF) {
        return false;               // No available MOb
    }
    
    // Select message object
    SelectMessageObject(mob);
    
    // Set identifier
    CANIDT1 = (message->id >> 3);
    CANIDT2 = (message->id << 5);
    
    // Set data length
    CANCDMOB = (1 << CONMOB0) | (message->length & 0x0F);
    
    // Load data
    for (uint8_t i = 0; i < message->length; i++) {
        CANMSG = message->data[i];
    }
    
    // Start transmission
    CANCDMOB |= (1 << CONMOB0);
    
    return true;
}

// CAN interrupt service routine
ISR(CAN_INT_vect) {
    uint8_t interrupt_source = CANGIT;
    
    if (interrupt_source & (1 << CANIT)) {
        // General CAN interrupt
        ProcessCANGeneralInterrupt();
    }
    
    if (interrupt_source & (1 << OVRTIM)) {
        // Overrun timer interrupt
        ProcessCANOverrunInterrupt();
    }
    
    if (interrupt_source & (1 << BOFFIT)) {
        // Bus-off interrupt
        ProcessCANBusOffInterrupt();
    }
    
    // Clear interrupt flags
    CANGIT = interrupt_source;
}
```

## Analog System

### ADC Configuration and Operation

```c
// ADC configuration for cell monitoring
typedef struct {
    uint8_t reference;              // Voltage reference selection
    uint8_t prescaler;              // ADC clock prescaler
    bool left_adjust;               // Result left adjustment
    bool differential_mode;         // Differential input mode
    uint8_t gain;                   // Gain amplifier setting
} ADCConfig;

void ADC_Init(void) {
    // Configure ADC reference (AVCC with external capacitor)
    ADMUX = (1 << REFS0);
    
    // Configure ADC prescaler for 125kHz ADC clock (8MHz/64)
    ADCSRA = (1 << ADEN) |          // Enable ADC
             (1 << ADPS2) |         // Prescaler /64
             (1 << ADPS1);
    
    // Configure gain amplifier (1x gain initially)
    ADCSRB = 0;
    
    // Perform initial conversion to stabilize ADC
    ADMUX = (1 << REFS0);           // Select channel 0
    ADCSRA |= (1 << ADSC);          // Start conversion
    while (ADCSRA & (1 << ADSC));   // Wait for completion
    (void)ADC;                      // Discard result
}

uint16_t ADC_ReadChannel(uint8_t channel) {
    // Validate channel number
    if (channel > 10) {
        return 0;
    }
    
    // Select channel
    ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
    
    // Start conversion
    ADCSRA |= (1 << ADSC);
    
    // Wait for conversion complete
    while (ADCSRA & (1 << ADSC));
    
    // Return result
    return ADC;
}

uint16_t ADC_ReadDifferential(uint8_t positive, uint8_t negative, uint8_t gain) {
    // Configure differential mode
    uint8_t channel_select;
    
    // Determine differential channel selection
    if (positive == 0 && negative == 1) {
        channel_select = 0x08;      // ADC0 - ADC1
    } else if (positive == 2 && negative == 3) {
        channel_select = 0x0A;      // ADC2 - ADC3
    } else {
        return 0;                   // Invalid differential pair
    }
    
    // Configure gain amplifier
    switch (gain) {
        case 1:  ADCSRB = 0; break;                    // 1x gain
        case 10: ADCSRB = (1 << GSEL); break;          // 10x gain
        case 20: ADCSRB = (1 << GSEL) | (1 << AREFEN); break;  // 20x gain
        default: return 0;          // Invalid gain
    }
    
    // Select differential channel
    ADMUX = (ADMUX & 0xF0) | channel_select;
    
    // Start conversion
    ADCSRA |= (1 << ADSC);
    
    // Wait for conversion complete
    while (ADCSRA & (1 << ADSC));
    
    // Return result
    return ADC;
}

// Cell voltage measurement with calibration
uint16_t MeasureCellVoltage(uint8_t cell_index) {
    // Validate cell index
    if (cell_index >= TOTAL_CELL_COUNT_MAX) {
        return 0;
    }
    
    // Calculate ADC channel for this cell
    uint8_t adc_channel = CalculateADCChannel(cell_index);
    
    // Read raw ADC value
    uint16_t raw_value = ADC_ReadChannel(adc_channel);
    
    // Apply calibration correction
    int16_t offset = GetCellVoltageOffset(cell_index);
    int32_t corrected_value = (int32_t)raw_value + offset;
    
    // Clamp to valid range
    if (corrected_value < 0) corrected_value = 0;
    if (corrected_value > 4095) corrected_value = 4095;
    
    return (uint16_t)corrected_value;
}

// Convert ADC reading to millivolts
uint16_t ADCToMillivolts(uint16_t adc_value) {
    // Assuming 5V reference and 12-bit ADC
    // 5000mV / 4096 counts = 1.22mV per count
    return (uint32_t)adc_value * 5000UL / 4096UL;
}

// Temperature measurement using internal sensor or thermistor
int16_t MeasureTemperature(uint8_t sensor_index) {
    uint16_t adc_value;
    
    if (sensor_index == 0xFF) {
        // Internal temperature sensor
        ADMUX = (1 << REFS1) | (1 << REFS0) | 0x0F;  // 1.1V ref, temp sensor
        ADCSRA |= (1 << ADSC);
        while (ADCSRA & (1 << ADSC));
        adc_value = ADC;
        
        // Convert to temperature (datasheet formula)
        int32_t temp = (adc_value - 314) * 1000 / 1.22;  // Rough conversion
        return (int16_t)temp;
    } else {
        // External thermistor
        adc_value = ADC_ReadChannel(GetTemperatureSensorChannel(sensor_index));
        
        // Convert using Steinhart-Hart equation or lookup table
        return ConvertThermistorToTemperature(adc_value);
    }
}
```

## Power Management

### Low Power Operation

```c
// Power management configuration
typedef enum {
    SLEEP_MODE_IDLE,                // CPU stopped, peripherals running
    SLEEP_MODE_ADC_NOISE_REDUCTION, // CPU and I/O stopped, ADC running
    SLEEP_MODE_POWER_DOWN,          // All clocks stopped except async
    SLEEP_MODE_POWER_SAVE,          // Timer2 async operation allowed
    SLEEP_MODE_STANDBY,             // Crystal oscillator running
    SLEEP_MODE_EXTENDED_STANDBY     // Crystal + Timer2 running
} SleepMode;

void PowerManagement_Init(void) {
    // Configure power reduction register
    PRR = (1 << PRLIN) |           // Disable LIN
          (1 << PRSPI) |           // Disable SPI (enable when needed)
          (1 << PRTIM1);           // Disable Timer1 (enable when needed)
    
    // Configure unused pins as inputs with pull-ups to save power
    ConfigureUnusedPinsForLowPower();
    
    // Enable sleep mode
    SMCR = (1 << SE);
}

void EnterSleepMode(SleepMode mode) {
    // Set sleep mode
    switch (mode) {
        case SLEEP_MODE_IDLE:
            SMCR = (1 << SE);
            break;
        case SLEEP_MODE_ADC_NOISE_REDUCTION:
            SMCR = (1 << SE) | (1 << SM0);
            break;
        case SLEEP_MODE_POWER_DOWN:
            SMCR = (1 << SE) | (1 << SM1);
            break;
        case SLEEP_MODE_POWER_SAVE:
            SMCR = (1 << SE) | (1 << SM1) | (1 << SM0);
            break;
    }
    
    // Disable unnecessary peripherals
    DisableNonEssentialPeripherals();
    
    // Enter sleep mode
    __asm__ __volatile__ ("sleep" ::: "memory");
    
    // Re-enable peripherals after wake
    EnablePeripherals();
}

void ConfigureUnusedPinsForLowPower(void) {
    // Configure unused pins as inputs with pull-ups
    // This prevents floating inputs which can cause increased current consumption
    
    // Port A - configure unused pins
    DDRA &= ~((1 << PA0) | (1 << PA1));  // Set as inputs
    PORTA |= (1 << PA0) | (1 << PA1);    // Enable pull-ups
    
    // Port B - configure unused pins
    DDRB &= ~(1 << PB4);                 // Set as input
    PORTB |= (1 << PB4);                 // Enable pull-up
    
    // Repeat for other unused pins on ports C and D
}

// Watchdog timer configuration
void Watchdog_Init(void) {
    #ifdef WDT_ENABLE
        // Reset watchdog timer
        wdt_reset();
        
        // Configure watchdog for 2 second timeout
        WDTCSR = (1 << WDCE) | (1 << WDE);
        WDTCSR = (1 << WDE) | (1 << WDP2) | (1 << WDP1) | (1 << WDP0);
    #endif
}

void Watchdog_Reset(void) {
    #ifdef WDT_ENABLE
        wdt_reset();
    #endif
}

// Power consumption estimates
typedef struct {
    const char* mode_name;
    float current_ma_typical;       // Typical current consumption (mA)
    float current_ma_max;           // Maximum current consumption (mA)
    const char* description;
} PowerConsumptionSpec;

static const PowerConsumptionSpec power_specs[] = {
    {"Active @ 8MHz",     15.0,  20.0,  "Full operation, all peripherals"},
    {"Idle",              8.0,   12.0,  "CPU stopped, peripherals active"},
    {"ADC Noise Reduction", 6.0,  8.0,   "CPU stopped, ADC running"},
    {"Power Save",        2.0,   3.0,   "Async Timer2 operation only"},
    {"Power Down",        0.5,   1.0,   "All clocks stopped"},
    {"Standby",           0.8,   1.2,   "Crystal oscillator running"}
};
```

## Pin Configuration

### Pin Assignment and Mapping

```c
// Pin definitions for ModuleCPU
// Note: Actual pin assignments depend on hardware design

// ADC input pins for cell monitoring
#define ADC_CELL_START_PIN      ADC0        // First cell ADC input
#define ADC_CELL_COUNT          11          // Number of ADC channels
#define ADC_TEMP_SENSOR_PIN     ADC10       // Temperature sensor input

// CAN interface pins (built-in controller)
#define CAN_RX_PIN              PC2         // CAN RX input
#define CAN_TX_PIN              PC3         // CAN TX output

// SPI pins for SD card interface
#define SPI_MOSI_PIN            PB2         // Master Out Slave In
#define SPI_MISO_PIN            PB3         // Master In Slave Out
#define SPI_SCK_PIN             PB1         // Serial Clock
#define SPI_SS_PIN              PB0         // Slave Select

// I2C pins for RTC interface
#define I2C_SDA_PIN             PC1         // Serial Data
#define I2C_SCL_PIN             PC0         // Serial Clock

// Control pins for balancing circuits
#define BALANCE_FET_ENABLE      PD0         // FET balancing enable
#define BALANCE_RELAY_ENABLE    PD1         // Relay balancing enable
#define BALANCE_FET_CONTROL     PD2         // FET control signal
#define BALANCE_RELAY_CONTROL   PD3         // Relay control signal

// Status and debug pins
#define LED_STATUS_PIN          PD4         // Status LED
#define LED_FAULT_PIN           PD5         // Fault LED
#define DEBUG_TX_PIN            PD6         // Debug UART TX
#define DEBUG_RX_PIN            PD7         // Debug UART RX

// GPIO configuration function
void GPIO_Init(void) {
    // Configure ADC pins as inputs (default state)
    DDRA = 0x00;                    // All Port A as inputs
    PORTA = 0x00;                   // No pull-ups on ADC pins
    
    // Configure SPI pins
    DDRB |= (1 << SPI_MOSI_PIN) | (1 << SPI_SCK_PIN) | (1 << SPI_SS_PIN);
    DDRB &= ~(1 << SPI_MISO_PIN);   // MISO as input
    PORTB |= (1 << SPI_SS_PIN);     // SS high (deselected)
    
    // Configure I2C pins as inputs with pull-ups
    DDRC &= ~((1 << I2C_SDA_PIN) | (1 << I2C_SCL_PIN));
    PORTC |= (1 << I2C_SDA_PIN) | (1 << I2C_SCL_PIN);
    
    // Configure CAN pins
    DDRC &= ~(1 << CAN_RX_PIN);     // RX as input
    DDRC |= (1 << CAN_TX_PIN);      // TX as output
    
    // Configure control pins as outputs
    DDRD |= (1 << BALANCE_FET_ENABLE) | (1 << BALANCE_RELAY_ENABLE) |
            (1 << BALANCE_FET_CONTROL) | (1 << BALANCE_RELAY_CONTROL) |
            (1 << LED_STATUS_PIN) | (1 << LED_FAULT_PIN);
    
    // Configure debug pins
    DDRD |= (1 << DEBUG_TX_PIN);    // TX as output
    DDRD &= ~(1 << DEBUG_RX_PIN);   // RX as input
    PORTD |= (1 << DEBUG_RX_PIN);   // Pull-up on RX
    
    // Initialize outputs to safe states
    PORTD &= ~((1 << BALANCE_FET_ENABLE) | (1 << BALANCE_RELAY_ENABLE) |
               (1 << BALANCE_FET_CONTROL) | (1 << BALANCE_RELAY_CONTROL));
    PORTD &= ~((1 << LED_STATUS_PIN) | (1 << LED_FAULT_PIN));
}

// Pin manipulation functions
void SetBalanceFETEnable(bool enable) {
    if (enable) {
        PORTD |= (1 << BALANCE_FET_ENABLE);
    } else {
        PORTD &= ~(1 << BALANCE_FET_ENABLE);
    }
}

void SetBalanceRelayEnable(bool enable) {
    if (enable) {
        PORTD |= (1 << BALANCE_RELAY_ENABLE);
    } else {
        PORTD &= ~(1 << BALANCE_RELAY_ENABLE);
    }
}

void SetStatusLED(bool on) {
    if (on) {
        PORTD |= (1 << LED_STATUS_PIN);
    } else {
        PORTD &= ~(1 << LED_STATUS_PIN);
    }
}

void SetFaultLED(bool on) {
    if (on) {
        PORTD |= (1 << LED_FAULT_PIN);
    } else {
        PORTD &= ~(1 << LED_FAULT_PIN);
    }
}

// Pin state reading functions
bool GetBalanceFETStatus(void) {
    return (PORTD & (1 << BALANCE_FET_ENABLE)) != 0;
}

bool GetBalanceRelayStatus(void) {
    return (PORTD & (1 << BALANCE_RELAY_ENABLE)) != 0;
}
```

This comprehensive hardware documentation provides the foundation for understanding the ATmega64M1 platform capabilities and configuration for the ModuleCPU firmware. The integrated CAN controller, high-resolution ADC, and automotive-grade design make it ideal for battery module management applications.