/**
 * @file   myo_gatt.c
 * @author Valentin Roland (webmaster at vroland.de)
 * @date   September-October 2015
 * @brief  Implementation file of all functions regarding GATT actions and discovery.
 *
 * This file is part of the MyoBridge project. MyoBridge is a firmware for the CC2541/40 from Texas Instruments,
 * particularly on the HM-11 BLE Module. With this software, such a device can operate as an intermediate mediator
 * between a Myo Gesture Control Armband (https://www.myo.com/) and another device capable of sending and receiving
 * data via a two-wire (RX/TX) serial connection, like Arduino. The usage with an Arduino device will be the primary
 * application and the target platform of a high-level library to use together with the firmware. 
 * 
 * This project is developed for research and educational purposes at Victoria University of Wellington, 
 * School of Engineering and Computer Science.
 */


#include "myo_gatt.h"
#include "MyoBridge.h"
#include "serialinterface.h"
#include "myohw.h"
#include <stdio.h>
#include "gatt_uuid.h"

/// The current state of the GATT module of the application. 
///Controls the flow of this part of the Application.
MyoGATTState_t gatt_action_state = MYB_INIT;

/// The discovered Myo BLE service and characteristic handles. 
///Can be reused when reconnecting with the same device, saving discovery time.
MyoHandles_t myo_handles;

///store index for returning with read response
uint8 index_cache;

#define READ_BY_TYPE_RSP_SKIP 3

/// Lookup Table for 16 bit Characteristic UUID by MyoCharacteristicIndex_t
uint16 myo_uuid_lookup[MYO_NUM_CHARACTERISTICS] = {
    ControlService,
    0,
    MyoInfoCharacteristic,
    FirmwareVersionCharacteristic,
    CommandCharacteristic,
    ImuDataService,
    0,
    IMUDataCharacteristic,
    MotionEventCharacteristic,
    ClassifierService,
    0,
    ClassifierEventCharacteristic,
    EmgDataService,
    0,
    EmgData0Characteristic,
    EmgData1Characteristic,
    EmgData2Characteristic,
    EmgData3Characteristic,
    0,
    0,
    BatteryLevelCharacteristic,
};

///Storage for Client Char Config Descriptor Handles (aka notification/indication switch) for every characteristic, if existing
uint16 myo_cccd_handles[MYO_NUM_CHARACTERISTICS] = {0};

///Lookup Table for position of service start handle in MyoHandles_t by MyoCharacteristicIndex_t 
uint8 myo_lookup_service_handle[MYO_NUM_CHARACTERISTICS] = {
    0,
    0,
    0, ///< MyoInfoCharacteristic
    0, ///< FirmwareVersionCharacteristic
    0, ///< CommandCharacteristic
    0,
    0,
    5, ///< IMUDataCharacteristic
    5, ///< MotionEventCharacteristic
    0,
    0,
    9, ///< ClassifierEventCharacteristic
    0,
    0,
    12, ///< EmgData0Characteristic
    12, ///< EmgData1Characteristic
    12, ///< EmgData2Characteristic
    12, ///< EmgData3Characteristic
    0,
    0,
    18, ///< BatteryLevelCharacteristic
};

///Lookup Table for MyoCharacteristicIndex_t by MyoGATTState_t
uint8 myo_service_state_service_addr_lookup[MYO_NUM_SERVICES] = {
  MYO_SERV_CONTROL,
  MYO_SERV_IMU_DATA,
  MYO_SERV_CLASSIFIER,
  MYO_SERV_EMG,
  MYO_SERV_BATTERY
};

/// Array instance of the Myo base UUID.
const uint8 myo_base_uuid[16] = MYO_SERVICE_BASE_UUID;

/**********************************
 * LOCAL FUNCTIONS
 *********************************/
 
 /**
 * @brief   Extracts data for characteristic handle discovery from GATT responses.
 *
 * @return  none
 */
void DiscoverServHandles( uint8 MyoBridge_TaskID, gattMsgEvent_t *pMsg, MyoGATTState_t nextState);

