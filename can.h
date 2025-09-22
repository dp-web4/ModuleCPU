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
	ECANMessageType_ModuleDeRegister,
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
extern void CANCheckTxStatus( void );

// Diagnostic functions
extern uint16_t CANGetTxTimeouts( void );
extern uint16_t CANGetTxErrors( void );
extern uint16_t CANGetTxOkPolled( void );
extern uint16_t CANGetBusOffEvents( void );
extern uint8_t CANGetTEC( void );
extern uint8_t CANGetREC( void );
extern void CANCheckHealth( void );

#endif	// _CAN_H_