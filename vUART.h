#ifndef _VUART_H_
#define _VUART_H_

extern void vUARTInit( void );
extern bool vUARTStarttx(void);
extern void vUARTRXReset(void);
extern void vUARTInitReceive(void);
extern bool vUARTIsBusy(void);  // Returns true if UART is actively receiving

// # Of bytes per cell for cell data IN 1 << FORM (so (1 << 2) =4, (1 << 3) = 8, etc...
#define BYTES_PER_CELL_SHIFT 2

#define PIN_RX								(PORTB2)
#define PIN_TX								(PORTB3)

#define IS_PIN_RX_ASSERTED()				((bool) ((PINB >> PIN_RX) & 0x01))

// Out of the CPU, mc tx is active high, on there other side of the FET it's active low
// so the bus, on the upstream side of the FET, it's high when not being used
#define VUART_TX_ASSERT()					PORTB |= ((uint8_t) (1 << PIN_TX));
#define VUART_TX_DEASSERT()					PORTB &= ((uint8_t) ~(1 << PIN_TX));

// Program INT1 (MC RX) as falling edge interrupt (for start bit detection - idle high, start bit low)
// ISC11=1, ISC10=0: Falling edge
#define VUART_RX_ENABLE()					EICRA = (1 << ISC11) | (0 << ISC10); EIFR |= (1 << INTF1); EIMSK |= (1 << INT1);
#define VUART_RX_DISABLE()					EIMSK &= (uint8_t) ~(1 << INT1);

// Program INT1 for any edge interrupt (for timing correction during reception)
// ISC11=0, ISC10=1: Any logical change
#define VUART_RX_ANY_EDGE()				EICRA = (EICRA & ~(1 << ISC11)) | (1 << ISC10); EIFR |= (1 << INTF1);
// Return to falling edge interrupt (for start bit detection)  
#define VUART_RX_FALLING_EDGE()			EICRA = (1 << ISC11) | (0 << ISC10); EIFR |= (1 << INTF1);

// Set oneshot timer and enable interrupt
#define TIMER_CHA_INT(x)					OCR0A = (uint8_t) (TCNT0 + (x)); TIFR0 |= (1 << OCF0A); TIMSK0 |= (1 << OCIE0A)
#define TIMER_CHB_INT(x)					OCR0B = (uint8_t) (TCNT0 + (x)); TIFR0 |= (1 << OCF0B); TIMSK0 |= (1 << OCIE0B)
#define TIMER_CHB_MAX()						OCR0B = (uint8_t) (TCNT0 - 1); TIFR0 = (1 << OCF0B); TIMSK0 |= (1 << OCIE0B)

// Disable interrupt
#define TIMER_CHA_INT_DISABLE()				TIMSK0 &= (uint8_t) ~(1 << OCIE0A)
#define TIMER_CHB_INT_DISABLE()				TIMSK0 &= (uint8_t) ~(1 << OCIE0B)


#endif // _VUART_H_