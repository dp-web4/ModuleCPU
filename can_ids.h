/***************************************************************************************************************
 * @file           : can_ids.h
 * @brief          : Modbatt CAN packet identifiers - ModuleCPU wrapper
 ***************************************************************************************************************
 *
 * Copyright (c) 2023 Modular Battery Technologies, Inc
 *
 * This file includes the shared protocol definitions and creates PKT_ aliases for ModuleCPU compatibility
 **************************************************************************************************************/
#ifndef INC_CAN_IDS_H_
#define INC_CAN_IDS_H_

// Include shared protocol definitions
#include "../Pack-Controller-EEPROM/protocols/CAN_ID_ALL.h"

// Create PKT_ aliases for ModuleCPU code compatibility
// Module Controller to Pack Controller
#define PKT_MODULE_ANNOUNCEMENT     ID_MODULE_ANNOUNCEMENT
#define PKT_MODULE_HARDWARE         ID_MODULE_HARDWARE
#define PKT_MODULE_STATUS1          ID_MODULE_STATUS_1
#define PKT_MODULE_STATUS2          ID_MODULE_STATUS_2
#define PKT_MODULE_STATUS3          ID_MODULE_STATUS_3
#define PKT_MODULE_CELL_DETAIL      ID_MODULE_DETAIL
#define PKT_MODULE_REQUEST_TIME     ID_MODULE_TIME_REQUEST
#define PKT_MODULE_CELL_COMM_STAT1  ID_MODULE_CELL_COMM_STATUS1
#define PKT_MODULE_CELL_COMM_STAT2  ID_MODULE_CELL_COMM_STATUS2

// Pack Controller to Module Controller
#define PKT_MODULE_REGISTRATION     ID_MODULE_REGISTRATION
#define PKT_MODULE_HARDWARE_REQUEST ID_MODULE_HARDWARE_REQUEST
#define PKT_MODULE_STATUS_REQUEST   ID_MODULE_STATUS_REQUEST
#define PKT_MODULE_STATE_CHANGE     ID_MODULE_STATE_CHANGE
#define PKT_MODULE_DETAIL_REQUEST   ID_MODULE_DETAIL_REQUEST
#define PKT_MODULE_SET_TIME         ID_MODULE_SET_TIME
#define PKT_MODULE_MAX_STATE        ID_MODULE_MAX_STATE
#define PKT_MODULE_DEREGISTER       ID_MODULE_DEREGISTER
#define PKT_MODULE_ANNOUNCE_REQUEST ID_MODULE_ANNOUNCE_REQUEST
#define PKT_MODULE_ALL_DEREGISTER   ID_MODULE_ALL_DEREGISTER
#define PKT_MODULE_ALL_ISOLATE      ID_MODULE_ALL_ISOLATE

// Frame transfer (bidirectional)
#define PKT_FRAME_TRANSFER_REQUEST  ID_FRAME_TRANSFER_REQUEST
#define PKT_FRAME_TRANSFER_START    ID_FRAME_TRANSFER_START
#define PKT_FRAME_TRANSFER_DATA     ID_FRAME_TRANSFER_DATA
#define PKT_FRAME_TRANSFER_END      ID_FRAME_TRANSFER_END

#endif /* INC_CAN_IDS_H_ */
