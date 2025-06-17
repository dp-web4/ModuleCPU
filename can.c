
/* 
 * Module controller firmware - CAN
 *
 * This module contains the CAN bus functionality. 
 *
 */

#include <stdint.h>
#include <xc.h>
#include <stdbool.h>
#include <avr/interrupt.h>
#include <string.h>
#include "main.h"
#include "can.h"
#include "can_ids.h"

#define CAN_DISABLED			(0)
#define CAN_TXONLY				(1)
#define CAN_RXONLY				(2)
#define CAN_FBRX				(3)

#define CANMOB_RX_IDX			(0)
#define CANMOB_TX_IDX			(1)

static volatile bool sg_bBusy;
static void (*sg_pfRXCallback)(ECANMessageType eType, uint8_t* pu8Data, uint8_t u8DataLen);

// Maximum size of CAN message
#define CAN_MAX_MSG_SIZE		8

// Last transmit info
static uint8_t sg_u8TransmitAttempts;
static ECANMessageType sg_eLastTXType;
static uint8_t sg_u8LastTXData[CAN_MAX_MSG_SIZE];
static uint8_t sg_u8LastTXDataLen;
static volatile bool sg_bInRetransmit = false;

typedef struct 
{
	uint8_t u8Mode;
	bool bReplyValid;
	uint16_t u16ID;
	uint16_t u16IDMask;
	bool bRTRTag;
	bool bRTRMask;
} SMOBDef;

static const SMOBDef sg_sMOBDisabled =
{
	CAN_DISABLED,
	false,
	0x7ff,
	0x7ff,
	false,
	false,
};

// Recevie any message with ID 0x5xx (RTR or not)
static const SMOBDef sg_sMOBGenericReceive =
{
	CAN_RXONLY,
	false,
	0x500,
	0x500,
	false,
	false,
};

static const SMOBDef sg_sMOBModuleAnnouncement =
{
	CAN_TXONLY,
	false,
	PKT_MODULE_ANNOUNCEMENT,
	0x7ff,
	false,
	false,
};

static const SMOBDef sg_sMOBModuleStatus1 =
{
	CAN_TXONLY,
	false,
	PKT_MODULE_STATUS1,
	0x7ff,
	false,
	false,
};

static const SMOBDef sg_sMOBModuleStatus2 =
{
	CAN_TXONLY,
	false,
	PKT_MODULE_STATUS2,
	0x7ff,
	false,
	false,
};

static const SMOBDef sg_sMOBModuleStatus3 =
{
	CAN_TXONLY,
	false,
	PKT_MODULE_STATUS3,
	0x7ff,
	false,
	false,
};

static const SMOBDef sg_sMOBModuleCellCommStat1 =
{
	CAN_TXONLY,
	false,
	PKT_MODULE_CELL_COMM_STAT1,
	0x7ff,
	false,
	false,
};

static const SMOBDef sg_sMOBModuleCellCommStat2 =
{
	CAN_TXONLY,
	false,
	PKT_MODULE_CELL_COMM_STAT2,
	0x7ff,
	false,
	false,
};

static const SMOBDef sg_sMOBModuleCellDetail = 
{
	CAN_TXONLY,
	false,
	PKT_MODULE_CELL_DETAIL,
	0x7ff,
	false,
	false,
};

static const SMOBDef sg_sMOBModuleHardwareDetail = 
{
	CAN_TXONLY,
	false,
	PKT_MODULE_HARDWARE,
	0x7ff,
	false,
	false,
};

static const SMOBDef sg_sMOBModuleRequestTime =
{
	CAN_TXONLY,
	false,
	PKT_MODULE_REQUEST_TIME,
	0x7ff,
	false,
	false,
};


typedef struct  
{
	uint16_t u16ID;
	ECANMessageType eType;
} SCANCmdReg;

static const SCANCmdReg sg_sRXCommandList[] =
{
	{PKT_MODULE_REGISTRATION,	ECANMessageType_ModuleRegistration},
	{PKT_MODULE_DETAIL_REQUEST, ECANMessageType_ModuleCellDetailRequest},
	{PKT_MODULE_STATE_CHANGE,	ECANMessageType_ModuleStateChangeRequest},
	{PKT_MODULE_STATUS_REQUEST, ECANMessageType_ModuleStatusRequest},
	{PKT_MODULE_HARDWARE_REQUEST, ECANMessageType_ModuleHardwareDetail},
	{PKT_MODULE_ALL_DEREGISTER, ECANMessageType_AllDeRegister},
	{PKT_MODULE_ALL_ISOLATE,	ECANMessageType_AllIsolate},
	{PKT_MODULE_SET_TIME,		ECANMessageType_SetTime},
	{PKT_MODULE_MAX_STATE,		ECANMessageType_MaxState}
};