/**
 * @brief   Extracts data for characteristic handle discovery from GATT responses.
 *
 * @return  none
 */
void DiscoverCharHandles( uint8 MyoBridge_TaskID, gattMsgEvent_t *pMsg, MyoGATTState_t nextState);

/**
 * @brief   Extracts data for client characteristic configuration descriptor (CCCD) handle discovery from GATT responses.
 *
 * @return  none
 */
void DiscoverCCCDHandles( uint8 MyoBridge_TaskID, gattMsgEvent_t *pMsg, MyoGATTState_t nextState);



///Myo needs 128bit UUIDs, insert short service id in base UUID
void generate_full_uuid(uint16 short_uuid, uint8* UUID) {
  
  osal_memcpy(UUID, &myo_base_uuid[0], 16);
  
  UUID[12] = LO_UINT16(short_uuid);
  UUID[13] = HI_UINT16(short_uuid);
}

///Build a C string from a 128bit UUID
char *uuid2str( uint8 *pAddr )
{
  uint8       i;
  char        hex[] = "0123456789ABCDEF";
  static char str[33];
  char        *pStr = str;
  
  // Start from end of addr
  pAddr += 16;
  
  for ( i = 16; i > 0; i-- )
  {
    *pStr++ = hex[*--pAddr >> 4];
    *pStr++ = hex[*pAddr & 0x0F];
  }
  
  *pStr = 0;
  
  return str;
}

///String representation of a uint8 in hex.
char *hex22str( uint8 *pAddr )
{
  uint8       i;
  char        hex[] = "0123456789ABCDEF";
  static char str[3];
  char        *pStr = str;
  
  // Start from end of addr
  pAddr += 2;
  
  for ( i = 2; i > 0; i-- )
  {
    *pStr++ = hex[*--pAddr >> 4];
    *pStr++ = hex[*pAddr & 0x0F];
  }
  
  *pStr = 0;
  
  return str;
}

/**
 * request the value of a characteristic. the value will be sent back via an OSAL message
 */
uint8 requestCharValue(uint8 MyoBridge_TaskID, uint16 myo_conn_handle, MyoCharacteristicIndex_t index) {
  if (gatt_action_state!=MYB_READY) return bleNotReady;
  
  attReadReq_t req;
  req.handle = ((uint16*)&myo_handles)[index];
  index_cache = index;
  uint8 status = GATT_ReadCharValue( myo_conn_handle, &req, MyoBridge_TaskID );
  
  if (status != SUCCESS) return status;
  gatt_action_state = MYB_READING_CHAR;
  setSystemStatus(MYB_STATUS_BUSY);
  return SUCCESS;
}
  
/**
 * write to a characteristic.
 */
uint8 writeChar(uint8 MyoBridge_TaskID, uint16 myo_conn_handle, uint8* pCommand, uint8 len, uint16 handle) {
  if (gatt_action_state!=MYB_READY) return bleNotReady;
  
  // Do a write
  attWriteReq_t req;
  uint8 status;
  req.pValue = GATT_bm_alloc( myo_conn_handle, ATT_WRITE_REQ, len, NULL );
  osal_memcpy(&req.pValue[0], pCommand, len);
  
  if ( req.pValue != NULL )
  {
    req.handle = handle;
    req.len = len;
    req.sig = 0;
    req.cmd = 0;
    status = GATT_WriteCharValue( myo_conn_handle, &req , MyoBridge_TaskID);
    if ( status != SUCCESS ) {
      GATT_bm_free( (gattMsg_t *)&req, ATT_WRITE_REQ );
    } else {
      setSystemStatus(MYB_STATUS_BUSY);
      gatt_action_state = MYB_WRITING_CHAR;   
    }
  } else {
    status = bleMemAllocError;
  }
  
  return status;
}

/**
 * write to the characteristic of the given index.
 */
uint8 writeCharByIndex(uint8 MyoBridge_TaskID, uint16 myo_conn_handle, uint8* pCommand, uint8 len, MyoCharacteristicIndex_t index) {
  return writeChar(MyoBridge_TaskID, myo_conn_handle, pCommand, len, ((uint16*)&myo_handles)[(uint8)index]);
}

