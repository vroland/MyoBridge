/**
 * @file   MyoBridge.c
 * @author Valentin Roland (webmaster at vroland.de)
 * @date   September-October 2015
 * @brief  Implementation file of the MyoBridge task.
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

/*********************************************************************
 * INCLUDES
 */

#include "bcomdef.h"
#include "OSAL.h"
#include "OSAL_PwrMgr.h"
#include "gatt.h"
#include "ll.h"
#include "hci.h"
#include "gapgattserver.h"
#include "gattservapp.h"
#include "central.h"
#include "gapbondmgr.h"
#include "myohw.h"

#include "MyoBridge.h"
#include "myo_gatt.h"
#include "serialInterface.h"

/*********************************************************************
 * CONSTANTS
 */

/// Maximum number of scan responses
#define DEFAULT_MAX_SCAN_RES                  8

/// Scan duration in ms
#define DEFAULT_SCAN_DURATION                 4000

/// Discovey mode (limited, general, all)
#define DEFAULT_DISCOVERY_MODE                DEVDISC_MODE_ALL

/// TRUE to use active scan
#define DEFAULT_DISCOVERY_ACTIVE_SCAN         TRUE

/// TRUE to use white list during discovery
#define DEFAULT_DISCOVERY_WHITE_LIST          FALSE

/// TRUE to use high scan duty cycle when creating link
#define DEFAULT_LINK_HIGH_DUTY_CYCLE          TRUE

/// TRUE to use white list when creating link
#define DEFAULT_LINK_WHITE_LIST               FALSE

/// Default RSSI polling period in ms
#define DEFAULT_RSSI_PERIOD                   1000

/// Whether to enable automatic parameter update request when a connection is formed
#define DEFAULT_ENABLE_UPDATE_REQUEST         TRUE

/// Minimum connection interval (units of 1.25ms) if automatic parameter update request is enabled
#define DEFAULT_UPDATE_MIN_CONN_INTERVAL      400

/// Maximum connection interval (units of 1.25ms) if automatic parameter update request is enabled
#define DEFAULT_UPDATE_MAX_CONN_INTERVAL      800

/// Default passcode
#define DEFAULT_PASSCODE                      19655

/// Default GAP pairing mode
#define DEFAULT_PAIRING_MODE                  GAPBOND_PAIRING_MODE_WAIT_FOR_REQ;

/// Default MITM mode (TRUE to require passcode or OOB when pairing)
#define DEFAULT_MITM_MODE                     FALSE

/// Default bonding mode, TRUE to bond
#define DEFAULT_BONDING_MODE                  FALSE

/// Default GAP bonding I/O capabilities
#define DEFAULT_IO_CAPABILITIES               GAPBOND_IO_CAP_NO_INPUT_NO_OUTPUT

/*********************************************************************
 * GLOBAL VARIABLES
 */

/// number of scan results
static uint8 myoBridgeScanRes;

/// address of the Myo device to connect to
uint8 myo_addr[B_ADDR_LEN];
/// index of the myo device in the discovered devices list
int8 myo_index = -1;
// the BLE connection handle of the myo device when conected
static uint16 myo_conn_handle = GAP_CONNHANDLE_INIT;

/// the system status.
uint8 system_status = MYB_STATUS_UNKNOWN;
    
/// Scan result list
static gapDevRec_t discovered_devices[DEFAULT_MAX_SCAN_RES];

///store the address of the last connected device to decide if cached handles are to be used.
uint8 last_device_address[6] = {0};

/// do we have valid GATT discovery results to reuse?
uint8 descovery_was_finished = FALSE;

/*********************************************************************
 * LOCAL VARIABLES
 */

/// Task ID for internal task/event processing
uint8 MyoBridge_TaskID;   

/*********************************************************************
 * LOCAL FUNCTIONS
 */

/**
 * RSSI callback. Currently unused.
 */
static void simpleBLECentralRssiCB( uint16 connHandle, int8  rssi );

/**
 * Callback for Events regarding the central role.
 */
static uint8 simpleBLECentralEventCB( gapCentralRoleEvent_t *pEvent );

/**
 * Callback for pairing. Currently unsused.
 */
static void simpleBLECentralPairStateCB( uint16 connHandle, uint8 state, uint8 status );

/**
 * Passcode Callback. currently unused.
 */
static void simpleBLECentralPasscodeCB( uint8 *deviceAddr, uint16 connectionHandle,
                                        uint8 uiInputs, uint8 uiOutputs );

/**
 * Sends a notification of the current status through the serial interface.
 */
void sendStatusNotification();

/*********************************************************************
 * PROFILE CALLBACKS
 */

