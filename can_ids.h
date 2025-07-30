/***************************************************************************************************************
 * @file           : can_ids.h
 * @brief          : Modbatt CAN packet identifiers 
 ***************************************************************************************************************
 *
 * Copyright (c) 2023 Modular Battery Technologies, Inc
 *
 **************************************************************************************************************/
#ifndef INC_CAN_IDS_H_
#define INC_CAN_IDS_H_

// CAN Packet IDs
// Module Controller to Pack Controller
#define PKT_MODULE_ANNOUNCEMENT     0x500
#define PKT_MODULE_HARDWARE			0x501
#define PKT_MODULE_STATUS1          0x502
#define PKT_MODULE_STATUS2			0x503
#define PKT_MODULE_STATUS3			0x504
#define PKT_MODULE_CELL_DETAIL		0x505
#define PKT_MODULE_REQUEST_TIME		0x506
#define PKT_MODULE_CELL_COMM_STAT1	0x507
#define PKT_MODULE_CELL_COMM_STAT2	0x508

// Pack Controller to Module Controller
#define PKT_MODULE_REGISTRATION     0x510
#define PKT_MODULE_HARDWARE_REQUEST	0x511
#define PKT_MODULE_STATUS_REQUEST   0x512
#define PKT_MODULE_STATE_CHANGE     0x514
#define PKT_MODULE_DETAIL_REQUEST   0x515
#define PKT_MODULE_SET_TIME			0x516
#define PKT_MODULE_MAX_STATE		0x517
#define PKT_MODULE_DEREGISTER       0x518
#define PKT_MODULE_ANNOUNCE_REQUEST 0x51D
#define PKT_MODULE_ALL_DEREGISTER   0x51E
#define PKT_MODULE_ALL_ISOLATE      0x51F

#endif /* INC_CAN_IDS_H_ */
