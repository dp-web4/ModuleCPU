#ifndef I2C_H_
#define I2C_H_

extern void I2CSetup(void);
extern bool I2CTxByte(uint8_t u8Byte);
extern uint8_t I2CRxByte(bool bAck);
extern void I2CStart(void);
extern void I2CStop(void);
extern void I2CUnstick(void);
extern bool I2CStartTransaction(uint8_t u8SlaveAddress,
								bool bRead);

#endif
