/**
 * @file   MyoBridge.h
 * @author Valentin Roland (webmaster at vroland.de)
 * @date   September-October 2015
 * @brief  Header file for the main MyoBridge Task, containing Status-, Event-, and Type constants as well as Data Structures.
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

#ifndef MYBRIDGE_H
#define MYBRIDGE_H

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

/// @defgroup myb_osal_events OSAL Message Events
/// Internal OSAL Message Event Types
/// @{
#define MYB_DATA_RSP_EVENT                            0xE1      ///< This OSAL Message contains a GATT value/data response.
#define MYB_WRITE_RSP_EVENT                           0xE2      ///< This OSAL Message contains a GATT write response.
#define MYB_SERIAL_EVENT                              0xE3      ///< This OSAL Message contains data from a serial event.
///@}

/// @defgroup myb_task_events MyoBridge Application Events
/// Internal MyoBridge Application/Task events.
/// @{
#define MYB_START_DEVICE_EVT                          0x0001    ///< Internal MyoBridge startup event.
#define MYB_START_DISCOVERY_EVT                       0x0002    ///< Internal MyoBridge start device discovery (scanning) event.
#define MYB_TIMER_EVT                                 0x0004    ///< Internal MyoBridge timer event.
#define MYB_INIT_LINK_EVT                             0x0008    ///< Internal MyoBridge connection initiation event.
#define MYB_GATT_WORKING_EVT                          0x0200    ///< Internal MyoBridge event to call the GATT functions in myo_gatt and let them do their work.
#define MYB_GATT_READY_EVT                            0x0400    ///< Internal MyoBridge event to tell the application it finished executing a GATT action.
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


/// @defgroup myb_data_structs MyoBridge Data Structures
/// MyoBridge Command, Response and OSAL Structures
/// @{

/// @defgroup myb_cmd_structs MyoBridge Command Structures
/// Structures of the commands recognized by MyoBridge.
/// @{

///Myb Command Header
typedef struct {
  uint8 cmd;                    ///< Command Type
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
  uint8 index;                  ///< characteristic index
} MYBReadCmd_t;

///Write Characteristic Command
typedef struct {
  MYBCmdHdr_t hdr;              ///< Command Header
  uint8 index;                  ///< characteristic index
  uint8 len;                    ///< length of value data
  uint8* pData;                 ///< data to write
} MYBWriteCmd_t;

///Async Status Command
typedef struct {
  MYBCmdHdr_t hdr;              ///< Command Header
  uint8 index;                  ///< characteristic index
  uint16 status;                ///< The CCCD status. See https://developer.bluetooth.org/gatt/descriptors/Pages/DescriptorViewer.aspx?u=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
} MYBAsyncStatusCmd_t;
///@}

/// @defgroup myb_rsp_structs MyoBridge Response Structures
/// Structures of the responses sent by MyoBridge.
/// @{

///Myb Response Header
typedef struct {
  uint8 type;                   ///< Response Type
  uint8 len;                    ///< Number of Bytes after the header
} MYBRspHdr_t;

///Data Response
typedef struct {
  MYBRspHdr_t hdr;              ///< Response Header
  uint8 characteristic;         ///< The characteristic index where the data comes from
  uint8* pData;                 ///< Response Data
} MYBDataRsp_t;

///Ping Response
typedef struct {
  MYBRspHdr_t hdr;              ///< Response Header         
} MYBPingRsp_t;

///Staus Response
typedef struct {
  MYBRspHdr_t hdr;              ///< Response Header  
  uint8 status;                 ///< System Status Code
} MYBStatusRsp_t;
///@}

/// @defgroup myb_osal_structs MyoBridge OSAL Structures
/// Structures for packing data when sent via OSAL Messages. 
/// Used to move data between different parts of the Application.
/// @{

///OSAL Message Structure for returning values from reads or notifications
typedef struct {
  osal_event_hdr_t hdr;         ///< OSAL Event Message Header
  MYBDataRsp_t rsp;             ///< Data Response
} MYBOSALDataRsp_t;

///OSAL Message Structure for returning write response
typedef struct {
  osal_event_hdr_t hdr;         ///< OSAL Event Message Header
  MYBPingRsp_t rsp;             ///< Ping Response
} MYBOSALPingRsp_t;

///OSAL Message Structure for sending incoming package data
typedef struct {
  osal_event_hdr_t hdr;         ///< OSAL Event Message Header
  uint8 len;                    ///< Length of package data
  uint8* pData;                 ///< The package data
} MYBOSALSerialPacket_t;
///@}
///@}

/*********************************************************************
 * FUNCTIONS
 */

/**
 * Task Initialization for MyoBridge. Is automatically called during initialization.
 */
void MyoBridge_Init( uint8 task_id );

/**
 * Task Event Processor for MyoBridge. Is called whenever an OSAL event for this Application occurs.
 */
uint16 MyoBridge_ProcessEvent( uint8 task_id, uint16 events );

/**
 * Task Event Processor for MyoBridge
 */
void MyoBridge_HandleSerialEvent(uint8* packet, uint16 len);

/**
 * get the system status
 */
void setSystemStatus(uint8 state);

/*********************************************************************
*********************************************************************/

#endif /* MYOBRIDGE_H */