/**
 * send a command to the command characteristic.
 */
uint8 sendCommand(uint8 MyoBridge_TaskID, uint16 myo_conn_handle, uint8* pCommand, uint8 len) {
  return writeChar(MyoBridge_TaskID, myo_conn_handle, pCommand, len, ((uint16*)&myo_handles)[(uint8)MYO_CHAR_COMMAND]);
}

/**
 * set the status of asynchronous messages (nofifications, indications) of a characteristic
 */
uint8 setAsyncStatus(uint8 MyoBridge_TaskID, uint16 myo_conn_handle, uint16 status, MyoCharacteristicIndex_t index) {
  uint8 cmd[2] = {LO_UINT16(status), HI_UINT16(status)};
  return writeChar(MyoBridge_TaskID, myo_conn_handle, (uint8*)&cmd, 2, myo_cccd_handles[index]);
}
      
/**
 * @brief   This function coordinates the GATT discovery by sending discovery request to the GATT layer.
 *          The responses are handled in MyoBridgeProcessGATTMsg.
 *
 * @param   MyoBridge_TaskID Task id of the main application.
 * @param   myo_conn_handle Myo BLE connection handle.
 *
 * @return  status
 */
uint16 MyoBridge_ProcessGATTScanEvt(uint8 MyoBridge_TaskID, uint16 myo_conn_handle) {
  
  switch (gatt_action_state) {
    
    case MYB_DIS_CONTRL_SERV: {
      uint8 uuid[ATT_UUID_SIZE];
      generate_full_uuid(ControlService, uuid);
      GATT_DiscPrimaryServiceByUUID( myo_conn_handle, uuid, ATT_UUID_SIZE, MyoBridge_TaskID );
      break;
    }
    
    case MYB_DIS_IMU_SERV: {
      uint8 uuid[ATT_UUID_SIZE];
      generate_full_uuid(ImuDataService, uuid);
      GATT_DiscPrimaryServiceByUUID( myo_conn_handle, uuid, ATT_UUID_SIZE, MyoBridge_TaskID );
      break;
    }
    
    case MYB_DIS_CLASS_SERV: {
      uint8 uuid[ATT_UUID_SIZE];
      generate_full_uuid(ClassifierService, uuid);
      GATT_DiscPrimaryServiceByUUID( myo_conn_handle, uuid, ATT_UUID_SIZE, MyoBridge_TaskID );
      break;
    }
    
    case MYB_DIS_EMG_SERV: {
      uint8 uuid[ATT_UUID_SIZE];
      generate_full_uuid(EmgDataService, uuid);
      GATT_DiscPrimaryServiceByUUID( myo_conn_handle, uuid, ATT_UUID_SIZE, MyoBridge_TaskID );
      break;
    }
    
    case MYB_DIS_BATT_SERV: {
      // Battery Service has other Prefix, use short UUID
      uint8 uuid[ATT_BT_UUID_SIZE] = { LO_UINT16(BatteryService),  HI_UINT16(BatteryService) };
      GATT_DiscPrimaryServiceByUUID( myo_conn_handle, uuid, ATT_BT_UUID_SIZE, MyoBridge_TaskID );
      break;
    }
    
    case MYB_DES_CONTRL_SERV: {   
      uint8 status = GATT_DiscAllCharDescs( myo_conn_handle, myo_handles.controlServiceStart, myo_handles.controlServiceEnd, MyoBridge_TaskID );
      break;
    }
    
    case MYB_DES_IMU_SERV: {   
      uint8 status = GATT_DiscAllCharDescs( myo_conn_handle, myo_handles.imuServiceStart, myo_handles.imuServiceEnd, MyoBridge_TaskID );
      break;
    }
    
    case MYB_DES_CLASS_SERV: {   
      uint8 status = GATT_DiscAllCharDescs( myo_conn_handle, myo_handles.classifierServiceStart, myo_handles.classifierServiceEnd, MyoBridge_TaskID );
      break;
    }
    
    case MYB_DES_EMG_SERV: {   
      uint8 status = GATT_DiscAllCharDescs( myo_conn_handle, myo_handles.emgServiceStart, myo_handles.emgServiceEnd, MyoBridge_TaskID );
      break;
    }
    
    case MYB_DES_BATT_SERV: {   
      uint8 status = GATT_DiscAllCharDescs( myo_conn_handle, myo_handles.batteryServiceStart, myo_handles.batteryServiceEnd, MyoBridge_TaskID );
      break;
    }
    
    case MYB_DIS_CONTRL_CHAR: {   
      uint8 status = GATT_DiscAllChars( myo_conn_handle, myo_handles.controlServiceStart, myo_handles.controlServiceEnd, MyoBridge_TaskID );
      break;
    }
    
    case MYB_DIS_IMU_CHAR: {   
      uint8 status = GATT_DiscAllChars( myo_conn_handle, myo_handles.imuServiceStart, myo_handles.imuServiceEnd, MyoBridge_TaskID );
      break;
    }
    
    case MYB_DIS_CLASS_CHAR: {   
      uint8 status = GATT_DiscAllChars( myo_conn_handle, myo_handles.classifierServiceStart, myo_handles.classifierServiceEnd, MyoBridge_TaskID );
      break;
    }
    
    case MYB_DIS_EMG_CHAR: {   
      uint8 status = GATT_DiscAllChars( myo_conn_handle, myo_handles.emgServiceStart, myo_handles.emgServiceEnd, MyoBridge_TaskID );
      break;
    }
    
    case MYB_DIS_BATT_CHAR: {   
      uint8 status = GATT_DiscAllChars( myo_conn_handle, myo_handles.batteryServiceStart, myo_handles.batteryServiceEnd, MyoBridge_TaskID );
      break;
    }
    
    // called after an completed event
    case MYB_READY: {
      osal_set_event( MyoBridge_TaskID, MYB_GATT_READY_EVT );
      break;
    }
  }
  
  return 0;
}