/// GAP Role Callbacks
static const gapCentralRoleCB_t MyoBridgeRoleCB =
{
  simpleBLECentralRssiCB,       ///< RSSI callback
  simpleBLECentralEventCB       ///< Event callback
};

/// Bond Manager Callbacks
static const gapBondCBs_t MyoBridgeBondCB =
{
  simpleBLECentralPasscodeCB,
  simpleBLECentralPairStateCB
};

/*********************************************************************
 * IMPLEMENTATION
 */


/**
 * Task Initialization for MyoBridge. Is automatically called during initialization.
 */
void MyoBridge_Init( uint8 task_id ) {
  MyoBridge_TaskID = task_id;
  
  setSystemStatus(MYB_STATUS_INIT);
  
  // Setup Central Profile
  {
    uint8 scanRes = DEFAULT_MAX_SCAN_RES;
    GAPCentralRole_SetParameter ( GAPCENTRALROLE_MAX_SCAN_RES, sizeof( uint8 ), &scanRes );
  }
  
  // Setup GAP
  GAP_SetParamValue( TGAP_GEN_DISC_SCAN, DEFAULT_SCAN_DURATION );
  GAP_SetParamValue( TGAP_LIM_DISC_SCAN, DEFAULT_SCAN_DURATION );
  
  // Setup the GAP Bond Manager
  {
    uint32 passkey = DEFAULT_PASSCODE;
    uint8 pairMode = DEFAULT_PAIRING_MODE;
    uint8 mitm = DEFAULT_MITM_MODE;
    uint8 ioCap = DEFAULT_IO_CAPABILITIES;
    uint8 bonding = DEFAULT_BONDING_MODE;
    GAPBondMgr_SetParameter( GAPBOND_DEFAULT_PASSCODE, sizeof( uint32 ), &passkey );
    GAPBondMgr_SetParameter( GAPBOND_PAIRING_MODE, sizeof( uint8 ), &pairMode );
    GAPBondMgr_SetParameter( GAPBOND_MITM_PROTECTION, sizeof( uint8 ), &mitm );
    GAPBondMgr_SetParameter( GAPBOND_IO_CAPABILITIES, sizeof( uint8 ), &ioCap );
    GAPBondMgr_SetParameter( GAPBOND_BONDING_ENABLED, sizeof( uint8 ), &bonding );
  }  
  
  // Initialize GATT Client
  VOID GATT_InitClient();
  
  // Register to receive incoming ATT Indications/Notifications
  GATT_RegisterForInd( MyoBridge_TaskID );
  
  // Initialize GATT attributes
  GGS_AddService( GATT_ALL_SERVICES );         // GAP
  GATTServApp_AddService( GATT_ALL_SERVICES ); // GATT attributes
  
  //disable halt during RF (needed for UART / SPI)
  HCI_EXT_HaltDuringRfCmd(HCI_EXT_HALT_DURING_RF_DISABLE);
  
  // Setup a delayed profile startup
  osal_set_event( MyoBridge_TaskID, MYB_START_DEVICE_EVT );
}
  
/**
 * Task Event Processor for MyoBridge. Is called whenever an OSAL event for this Application occurs.
 */
