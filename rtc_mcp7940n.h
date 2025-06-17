#ifndef _RTC_MCP7940N_H_
#define _RTC_MCP7940N_H_

#include <time.h>
#include <stdbool.h>

extern bool RTCInit( void );
extern bool RTCRead( struct tm * psTime );
extern bool RTCSetTime( uint64_t u64Timet );

#endif // _RTC_MCP7940N_H_