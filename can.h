#ifndef _CAN_H_
#define _CAN_H_

typedef enum
{
	// Module controller messages
	ECANMessageType_ModuleAnnouncement = 0,
	ECANMessageType_ModuleStatus1,
	ECANMessageType_ModuleStatus2,
	ECANMessageType_ModuleStatus3,
	ECANMessageType_ModuleCellDetail,
	ECANMessageType_ModuleHardwareDetail,
	ECANMessageType_ModuleCellCommStat1,
	ECANMessageType_ModuleCellCommStat2,
	ECANMessageType_ModuleRequestTime,
	
	// Pack controller messages
	ECANMessageType_ModuleRegistration,
	ECANMessageType_ModuleStatusRequest,
	ECANMessageType_ModuleCellDetailRequest,
	ECANMessageType_ModuleStateChangeRequest,
	ECANMessageType_ModuleAnnounceRequest,
	ECANMessageType_AllDeRegister,
	ECANMessageType_AllIsolate,
	ECANMessageType_SetTime,
	ECANMessageType_MaxState,
	
	ECANMessageType_MAX
} ECANMessageType;

extern void CANInit( void );
extern void CANSetRXCallback( void (*pfCallback)(ECANMessageType eType, uint8_t* pu8Data, uint8_t u8DataLen) );
extern bool CANSendMessage( ECANMessageType eType,
							uint8_t* pu8Data,
							uint8_t u8DataLen );
extern void CANPoll( void );

#endif	// _CAN_H_