uint16 MyoBridge_ProcessEvent( uint8 task_id, uint16 events ) {
  if ( events & SYS_EVENT_MSG )
  {
    uint8 *pMsg;

    if ( (pMsg = osal_msg_receive( MyoBridge_TaskID )) != NULL )
    {
      switch ( ((osal_event_hdr_t*)pMsg)->event ) {
       
      //process GATT messages, done in myo_gatt.h/.c
      case GATT_MSG_EVENT: {
        
        MyoBridgeProcessGATTMsg(MyoBridge_TaskID, myo_conn_handle, (gattMsgEvent_t *) pMsg );
        
        GATT_bm_free( &((gattMsgEvent_t *)pMsg)->msg, ((gattMsgEvent_t *)pMsg)->method );
        break;
      } 
      
      //send data response to serial interface
      case MYB_DATA_RSP_EVENT: {
        MYBOSALDataRsp_t* msg = (MYBOSALDataRsp_t*) pMsg;
        sendDataRsp(&msg->rsp);
        osal_mem_free(msg->rsp.pData); 
        break;
      }
      
      //send write response to serial interface
      case MYB_WRITE_RSP_EVENT: {
        MYBOSALPingRsp_t* msg = (MYBOSALPingRsp_t*) pMsg;
        sendPingRsp(&msg->rsp);
        break;
      }
      
      //process events from serial interface
      case MYB_SERIAL_EVENT: {
        
        MYBOSALSerialPacket_t* msg = (MYBOSALSerialPacket_t*)pMsg;
        uint8 type = msg->pData[0];
        uint8* packet = &msg->pData[0];
        
        //switch by package type, perform actions
        switch (type) {
        case MYB_CMD_PING: {
          MYBPingRsp_t rsp;
          rsp.hdr.type = MYB_RSP_PING;
          rsp.hdr.len = 0;
          sendPingRsp(&rsp);        
          break;
        }
        case MYB_CMD_READ_CHAR: {
          MYBReadCmd_t* cmd = (MYBReadCmd_t*) packet;
          requestCharValue(MyoBridge_TaskID, myo_conn_handle, (MyoCharacteristicIndex_t) cmd->index);  
          break;
        }
        case MYB_CMD_WRITE_CHAR: {
          MYBWriteCmd_t* cmd = (MYBWriteCmd_t*) packet;
          writeCharByIndex(MyoBridge_TaskID, myo_conn_handle, &packet[3], cmd->len, (MyoCharacteristicIndex_t) cmd->index);
          break;
        }
        case MYB_CMD_SET_ASYNC_STATUS: {
          MYBAsyncStatusCmd_t* cmd = (MYBAsyncStatusCmd_t*) packet;
          setAsyncStatus(MyoBridge_TaskID, myo_conn_handle, cmd->status, (MyoCharacteristicIndex_t) cmd->index);
          break;
        }
        case MYB_CMD_GET_STATUS: {
          sendStatusNotification();
          break;
        }
        
        }
        
        //free message data
        osal_mem_free(msg->pData); 
        break;
      }
      default: {
        break;
      }
      }

      // Release the OSAL message
      VOID osal_msg_deallocate( pMsg );
    }

    // return unprocessed events
    return (events ^ SYS_EVENT_MSG);
  }
  
  //device startup
  if ( events & MYB_START_DEVICE_EVT ) {
    // Start the Device
    VOID GAPCentralRole_StartDevice( (gapCentralRoleCB_t *) &MyoBridgeRoleCB );

    // Register with bond manager after starting device
    GAPBondMgr_Register( (gapBondCBs_t *) &MyoBridgeBondCB );
    
    // start discovery
    osal_set_event( MyoBridge_TaskID, MYB_START_DISCOVERY_EVT );
    return (events ^ MYB_START_DEVICE_EVT);
  }
  
  //begin device scanning
  if (events & MYB_START_DISCOVERY_EVT) {
    setSystemStatus(MYB_STATUS_SCANNING);
    
    
    // Start discovery
    GAPCentralRole_StartDiscovery( DEFAULT_DISCOVERY_MODE,
                                   DEFAULT_DISCOVERY_ACTIVE_SCAN,
                                   DEFAULT_DISCOVERY_WHITE_LIST );
    
    return (events ^ MYB_START_DISCOVERY_EVT);
  }
  
  //begin linking (connecting) to myo
  if ( events & MYB_INIT_LINK_EVT ) {
    
    GAPCentralRole_CancelDiscovery();
    
    // have we discovered a myo device?
    if (myo_index>=0) {
      
      setSystemStatus(MYB_STATUS_CONNECTING);
      gapDevRec_t* dev = &discovered_devices[myo_index];
        
      GAPCentralRole_EstablishLink( DEFAULT_LINK_HIGH_DUTY_CYCLE,
                                        DEFAULT_LINK_WHITE_LIST,
                                        dev->addrType, dev->addr );
      
    } else {
      
      osal_set_event( MyoBridge_TaskID, MYB_START_DISCOVERY_EVT );
    }
    return (events ^ MYB_INIT_LINK_EVT);
  }
  
  //do GATT work
  if (events & MYB_GATT_WORKING_EVT) {
    //is GATT discovery started?
    if (getGATTState() == MYB_INIT) {
      setGATTState(MYB_DIS_CONTRL_SERV);
      descovery_was_finished = FALSE;
      setSystemStatus(MYB_STATUS_DISCOVERING);  
    }
    
    MyoBridge_ProcessGATTScanEvt(MyoBridge_TaskID ,myo_conn_handle);
    return (events ^ MYB_GATT_WORKING_EVT);
  }
  
  //GATT work is ready
  if (events & MYB_GATT_READY_EVT) {
    //remember that the discovery was finished for later use
    descovery_was_finished = TRUE;
    
    setSystemStatus(MYB_STATUS_READY);
    return (events ^ MYB_GATT_READY_EVT);
  }

  // Discard unknown events
  return 0;
}

/**
 * RSSI callback. Currently unused.
 */
