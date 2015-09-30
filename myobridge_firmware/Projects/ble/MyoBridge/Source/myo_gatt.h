/**
 * @file   myo_gatt.h
 * @author Valentin Roland (webmaster at vroland.de)
 * @date   September-October 2015
 * @brief  Header file for the GATT handling functions of MyoBridge. Contains constant and type definitions for GATT handling.
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
#include "gatt.h"
  
/*********************************************************************
 * CONSTANTS
 */
 
/**
 * @defgroup gatt_types MyoBridge GATT Data Structures
 * These types are used for GATT discovery and operations. 
 * MyoBridge uses a MyoGATTState_t to determine how to react to current events.
 * @{
 */
	
///offset of the respective characteristic discovery state from the service discovery state (MyoGATTState_t).
#define MYO_DISCOVER_CHAR_OFFSET 0x10
///offset of the respective characteristic descriptor discovery state from the service discovery state (MyoGATTState_t).
#define MYO_DISCOVER_DESC_OFFSET 0x20

///number of characteristics and services
#define MYO_NUM_CHARACTERISTICS 21
///number of services 
#define MYO_NUM_SERVICES 5

/**
 * GATT program states. DO NOT MODIFY! (Service number used as lookup index: 0xXZ-> Z: Service, X: Serv/Char/Desc)
 */
 typedef enum {
    MYB_INIT = 0xA0,                    ///<initialize GATT discovery
    MYB_START_SERVICE_DISC = 0xA1,      ///<start GATT discovery
    MYB_DIS_CONTRL_SERV = 0x00,         ///<discover control service
    MYB_DIS_CONTRL_CHAR = 0x10,         ///<discover all characteristics of this service
    MYB_DES_CONTRL_SERV  = 0x20,        ///<get cccd descriptors for every characteristic of the control service  
    MYB_DIS_IMU_SERV = 0x01,            ///<discover IMU service
    MYB_DIS_IMU_CHAR = 0x11,            ///<discover all characteristics of this service
    MYB_DES_IMU_SERV = 0x21,            ///<get cccd descriptors for every characteristic of the IMU service
    MYB_DIS_CLASS_SERV = 0x02,          ///<discover classifier service
    MYB_DIS_CLASS_CHAR = 0x12,		///<discover all characteristics of this service
    MYB_DES_CLASS_SERV = 0x22,		///<get cccd descriptors for every characteristic of this service
    MYB_DIS_EMG_SERV = 0x03,		///<discover EMG service
    MYB_DIS_EMG_CHAR = 0x13,		///<discover all characteristics of this service
    MYB_DES_EMG_SERV = 0x23,		///<get cccd descriptors for every characteristic of this service
    MYB_DIS_BATT_SERV = 0x04,		///<discover battery service
    MYB_DIS_BATT_CHAR = 0x14,		///<discover all characteristics of this service
    MYB_DES_BATT_SERV = 0x24,		///<get cccd descriptors for every characteristic of this service
	
    MYB_READY = 0xA2,                   ///<ready for requests (read, write, etc)
    MYB_READING_CHAR = 0xA3,            ///<reading a characteristic
    MYB_WRITING_CHAR = 0xA4,            ///<writing a characteristic
 } MyoGATTState_t;
 
/**
 * Represents the position of a characteristic/service in myo_handles_t, for direct memory lookup
 */
 typedef enum {
    //control Service
    MYO_SERV_CONTROL = 0,			///< control serivice start index
    MYO_CHAR_INFO = 2,				///< hardware info characteristic index
    MYO_CHAR_FIRMWARE = 3,			///< firmware info characteristic index
    MYO_CHAR_COMMAND = 4,			///< command characteristic index
    
    //IMU Service
    MYO_SERV_IMU_DATA = 5,			///< IMU serivice start index
    MYO_CHAR_IMU_DATA = 7,			///< IMU data characteristic index
    MYO_CHAR_IMU_EVENTS = 8,			///< IMU events characteristic index
    
    //Classifier Service
    MYO_SERV_CLASSIFIER = 9,			///< classifier serivice start index
    MYO_CHAR_CLASSIFIER_EVENTS = 11,	        ///< classifier events characteristic index
    
    //EMG Service
    MYO_SERV_EMG = 12,				///< EMG serivice start index, see http://developerblog.myo.com/myocraft-emg-in-the-bluetooth-protocol/.
    MYO_CHAR_EMG_0 = 14,			///< EMG 0 characteristic index
    MYO_CHAR_EMG_1 = 15,			///< EMG 1 characteristic index
    MYO_CHAR_EMG_2 = 16,			///< EMG 2 characteristic index
    MYO_CHAR_EMG_3 = 17,			///< EMG 3 characteristic index
    
    //Battery Service
    MYO_SERV_BATTERY = 18,			///< battery serivice start index
    MYO_CHAR_BATTERY_LEVEL = 20,		///< battery level characteristic index
 } MyoCharacteristicIndex_t;  //should < 256, get cast to uint8