static ECANMessageType CANLookupCommand( uint16_t u16ID )
{
	uint8_t u8Index = 0;
	
	while( u8Index < (sizeof(sg_sRXCommandList)/sizeof(sg_sRXCommandList[0])) )
	{
		if( sg_sRXCommandList[u8Index].u16ID == u16ID )
		{
			return( sg_sRXCommandList[u8Index].eType );
		}
		
		u8Index++;
	}
	
	return( ECANMessageType_MAX );
}

static void CANMOBSet( uint8_t u8MOBIndex,
					   const SMOBDef* psDef,
					   uint8_t* pu8Data,
					   uint8_t u8DataLen )
{
	uint8_t u8CANCDMOBValue;
	uint32_t u32MessageID;
	
	MBASSERT( u8MOBIndex <= 5 );
	MBASSERT( u8DataLen <= 8 );
	
	// Set the MOB page, auto increment, and data index 0
	CANPAGE = u8MOBIndex << 4;
	
	// Clear the MOB status
	CANSTMOB = 0;
	
	// Set the MOB registers for this page
	u8CANCDMOBValue = u8DataLen;
	u8CANCDMOBValue |= (psDef->u8Mode << CONMOB0);
	if( psDef->bReplyValid )
	{
		u8CANCDMOBValue |= (1 << RPLV);
	}
	u8CANCDMOBValue |= (1 << IDE);
	
	// Extended frame address format
	//	Bits 0-7: Pack / Module registration ID of sender (if assigned, zero otherwise)
	//	Bits 8-17: Reserved, set to zero.
	//	Bits 18-28: Static message ID (eg. 0x500)
	
	u32MessageID = (uint32_t)PlatformGetRegistrationID();
	u32MessageID |= (((uint32_t)psDef->u16ID) & 0x7ff) << 18;
	
	CANIDT4 = psDef->bRTRTag?(1 << RTRTAG):0;
	CANIDT4 |= u32MessageID << 3;
	CANIDT3 = u32MessageID >> 5;
	CANIDT2 = u32MessageID >> 13;
	CANIDT1 = u32MessageID >> 21;
	
	// Setup mask for the cmd (require matching ID extention bit)
	CANIDM4 = psDef->bRTRMask?(1 << RTRTAG):0;
	CANIDM4 |= (1 << IDEMSK);
	CANIDM3 = 0;
	CANIDM2 = psDef->u16IDMask << 5;
	CANIDM1 = psDef->u16IDMask >> 3;
	
	// Set the data into the FIFO
	while( u8DataLen )
	{
		CANMSG = *pu8Data;
		
		pu8Data++;
		u8DataLen--;
	}
	
	// Send it now
	CANCDMOB = u8CANCDMOBValue;
	
	// Enable (activates the MOB) or disable the MOB interrupt
	if( psDef->u8Mode != CAN_DISABLED )
	{
		CANIE2 |= (1 << u8MOBIndex);
	}
	else
	{
		CANIE2 &= (uint8_t)~(1 << u8MOBIndex);
	}
}

static void CANSendMessageInternal( ECANMessageType eType,
									uint8_t* pu8Data,
									uint8_t u8DataLen,	
									bool bRetransmit )
{
	const SMOBDef* psDef = NULL;
	
	if( ECANMessageType_ModuleAnnouncement == eType )
	{
		psDef = &sg_sMOBModuleAnnouncement;
	}
	else if( ECANMessageType_ModuleStatus1 == eType )
	{
		psDef = &sg_sMOBModuleStatus1;
	}
	else if( ECANMessageType_ModuleStatus2 == eType )
	{
		psDef = &sg_sMOBModuleStatus2;
	}
	else if( ECANMessageType_ModuleStatus3 == eType )
	{
		psDef = &sg_sMOBModuleStatus3;
	}
	else if( ECANMessageType_ModuleCellCommStat1 == eType )
	{
		psDef = &sg_sMOBModuleCellCommStat1;
	}
	else if( ECANMessageType_ModuleCellCommStat2 == eType )
	{
		psDef = &sg_sMOBModuleCellCommStat2;
	}
	else if( ECANMessageType_ModuleHardwareDetail == eType )
	{
		psDef = &sg_sMOBModuleHardwareDetail;
	}
	else if( ECANMessageType_ModuleCellDetail == eType )
	{
		psDef = &sg_sMOBModuleCellDetail;
	}
	else if( ECANMessageType_ModuleRequestTime == eType )
	{
		psDef = &sg_sMOBModuleRequestTime;
	}
	else
	{
		// Invalid message type
		MBASSERT(0);
	}
	
	if(bRetransmit && sg_bInRetransmit) {
		// Already in a retransmit, don't allow nested retries
		return;
	}
	if( bRetransmit || (false == sg_bBusy) )
	{
		sg_bBusy = true;
		
		// Save this message info for retransmit later if needed
		if( false == bRetransmit )
		{
			sg_u8TransmitAttempts = 0;
			sg_eLastTXType = eType;
			MBASSERT(u8DataLen <= CAN_MAX_MSG_SIZE);
			memcpy(sg_u8LastTXData, pu8Data, u8DataLen);
			sg_u8LastTXDataLen = u8DataLen;
		}
		else
		{
			sg_u8TransmitAttempts++;
		}
		
		// Send it now
		CANMOBSet( CANMOB_TX_IDX, psDef, pu8Data, u8DataLen );
	}
}

