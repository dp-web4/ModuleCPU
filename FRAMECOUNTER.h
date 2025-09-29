#ifndef _FRAMECOUNTER_H_
#define _FRAMECOUNTER_H_

#include <stdint.h>
#include <stdbool.h>

// Frame counter functions with wear leveling
extern void FrameCounter_Init(void);           // Initialize and find current counter value
extern uint32_t FrameCounter_Get(void);        // Get current counter value
extern void FrameCounter_Increment(void);      // Increment and persist counter
extern uint8_t FrameCounter_GetPosition(void); // Get current wear leveling position

#endif