/**
 * @brief   handle GATT responses for discovery, read/write characteristics and notifications.
 *
 * @return  none
 */
void MyoBridgeProcessGATTMsg( uint8 MyoBridge_TaskID, uint16 myo_conn_handle, gattMsgEvent_t *pMsg) {
  
  if (pMsg->method == ATT_ERROR_RSP) {
    setSystemStatus(MYB_STATUS_READY);
    //prints("GATT Message Error!\n");
    return;
  }
  
  /***************************************************
   * Notifications and Indications
   **************************************************/
  
  if (pMsg->method == ATT_HANDLE_VALUE_NOTI) {
    
    uint16 handle = pMsg->msg.handleValueNoti.handle;
    //reverse handle lookup
    uint8 index=0;
    for (index=0; index<MYO_NUM_CHARACTERISTICS; index++) {
      if (((uint16*)&myo_handles)[index] == handle) {
        break;
      }
    }
    
    //send notification data via OSAL system event
    MYBOSALDataRsp_t* msg = (MYBOSALDataRsp_t*)osal_msg_allocate(sizeof(MYBOSALDataRsp_t));
    if (msg) {
      msg->hdr.event = MYB_DATA_RSP_EVENT;
      msg->hdr.status = SUCCESS;
      msg->rsp.hdr.len = pMsg->msg.handleValueNoti.len + 1;
      msg->rsp.hdr.type = MYB_RSP_VALUE;
      msg->rsp.characteristic = (uint8) index;
      msg->rsp.pData = osal_mem_alloc(pMsg->msg.readRsp.len);
      osal_memcpy(msg->rsp.pData, pMsg->msg.handleValueNoti.pValue, pMsg->msg.handleValueNoti.len);
      
      osal_msg_send(MyoBridge_TaskID, (uint8*) msg);
    }
    return;
  }
  
  if (pMsg->method == ATT_HANDLE_VALUE_IND) {
      
    uint16 handle = pMsg->msg.handleValueInd.handle;
    //reverse handle lookup
    uint8 index=0;
    for (index=0; index<MYO_NUM_CHARACTERISTICS; index++) {
      if (((uint16*)&myo_handles)[index] == handle) {
        break;
      }
    }
    
    //send notification data via OSAL system event
    MYBOSALDataRsp_t* msg = (MYBOSALDataRsp_t*)osal_msg_allocate(sizeof(MYBOSALDataRsp_t));
    if (msg) {
      msg->hdr.event = MYB_DATA_RSP_EVENT;
      msg->hdr.status = SUCCESS;
      msg->rsp.hdr.len = pMsg->msg.handleValueInd.len + 1;
      msg->rsp.hdr.type = MYB_RSP_VALUE;
      msg->rsp.characteristic = (uint8) index;
      msg->rsp.pData = osal_mem_alloc(pMsg->msg.readRsp.len);
      osal_memcpy(msg->rsp.pData, pMsg->msg.handleValueInd.pValue, pMsg->msg.handleValueInd.len);
      
      osal_msg_send(MyoBridge_TaskID, (uint8*) msg);
    }
    
    ATT_HandleValueCfm(myo_conn_handle);
    return;
  }
  
  switch (gatt_action_state) {
    
  /***************************************************
   * Discover Services
   **************************************************/
    
  case MYB_DIS_CONTRL_SERV: {
    DiscoverServHandles( MyoBridge_TaskID, pMsg, MYB_DIS_CONTRL_CHAR);
    break;
  }
  
  case MYB_DIS_IMU_SERV: {
    DiscoverServHandles( MyoBridge_TaskID, pMsg, MYB_DIS_IMU_CHAR);
    break;
  }
  
  case MYB_DIS_CLASS_SERV: {
    DiscoverServHandles( MyoBridge_TaskID, pMsg, MYB_DIS_CLASS_CHAR);
    break;
  }
  
  case MYB_DIS_EMG_SERV: {
    DiscoverServHandles( MyoBridge_TaskID, pMsg, MYB_DIS_EMG_CHAR);
    break;
  }
  
  case MYB_DIS_BATT_SERV: {
    DiscoverServHandles( MyoBridge_TaskID, pMsg, MYB_DIS_BATT_CHAR);
    break;
  }
  
  /***************************************************
   * Discover Characteristics
   **************************************************/
  
  case MYB_DIS_CONTRL_CHAR: {
    DiscoverCharHandles(MyoBridge_TaskID, pMsg, MYB_DES_CONTRL_SERV);
    break;
  }
  
  case MYB_DIS_IMU_CHAR: {
    DiscoverCharHandles(MyoBridge_TaskID, pMsg, MYB_DES_IMU_SERV);
    break;
  }
  
  case MYB_DIS_CLASS_CHAR: {
    DiscoverCharHandles(MyoBridge_TaskID, pMsg, MYB_DES_CLASS_SERV);
    break;
  }
  
  case MYB_DIS_EMG_CHAR: {
    DiscoverCharHandles(MyoBridge_TaskID, pMsg, MYB_DES_EMG_SERV);
    break;
  }
  
  case MYB_DIS_BATT_CHAR: {
    DiscoverCharHandles(MyoBridge_TaskID, pMsg, MYB_DES_BATT_SERV);
    break;
  }
  
  /***************************************************
   * Discover CCCDs (aka Notification/Indication Switch)
   **************************************************/
  
  case MYB_DES_CONTRL_SERV: {
    DiscoverCCCDHandles( MyoBridge_TaskID, pMsg, MYB_DIS_IMU_SERV);
    break;
  }
  
  case MYB_DES_IMU_SERV: {
    DiscoverCCCDHandles( MyoBridge_TaskID, pMsg, MYB_DIS_CLASS_SERV);
    break;
  }
  
  case MYB_DES_CLASS_SERV: {
    DiscoverCCCDHandles( MyoBridge_TaskID, pMsg, MYB_DIS_EMG_SERV);
    break;
  }
  
  case MYB_DES_EMG_SERV: {
    DiscoverCCCDHandles( MyoBridge_TaskID, pMsg, MYB_DIS_BATT_SERV);
    break;
  }
  
  case MYB_DES_BATT_SERV: {
    DiscoverCCCDHandles( MyoBridge_TaskID, pMsg, MYB_READY);
    break;
  }
  
  /***************************************************
   * Read Characteristics
   **************************************************/
  
  case MYB_READING_CHAR: {
    
    //send notification data via OSAL system event
    MYBOSALDataRsp_t* msg = (MYBOSALDataRsp_t*)osal_msg_allocate(sizeof(MYBOSALDataRsp_t));
    if (msg) {
      msg->hdr.event = MYB_DATA_RSP_EVENT;
      msg->hdr.status = SUCCESS;
      msg->rsp.hdr.len = pMsg->msg.readRsp.len + 1;
      msg->rsp.hdr.type = MYB_RSP_VALUE;
      msg->rsp.characteristic = index_cache;
      msg->rsp.pData = osal_mem_alloc(pMsg->msg.readRsp.len);
      osal_memcpy(msg->rsp.pData, pMsg->msg.readRsp.pValue, pMsg->msg.readRsp.len);
      
      osal_msg_send(MyoBridge_TaskID, (uint8*) msg);
    }
    
    if ((pMsg->hdr.status == SUCCESS) || (pMsg->hdr.status == bleTimeout)) {
      gatt_action_state = MYB_READY;
      osal_set_event( MyoBridge_TaskID, MYB_GATT_WORKING_EVT );
    }
    break;
  }
  
  /***************************************************
   * Write Characteristics
   **************************************************/
  
  case MYB_WRITING_CHAR: {
    if ((pMsg->method == ATT_WRITE_RSP) && 
        ((pMsg->hdr.status == SUCCESS) || (pMsg->hdr.status == bleTimeout))) {
          
      //send notification data via OSAL system event
      MYBOSALPingRsp_t* msg = (MYBOSALPingRsp_t*)osal_msg_allocate(sizeof(MYBOSALPingRsp_t));
      if (msg) {

        msg->hdr.event = MYB_WRITE_RSP_EVENT;
        msg->hdr.status = SUCCESS;
        msg->rsp.hdr.type = MYB_RSP_PING;
        msg->rsp.hdr.len = 0;
        
        osal_msg_send(MyoBridge_TaskID, (uint8*) msg);
      }
    
      gatt_action_state = MYB_READY;
      osal_set_event( MyoBridge_TaskID, MYB_GATT_WORKING_EVT );
    } else {
      prints("Write Error!\n");
    }
    
    break;
  }
  
  case MYB_READY: {    
    break;
  }
  
  }
  
}