static void simpleBLECentralRssiCB( uint16 connHandle, int8 rssi ) {
  prints("RSSI: ");
  prints((char*) &rssi);
}



/**
 * Callback for Events regarding the central role.
 */
uint8 simpleBLECentralEventCB( gapCentralRoleEvent_t *pEvent ) {
  switch ( pEvent->gap.opcode ) {
  
  case GAP_DEVICE_INIT_DONE_EVENT: {
    break;
  }
  
  case GAP_LINK_ESTABLISHED_EVENT: {
    if ( pEvent->gap.hdr.status == SUCCESS ) {          
      myo_conn_handle = pEvent->linkCmpl.connectionHandle;  
      if ((descovery_was_finished) && (osal_memcmp(&last_device_address[0], &discovered_devices[myo_index].addr, B_ADDR_LEN)) == TRUE) {
        setGATTState(MYB_READY);
      } else {
        setGATTState(MYB_INIT);
      }
      
      //copy new address for cache testing
      osal_memcpy(&last_device_address[0], &discovered_devices[myo_index].addr, 6);
      
      osal_start_timerEx( MyoBridge_TaskID, MYB_GATT_WORKING_EVT, 500 );
    } else {
      myo_index = -1;
      myo_conn_handle = GAP_CONNHANDLE_INIT;
      osal_set_event( MyoBridge_TaskID, MYB_START_DISCOVERY_EVT );
    }
    
    break;
  }
  
  case GAP_LINK_TERMINATED_EVENT: {
    myo_index = -1;
    myo_conn_handle = GAP_CONNHANDLE_INIT;
    setGATTState(MYB_INIT);
    osal_set_event( MyoBridge_TaskID, MYB_START_DISCOVERY_EVT );
    break;
  }
  
  case GAP_DEVICE_INFO_EVENT: {
    
    uint8 uuid[ATT_UUID_SIZE] = MYO_SERVICE_INFO_UUID;
    uint8 adType = pEvent->deviceInfo.pEvtData[0];
    
    //myo advertises device name
    if (adType == GAP_ADTYPE_LOCAL_NAME_COMPLETE) {
      
      //myo sends 128bit UUID in advertising package -> is this a myo?
      uint8 cmp = osal_memcmp(&pEvent->deviceInfo.pEvtData[15], &uuid[0], ATT_UUID_SIZE);
      if (cmp == TRUE) {
        osal_memcpy(myo_addr, pEvent->deviceInfo.addr, B_ADDR_LEN);
      }
    }
    
    break;
  }
  
  case GAP_DEVICE_DISCOVERY_EVENT: {
    // Copy results
    myoBridgeScanRes = pEvent->discCmpl.numDevs;
    osal_memcpy( discovered_devices, pEvent->discCmpl.pDevList,
                 (sizeof( gapDevRec_t ) * pEvent->discCmpl.numDevs) );
    
    for (int i=0;i<myoBridgeScanRes;i++) {
      if (osal_memcmp(myo_addr, discovered_devices[i].addr, B_ADDR_LEN) == TRUE) {
         myo_index = i;
      }
    }
    
    // start connection
    osal_set_event( MyoBridge_TaskID, MYB_INIT_LINK_EVT );
  }
  break;
    
  default:
    break;
  }
  return 0;
}

/**
 * Sends a notification of the current status through the serial interface.
 */
void sendStatusNotification() {
  MYBStatusRsp_t rsp;
  rsp.hdr.type = MYB_RSP_STATUS;
  rsp.hdr.len = 1;
  rsp.status = system_status;
  sendStatusRsp(&rsp);
}

/**
 * Sets the system status variable.
 */
void setSystemStatus(uint8 state) {
  //notify the status change via uart when changed
  if (state != system_status) {
    system_status = state;
    sendStatusNotification();
  } else {
    system_status = state;
  }
}

/**
 * Callback for pairing. Currently unsused.
 */
static void simpleBLECentralPairStateCB( uint16 connHandle, uint8 state, uint8 status ) {
  if ( state == GAPBOND_PAIRING_STATE_STARTED ) {
    //maybe status update?
  } else if ( state == GAPBOND_PAIRING_STATE_COMPLETE ) {
    
    if ( status == SUCCESS ) {
      //maybe status update?
    } else {
      //maybe status update?
    }
  } else if ( state == GAPBOND_PAIRING_STATE_BONDED ) {
    if ( status == SUCCESS ) {
      //maybe status update?
    }
  }
}

/**
 * Passcode Callback. currently unused.
 */
static void simpleBLECentralPasscodeCB( uint8 *deviceAddr, uint16 connectionHandle,
                                        uint8 uiInputs, uint8 uiOutputs ) {
  
}