void CANMOBInterrupt( uint8_t u8MOBIndex )
{
	// Set the MOB page first (and zero the data index)
	CANPAGE = u8MOBIndex << 4;
	
	// Temporarily disable the MOB to examine and modify the registers
	CANIE2 &= (uint8_t)~(1 << u8MOBIndex);
	CANCDMOB &= (uint8_t)~((1 << CONMOB0) | (1 << CONMOB1));
	
	if( CANMOB_RX_IDX == u8MOBIndex )
	{
		// TX success?  Just clear it since this is the RX context
		if( CANSTMOB & (1 << TXOK) )
		{
			// Clear it
			CANSTMOB &= ~(1 << TXOK);
            sg_bBusy = false;
            sg_bInRetransmit = false;  // Clear flag on success	
		}
		// RX success?
		if( CANSTMOB & (1 << RXOK) )
		{
			// Clear it
			CANSTMOB &= ~(1 << RXOK);
		
			if( sg_pfRXCallback )
			{
				ECANMessageType eType;
				uint16_t u16ID = 0;
				uint8_t u8Data[8];
				uint8_t u8DataLen = CANCDMOB & 0x0f;
				uint8_t u8Index = 0;
			
				// Grab the messageID from the identifier registers
				u16ID = ((uint16_t)CANIDT1) << 3;
				u16ID |= CANIDT2 >> 5;
			
				// Pull the data out into a temp buffer
				while( u8Index < u8DataLen )
				{
					u8Data[u8Index] = CANMSG;
					u8Index++;
				}
			
				// Lookup ID to see if it matches one of ours
				eType = CANLookupCommand( u16ID );
			
				if( eType != ECANMessageType_MAX )
				{
					// Call comamnd handler
					sg_pfRXCallback( eType, u8Data, u8DataLen );
				}
			}
		}
	
		// If receive error, clear the bits
		if( CANSTMOB & ((1 << SERR) | (1 << CERR) | (1 << FERR)) )
		{
			// Clear it.  Just ignore
			CANSTMOB &= ~((1 << SERR) | (1 << CERR) | (1 << FERR));
		}
	
		// Re-enable RX now
		CANIE2 |= (1 << u8MOBIndex);
		CANCDMOB |= (1 << CONMOB1);
	}
	else if( CANMOB_TX_IDX == u8MOBIndex )
	{
		if( CANSTMOB & (1 << TXOK) )
		{
			// Clear it
			CANSTMOB &= ~(1 << TXOK);
		
			sg_bBusy = false;
		}
		
		// RX success, just clear it since this is the TX context
		if( CANSTMOB & (1 << RXOK) )
		{
			// Clear it
			CANSTMOB &= ~(1 << RXOK);
		}

		// If TX Error on transmit (collision with another device)
		// -or- ACK error (nobody listening),
		// retry send
		if( CANSTMOB & ((1 << BERR) | (1 << AERR) | (1 << SERR)) )
		{
			// Clear it
			CANSTMOB &= ~((1 << BERR) | (1 << AERR) | (1 << SERR));

			if( sg_u8TransmitAttempts < 20 )
			{
				// Retransmit now
               sg_bInRetransmit = true;  // Set flag before retry
               CANSendMessageInternal( sg_eLastTXType, sg_u8LastTXData, sg_u8LastTXDataLen, true );
			}
			else
			{
				// Retries exhausted.  Give up
				sg_bBusy = false;
                sg_bInRetransmit = false;
			}
		}
	}
}