/**
 * @brief   Extracts data for characteristic handle discovery from GATT responses.
 *
 * @return  none
 */
void DiscoverCharHandles( uint8 MyoBridge_TaskID, gattMsgEvent_t *pMsg, MyoGATTState_t nextState) {
  if (pMsg->msg.readByTypeRsp.numPairs == 1) {
    uint8 * h_info = pMsg->msg.readByTypeRsp.pDataList;
    
    
    //16 bit UUID
    uint16 short_uuid = BUILD_UINT16(h_info[READ_BY_TYPE_RSP_SKIP + 2], h_info[READ_BY_TYPE_RSP_SKIP + 3]); 
    //128 bit UUID
    if (pMsg->msg.readByTypeRsp.len > 7) {
       short_uuid = BUILD_UINT16(h_info[READ_BY_TYPE_RSP_SKIP + 14], h_info[READ_BY_TYPE_RSP_SKIP + 15]);
    }
    
    uint8 i=0;
    //find characteristic index
    for (i=0;i<MYO_NUM_CHARACTERISTICS + 1;i++) { 
      if (myo_uuid_lookup[i] == short_uuid) break;
    }
    
    //UUID found
    if (i<MYO_NUM_CHARACTERISTICS) {
      ((uint16*)&myo_handles)[i] = BUILD_UINT16(h_info[READ_BY_TYPE_RSP_SKIP], h_info[READ_BY_TYPE_RSP_SKIP + 1]);
    }
  } else {
    if (pMsg->msg.readByTypeRsp.numPairs > 1) {
      prints("char declaration too big!\n");
    }
  }

  if (pMsg->hdr.status == bleProcedureComplete) {
    gatt_action_state = nextState;
    
    //trigger next event in ProcessEvents
    osal_set_event( MyoBridge_TaskID, MYB_GATT_WORKING_EVT );
    return;
  }
}

