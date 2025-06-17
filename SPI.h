#ifndef _SPI_H_

// States of the SPI bus
typedef enum
{
	ESTATE_IDLE,
	ESTATE_TX_DATA,
	ESTATE_TX_PATTERN,
	ESTATE_RX_DATA
} ESPIBusState;

extern void SPIInit(void);
extern void SPITransaction(ESPIBusState eSPIBusState,
						   uint8_t *pu8Buffer,
						   uint16_t u16ByteCount);
extern uint32_t SPISetBaudRate(uint32_t u32BaudRate);

#define	SPIRead(ptr, size)					SPITransaction(ESTATE_RX_DATA, ptr, size)
#define	SPIWrite(ptr, size)					SPITransaction(ESTATE_TX_DATA, ptr, size)
#define	SPIWritePattern(pattern_ptr, size)	SPITransaction(ESTATE_TX_PATTERN, (uint8_t *) pattern_ptr, size)

#endif