/*********************************************************************
 * TYPEDEFS
 */

/**
 * Stores discovered GATT handles. Can be reused when reconnecting with the same device.
 */ 
typedef struct {
	
  uint16 controlServiceStart;                   ///< control serivice start handle
  uint16 controlServiceEnd;		        ///< control serivice end handle
  uint16 myoInfoChar;				///< hardware info characteristic handle
  uint16 firmwareVersionChar;		        ///< firmware info characteristic handle
  uint16 commandChar;				///< command characteristic handle

  //IMU Data Service
  uint16 imuServiceStart;			///< IMU serivice start handle
  uint16 imuServiceEnd;				///< IMU serivice end handle
  uint16 imuData;				///< IMU data characteristic handle
  uint16 imuEvents;				///< IMU events characteristic handle
  
  //Classifier Service
  uint16 classifierServiceStart;	        ///< classifier serivice start handle
  uint16 classifierServiceEnd;		        ///< classifier serivice end handle
  uint16 classifierEvents;			///< classifier events characteristic handle
  
  //EMG Data Service
  uint16 emgServiceStart;			///< EMG serivice start handle, see http://developerblog.myo.com/myocraft-emg-in-the-bluetooth-protocol/.
  uint16 emgServiceEnd;				///< EMG serivice end handle
  uint16 emgData0;				///< EMG 0 characteristic handle
  uint16 emgData1;				///< EMG 1 characteristic handle
  uint16 emgData2;				///< EMG 2 characteristic handle
  uint16 emgData3;				///< EMG 3 characteristic handle
  
  //Battery Service
  uint16 batteryServiceStart;		        ///< battery serivice start handle
  uint16 batteryServiceEnd;			///< battery serivice end handle
  uint16 batteryLevel;				///< battery level characteristic handle
} MyoHandles_t;

/// @}


/*********************************************************************
 * FUNCTIONS
 */

/**
 * Processes GATT Scanning from the OSAL event side, calls GATT discover functions.
 * GATT Disvocering is a state machine and is processed in MyoBridge_ProcessGATTScanEvt() and MyoBridgeProcessGATTMsg( gattMsgEvent_t *pMsg ).
 */
uint16 MyoBridge_ProcessGATTScanEvt(uint8 MyoBridge_TaskID, uint16 myo_conn_handle);

/**
* Processes GATT Scanning from the GATT Message side, extracts scan data.
* GATT Disvocering is a state machine and is processed in MyoBridge_ProcessGATTScanEvt() and MyoBridgeProcessGATTMsg( gattMsgEvent_t *pMsg ).
*/
void MyoBridgeProcessGATTMsg(uint8 MyoBridge_TaskID, uint16 myo_conn_handle, gattMsgEvent_t *pMsg);

/**
* Set the MyoGATTState
*/
void setGATTState(MyoGATTState_t state);

/**
* Get the MyoGATTState
*/
MyoGATTState_t getGATTState();
 
/**
 * request the value of a characteristic. the value will be sent back via an OSAL message
 */
uint8 requestCharValue(uint8 MyoBridge_TaskID, uint16 myo_conn_handle, MyoCharacteristicIndex_t index);

/**
 * send a command to the command characteristic.
 */
uint8 sendCommand(uint8 MyoBridge_TaskID, uint16 myo_conn_handle, uint8* pCommand, uint8 len);

/**
 * write to a characteristic.
 */
uint8 writeChar(uint8 MyoBridge_TaskID, uint16 myo_conn_handle, uint8* pCommand, uint8 len, uint16 handle);

/**
 * write to the characteristic of the given index.
 */
uint8 writeCharByIndex(uint8 MyoBridge_TaskID, uint16 myo_conn_handle, uint8* pCommand, uint8 len, MyoCharacteristicIndex_t index);

/**
 * set the status of asynchronous messages (nofifications, indications) of a characteristic
 */
uint8 setAsyncStatus(uint8 MyoBridge_TaskID, uint16 myo_conn_handle, uint16 status, MyoCharacteristicIndex_t index);

/*********************************************************************
*********************************************************************/