/**
 * @brief   Extracts data for service handle discovery from GATT responses.
 *
 * @return  none
 */
void DiscoverServHandles( uint8 MyoBridge_TaskID, gattMsgEvent_t *pMsg, MyoGATTState_t nextState) {
  if (pMsg->msg.findByTypeValueRsp.numInfo == 1 ) {
    ((uint16*)&myo_handles)[myo_service_state_service_addr_lookup[gatt_action_state]]     = ATT_ATTR_HANDLE(pMsg->msg.findByTypeValueRsp.pHandlesInfo, 0);
    ((uint16*)&myo_handles)[myo_service_state_service_addr_lookup[gatt_action_state] + 1] = ATT_GRP_END_HANDLE(pMsg->msg.findByTypeValueRsp.pHandlesInfo, 0);
  }
  
  if (pMsg->msg.findByTypeValueRsp.numInfo > 1) {
    prints("GATT Discovery Error!\n");
    return;
  }
  
  if (pMsg->hdr.status == bleProcedureComplete) {
    gatt_action_state = nextState;
    
    //trigger next event in ProcessEvents
    osal_set_event( MyoBridge_TaskID, MYB_GATT_WORKING_EVT );
    return;
  }
}

/**
 * @brief   Extracts data for client characteristic configuration descriptor (CCCD) handle discovery from GATT responses.
 *
 * @return  none
 */
