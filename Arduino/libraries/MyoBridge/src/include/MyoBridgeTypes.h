/**
 * @file   MyoBridgeTypes.h
 * @author Valentin Roland (webmaster at vroland.de)
 * @date   September-October 2015
 * @brief  Header file of MyoBridge Typedefs, Constants, Enums, Etc. for use with Arduino
 *
 * This file is part of the MyoBridge project. MyoBridge is a firmware for the CC2541/40 from Texas Instruments,
 * particularly on the HM-11 BLE Module that comes with a high-level Arduino library. This file belongs
 * to the Arduino library. It provides easy access to the basic functionality of MyoBridge and fast access
 * to Myo Armband sensor data and events. Because the main focus is ease of use, this library may not cover
 * all possible use cases of the MyoBridge Firmware and the Myo Armband itself.
 * 
 * This project is developed for research and educational purposes at Victoria University of Wellington, 
 * School of Engineering and Computer Science.
 */

#ifndef MYBRIDGETYPES_H
#define MYBRIDGETYPES_H

/*********************************************************************
 * INCLUDES
 */

/*********************************************************************
 * CONSTANTS
 */

/// @defgroup myb_consts MyoBridge Control Constants
/// These constants control the flow of the application or define the types of return
/// values, states, etc. Some are also used in serial communication.
/// @{

/// Time in milliseconds a request is resent if no answer is received
#define MYB_RETRY_TIMEOUT							  300
/// Time in milliseconds the MyoBridge is allowed to scan until the library resets it, if a reset pin is specified.
#define MYB_MODULE_CONNECT_TIMEOUT					  2000

/// @defgroup myb_system_status System Status Constants
/// Describes the current status of the application, can be notified via the serial interface.
/// @{
#define MYB_STATUS_UNKNOWN                            0x00      ///< The system status is unknown. This occurs before initialization and can be used to initialize a state variable.
#define MYB_STATUS_INIT                               0x01      ///< The system is currently initializing.
#define MYB_STATUS_SCANNING                           0x02      ///< The system is scanning for nearby Myo devices.
#define MYB_STATUS_CONNECTING                         0x03      ///< The system is connecting to a Myo device.
#define MYB_STATUS_DISCOVERING                        0x04      ///< The system is discovering the Myo's GATT profile.
#define MYB_STATUS_READY                              0x05      ///< The system is ready to execute commands.
#define MYB_STATUS_BUSY                               0x06      ///< The system is busy and cannot execute commands.
///@}

/// @defgroup myb_serial_commands Serial Command Types
/// Types of serial commands available for controlling MyoBridge.
/// @{
#define MYB_CMD_PING                                  0x01      ///< ping MyoBridge. MYB_RSP_PING is returned.
#define MYB_CMD_READ_CHAR                             0x02      ///< read a characteristic. MYB_RSP_VALUE is returned.
#define MYB_CMD_WRITE_CHAR                            0x03      ///< write a characteristic. MYB_RSP_PING is returned.
#define MYB_CMD_SET_ASYNC_STATUS                      0x04      ///< set notification/indication status. MYB_RSP_PING is returned.
#define MYB_CMD_GET_STATUS                            0x05      ///< request the current system status. Staus constant is returned.
///@}

/// @defgroup myb_serial_types Serial Packet Types
/// Types of answers or nitifications MyoBridge sends via the serial connection.
/// @{
#define MYB_RSP_PING                                  0x30      ///< Ping response type ID. Response contains only the packet type.
#define MYB_RSP_VALUE                                 0x31      ///< Data response type ID. Response contains a value of a specific characteristic.
#define MYB_RSP_STATUS                                0x32      ///< Status response type ID (Is sent either through MYB_CMD_GET_STATUS or asyncronously). Response contains the current system status of MyoBridge.
///@}
/// @}

/**
 * @defgroup gatt_indices MyoBridge Characteristic Indices
 * These characteristic indices are used when communication with a MyoBridge device.
 * @{
 */
 
/**
 * Enumeration of Characteristic indices for use in communication with a MyoBridge device.
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
 } MyoCharacteristicIndex_t;  //should < 256, get cast to uint8_t
 ///@}

/// @defgroup myb_data_structs MyoBridge Data Structures
/// MyoBridge Command, Response and OSAL Structures
/// @{

/// @defgroup myb_cmd_structs MyoBridge Command Structures
/// Structures of the commands recognized by MyoBridge.
/// @{

///Myb Command Header
typedef struct {
  uint8_t cmd;                    ///< Command Type
} MYBCmdHdr_t;

///Ping Command
typedef struct {
  MYBCmdHdr_t hdr;              ///< Command Header
} MYBPingCmd_t;
   
///Get Status Command
typedef struct {
  MYBCmdHdr_t hdr;              ///< Command Header
} MYBGetStatusCmd_t;

///Read Characteristic Command
typedef struct {
  MYBCmdHdr_t hdr;              ///< Command Header
  uint8_t index;                  ///< characteristic index
} MYBReadCmd_t;

///Write Characteristic Command
typedef struct {
  MYBCmdHdr_t hdr;              ///< Command Header
  uint8_t index;                  ///< characteristic index
  uint8_t len;                    ///< length of value data
  uint8_t* pData;                 ///< data to write
} MYBWriteCmd_t;

///Async Status Command
typedef struct {
  MYBCmdHdr_t hdr;              ///< Command Header
  uint8_t index;                  ///< characteristic index
  uint16_t status;                ///< The CCCD status. See https://developer.bluetooth.org/gatt/descriptors/Pages/DescriptorViewer.aspx?u=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
} MYBAsyncStatusCmd_t;
///@}

/// @defgroup myb_rsp_structs MyoBridge Response Structures
/// Structures of the responses sent by MyoBridge.
/// @{

///Myb Response Header
typedef struct {
  uint8_t type;                   ///< Response Type
  uint8_t len;                    ///< Number of Bytes after the header
} MYBRspHdr_t;

///Data Response
typedef struct {
  MYBRspHdr_t hdr;              ///< Response Header
  uint8_t characteristic;         ///< The characteristic index where the data comes from
  uint8_t* pData;                 ///< Response Data
} MYBDataRsp_t;

///Ping Response
typedef struct {
  MYBRspHdr_t hdr;              ///< Response Header         
} MYBPingRsp_t;

///Staus Response
typedef struct {
  MYBRspHdr_t hdr;              ///< Response Header  
  uint8_t status;                 ///< System Status Code
} MYBStatusRsp_t;
///@}
///@}

#endif