// Generic CAN interrupt handler
// Check interrrupt sources and clear them (by servicing or explicit clearing)
ISR(CAN_INT_vect, ISR_BLOCK)
{
	// Save state we'll need to restore
	uint8_t saved_cangie = CANGIE;
	uint8_t saved_canie2 = CANIE2;	// Temporarily disable CAN interrupts to prevent reentry
	CANGIE &= (uint8_t)~(1 << ENIT);

	sei();
	
	 uint8_t sit = CANSIT2;
	 
	 // Most common interrupts first  *claude
	 if(sit & (1 << CANMOB_RX_IDX)) {
		 CANMOBInterrupt(CANMOB_RX_IDX);
		 CANMOBSet(CANMOB_RX_IDX, &sg_sMOBGenericReceive, NULL, 0);
	 }
	 
	
	// Check TX MOB
	if( sit & (1 << CANMOB_TX_IDX) )
	{
		CANMOBInterrupt( CANMOB_TX_IDX );
	}
	
	// Now the generic, non-MOB interrupts (some of which may have already been handled by the MOB handler)
	
	// Bus Off interrupt
	if( CANGIT & (1 << BOFFIT) )
	{
		// Just clear it for now
		CANGIT = (1 << BOFFIT);
	}
	
	// Frame buffer receive (burst receive interrupt)
	// This should not be used
	if( CANGIT & (1 << BXOK) )
	{
		MBASSERT(0);
		
		// TODO: Must write the CONMOB fields of the MOB that interrupted first?
		// Just clear it for now
		CANGIT = (1 << BXOK);
	}
	
	// Bit stuffing error on receive
	if( CANGIT & (1 << SERG) )
	{
		// Just clear it for now.  Ignored.
		CANGIT = (1 << SERG);
	}
	
	// CRC error on receive
	if( CANGIT & (1 << CERG) )
	{
		// Just clear it for now.  Ignored.
		CANGIT = (1 << CERG);
	}
	
	// Form error
	if( CANGIT & (1 << FERG) )
	{
		// If detected form error on transmit, could be duplicate to MOB error interrupt (see 16.8.2 - Interrupt Behavior)
		// Ignore this one
		
		// Just clear it for now.  Ignored.
		CANGIT = (1 << FERG);
	}
	
	// ACK missing on transmit (nobody's listening! (or failure on transmit???))
	if( CANGIT & (1 << AERG) )
	{
		// Clear the interrupt
		CANGIT = (1 << AERG);
		
		if( sg_u8TransmitAttempts < 20 )
		{
			// Retransmit now
			CANSendMessageInternal( sg_eLastTXType, sg_u8LastTXData, sg_u8LastTXDataLen, true );
		}
		else
		{
			// Retries exhausted.  Give up
			sg_bBusy = false;
		}
	}

	// Reenable CAN general interrupt	
//	CANGIE |= (1 << ENIT);
    CANIE2 = saved_canie2;
    CANGIE = saved_cangie;
}

bool CANSendMessage( ECANMessageType eType,
					 uint8_t* pu8Data,
					 uint8_t u8DataLen )
{
	// If we're busy, kick it back so we're cooperatively multitasking
	// rather than spin locking
	if (sg_bBusy)
	{
		return(false);
	}

	CANSendMessageInternal( eType, pu8Data, u8DataLen, false );
	return( true );
}

void CANSetRXCallback( void (*pfCallback)(ECANMessageType eType, uint8_t* pu8Data, uint8_t u8DataLen) )
{
	sg_pfRXCallback = pfCallback;
}

void CANInit( void )
{
	// Init clock
	// See datasheet table 16-2.
	// For 8Mhz, set CAN rate to 500 Kbps
	CANBT1 = 0x02;
	CANBT2 = 0x04;
	CANBT3 = 0x12;
	
	// Init message objects (MOBs) since there are no defaults at reset
	CANMOBSet( 0, &sg_sMOBDisabled, NULL, 0 );
	CANMOBSet( 1, &sg_sMOBDisabled, NULL, 0 );
	CANMOBSet( 2, &sg_sMOBDisabled, NULL, 0 );
	CANMOBSet( 3, &sg_sMOBDisabled, NULL, 0 );
	CANMOBSet( 4, &sg_sMOBDisabled, NULL, 0 );
	CANMOBSet( 5, &sg_sMOBDisabled, NULL, 0 );
	
	// Setup generic RX
	CANMOBSet( CANMOB_RX_IDX, &sg_sMOBGenericReceive, NULL, 0 );
	
	// Enable general CAN interrupts
	CANGIE = (1 << ENIT) | (1 << ENRX) | (1 << ENTX) | (1 << ENERR) | (1 << ENBX) | (1 << ENERG);

//	CANGIE &= ~(1 << ENOVRT);  // Disable CAN timer overflow interrupt

	// Enable the bus
	CANGCON = (1 << ENASTB);
	sg_bBusy = false;
}