void DiscoverCCCDHandles( uint8 MyoBridge_TaskID, gattMsgEvent_t *pMsg, MyoGATTState_t nextState) {
  
  attFindInfoRsp_t *pRsp = &(pMsg->msg.findInfoRsp);
  
  for (uint8 i = 0; i < pMsg->msg.findInfoRsp.numInfo; i++ ) {

    // Look for CCCD
    if ( ATT_BT_PAIR_UUID( pRsp->pInfo, i ) == GATT_CLIENT_CHAR_CFG_UUID ) {
      //CCCD handle
      uint16 handle = ATT_BT_PAIR_HANDLE( pRsp->pInfo, i );
      
      //lookup current service start and end handle
      uint8 svc_index = myo_service_state_service_addr_lookup[gatt_action_state - MYO_DISCOVER_DESC_OFFSET];
      uint16 svc_end = ((uint16*)&myo_handles)[svc_index + 1];
      uint8 svc_end_index;
      
      //can we look up the index of the next service?
      if (gatt_action_state - MYO_DISCOVER_DESC_OFFSET + 1 < MYO_NUM_SERVICES) {
        svc_end_index = myo_service_state_service_addr_lookup[gatt_action_state - MYO_DISCOVER_DESC_OFFSET + 1] - 1;
      
      //last service
      } else {
        svc_end_index = MYO_NUM_CHARACTERISTICS - 1;
      }
      
      //if handle equals end service group -> assign to last characteristic
      if (handle == svc_end) {
        myo_cccd_handles[svc_end_index] = handle;
      } else {
        //associate with first characteristic value handle that is lower than the handle (descriptors have higher handles than values)
        for (int j=svc_index + 2; j<=svc_end_index; j++) {
          //higher char value handle found or
          if ((handle <= ((uint16*)&myo_handles)[j])) {
            myo_cccd_handles[j-1] = handle;
            break;
          }
        }
      }
    }
  }
  
  if (pMsg->hdr.status == bleProcedureComplete) {
    gatt_action_state = nextState;
    
    //trigger next event in ProcessEvents
    osal_set_event( MyoBridge_TaskID, MYB_GATT_WORKING_EVT );
    return;
  }
}

/**
* @brief Set the MyoGATTState
*/
void setGATTState(MyoGATTState_t state) {
  gatt_action_state = state;
}

/**
* @brief Get the MyoGATTState
*/
MyoGATTState_t getGATTState() {
  return gatt_action_state;
} 