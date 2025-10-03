#ifndef _CAN_H_
#define _CAN_H_

// MOB indices
#define CANMOB_RX_IDX			(0)
#define CANMOB_TX_IDX			(1)

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

	// Frame transfer messages
	ECANMessageType_FrameTransferRequest,  // Pack → Module: Request frame transfer
	ECANMessageType_FrameTransferStart,    // Module → Pack: Start frame transfer
	ECANMessageType_FrameTransferData,     // Module → Pack: Frame data segment
	ECANMessageType_FrameTransferEnd,      // Module → Pack: End frame transfer

	ECANMessageType_MAX
} ECANMessageType;

extern void CANInit( void );
extern void CANSetRXCallback( void (*pfCallback)(ECANMessageType eType, uint8_t* pu8Data, uint8_t u8DataLen) );
extern void CANSetModuleIDFilter( uint8_t u8MOBIndex, uint8_t u8ModuleID );
extern bool CANSendMessage( ECANMessageType eType,
							uint8_t* pu8Data,
							uint8_t u8DataLen );
extern bool CANSendMessageWithSeq( ECANMessageType eType,
							uint8_t* pu8Data,
							uint8_t u8DataLen,
							uint16_t u16SeqNum );
extern void CANPoll( void );
extern void CANCheckTxStatus( void );

// Diagnostic functions
extern uint16_t CANGetTxTimeouts( void );
extern uint16_t CANGetTxErrors( void );
extern uint16_t CANGetTxOkPolled( void );
extern uint16_t CANGetBusOffEvents( void );
extern uint8_t CANGetTEC( void );
extern uint8_t CANGetREC( void );
extern uint8_t CANGetTxOnlyErrorCount( void );
extern uint8_t CANGetTxBackoffDelay( void );
extern void CANCheckHealth( void );

#endif	// _CAN_H_