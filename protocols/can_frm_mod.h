 /**************************************************************************************************************
 * @file           : can_frm_mod.h                                                 P A C K   C O N T R O L L E R
 * @brief          : Modbatt CAN frame structures for module to pack controller interface
 ***************************************************************************************************************
 * Copyright (C) 2023-2024 Modular Battery Technologies, Inc.
 * US Patents 11,380,942; 11,469,470; 11,575,270; others. All rights reserved
 **************************************************************************************************************/
#ifndef INC_CAN_FRM_MOD_H_
#define INC_CAN_FRM_MOD_H_
/*
                                  =====================
                                  CAN FRAME STRUCTURES
                                  =====================
*/
typedef struct {                  // 0x500 ANNOUNCEMENT - 8 bytes
  uint32_t moduleFw       : 16;    // module firmware version
  uint32_t moduleMfgId    : 8;    // module hardware manufacturer
  uint32_t modulePartId   : 8;    // module part ID
  uint32_t moduleUniqueId : 32;   // module unique ID
}CANFRM_MODULE_ANNOUNCEMENT;

typedef struct {                  // 0x511 MODULE HARDWARE REQUEST - 1 byte
  uint8_t moduleId      : 8;      // module ID
}CANFRM_MODULE_HW_REQUEST;


typedef struct {                  // 0x501 HARDWARE CAPABILITY - 8 bytes
  uint32_t maxChargeA     : 16;   // module maximum charge current
  uint32_t maxDischargeA  : 16;   // module maximum discharge current
  uint32_t maxChargeEndV  : 16;   // module maximum end charge voltage
  uint32_t hwVersion      : 16;   // module hardware version information
}CANFRM_MODULE_HARDWARE;


typedef struct {                  // 0x502 MODULE STATUS #1 - 8 bytes
  uint32_t moduleState  : 4;      // module current state
  uint32_t moduleStatus : 4;      // module status
  uint32_t moduleSoc    : 8;      // module 8 bit state of charge
  uint32_t moduleSoh    : 8;      // module state of health
  uint32_t cellCount    : 8;      // module total number of cells (0-255)
  uint32_t moduleMmc    : 16;     // module measured voltage
  uint32_t moduleMmv    : 16;     // module measured current
}CANFRM_MODULE_STATUS_1;


typedef struct {                  // 0x503 MODULE STATUS #2 - 8 bytes
  uint32_t cellLoVolt   : 16;     // module minimum cell voltage
  uint32_t cellHiVolt   : 16;     // module maximum cell voltage
  uint32_t cellAvgVolt  : 16;     // module average cell voltage
  uint32_t cellTotalV   : 16;     // module total cell voltage
}CANFRM_MODULE_STATUS_2;


typedef struct {                  // 0x504 MODULE STATUS #3 - 8 bytes
  uint32_t cellLoTemp   : 16;     // module highest cell temperature
  uint32_t cellHiTemp   : 16;     // module lowest cell temperature
  uint32_t cellAvgTemp  : 16;     // module average cell temperature
  uint32_t UNUSED_32_63 : 16;
}CANFRM_MODULE_STATUS_3;


typedef struct {                  // 0x505 MODULE DETAIL - 8 bytes
  uint32_t cellId       : 8;      // cell ID
  uint32_t cellCount    : 8;      // module total number of cells (0-255)
  uint32_t cellTemp     : 16;     // cell temperature
  uint32_t cellVoltage  : 16;     // cell voltage
  uint32_t cellSoc      : 8;      // cell SOC
  uint32_t cellSoh      : 8;      // cell SOH
}CANFRM_MODULE_DETAIL;

typedef struct {                   // 0x515 MODULE DETAIL REQUEST - 3 bytes
  uint32_t moduleId      : 8;      // module ID
  uint32_t cellId        : 8;      // module cell number
  uint32_t UNUSED_18_31  : 16;     // UNUSED bits 18-31
  uint32_t UNUSED_32_63  : 32;     // UNUSED bits 32-63
}CANFRM_MODULE_DETAIL_REQUEST;


typedef struct {                  // 0x510 MODULE REGISTRATION - 8 bytes
  uint32_t moduleId       : 8;    // modules Id number used for future data exchange rather than unique ID
  uint32_t controllerId   : 8;    // controller Id that the module has been registered with
  uint32_t moduleMfgId    : 8;    // module hardware manufacturer
  uint32_t modulePartId   : 8;    // module part ID
  uint32_t moduleUniqueId : 32;   // module unique ID
}CANFRM_MODULE_REGISTRATION;


typedef struct {                  // 0x512 MODULE STATUS REQUEST - 1 byte
  uint8_t moduleId      : 8;      // module ID
}CANFRM_MODULE_STATUS_REQUEST;


typedef struct {                  // 0x514 MODULE STATE CHANGE - 4 bytes
  uint32_t moduleId      : 8;     // module ID
  uint32_t state         : 4;     // module state
  uint32_t UNUSED_12_15  : 4;     // 4 bits unused
  uint32_t hvBusVoltage  : 16;    // HV bus voltage
}CANFRM_MODULE_STATE_CHANGE;

typedef struct {                  // 0x506 Time Request - 0 bytes
  uint32_t UNUSED_00_31  : 32;     // UNUSED bits 00-31
  uint32_t UNUSED_32_63  : 32;     // UNUSED bits 32-63
}CANFRM_MODULE_TIME_REQUEST;


typedef struct {                  // 0x506 Time Request - 0 bytes
  uint64_t time           : 63;   // 63 bit time_t
  uint64_t rtcValid       : 1;    // rtcValid flag
}CANFRM_MODULE_TIME;


typedef struct {                  // 0x507 MODULE CELL COMMUNICATION STATUS #1 - 8 bytes
  uint32_t leastCellMsgs      : 8 ;   // Fewest # of cell messages = 0xff=No cells received
  uint32_t mostCellMsgs       : 8 ;   // Highest # of cell messages = 0x00 = No cells received
  uint32_t i2cErrors          : 16;   // # Of I2C faults - # Of times a cell has stated an I2C error
  uint32_t framingErrors      : 8;    // # Of cell serial framing errors - # Of times the Module Controller has detected a framing error in the data stream
  uint32_t cellI2cFaultFirst  : 8;    // First cell that's reporting an I2C fault (0xff == none are)
  uint32_t UNUSED_49_63       : 16;
}CANFRM_MODULE_CELL_COMM_STATUS_1;


typedef struct {                    // 0x517 MODULE MAXIMUM ALLOWED STATE - 1 bytes
  uint8_t maximumState       : 4 ; // Maximum allowed state
  uint8_t UNUSED_4_7         : 4 ;
}CANFRM_MODULE_MAX_STATE;


typedef struct {                  // 0x518 MODULE DEREGISTER - 2 bytes
  uint8_t moduleId      : 8;      // module ID
  uint8_t controllerId  : 8;      // controller ID
}CANFRM_MODULE_DEREGISTER;


typedef struct {                  // 0x51E ALL MODULES DEREGISTER - 1 bytes
  uint8_t controllerId  : 8;      // module ID
}CANFRM_MODULE_ALL_DEREGISTER;


typedef struct {                  // 0x51F ALL MODULES ISOLATE - 1 bytes
  uint8_t controllerId  : 8;      // module ID
}CANFRM_MODULE_ALL_ISOLATE;


typedef struct {                  // 0x51D MODULE ANNOUNCE REQUEST - 1 bytes
  uint8_t controllerId  : 8;      // controller ID
}CANFRM_MODULE_ANNOUNCE_REQUEST;


#endif /* INC_CAN_FRM_MOD_H_ */
