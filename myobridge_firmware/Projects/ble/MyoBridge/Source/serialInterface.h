/**
 * @file   serialInterface.h
 * @author Valentin Roland (webmaster at vroland.de)
 * @date   September-October 2015
 * @brief  Header file for the serial interface used for communication with the host device.
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

#include "hal_uart.h"
#include "OSAL.h"
#include "npi.h"
#include "MyoBridge.h"

/// The RX buffer size
#define RX_BUFF_SIZE    256

//===================================================

/*******************************************************************************
 * MACROS
 */

/*********************************************************************
 * FUNCTIONS
 */

/**
 * @brief handle incoming serial data
 */
void cSerialPacketParser( uint8 port, uint8 events );

/**
 * @brief erase a number of bytes from the RX buffer
 */
void eraseBuffer(uint16 bytes);

/**
 * @brief Initialization of the serial interface
 */
void SerialInterface_Init( uint8 task_id );

/**
 * @brief Task Event Processor for the SerialInterface Application
 */
uint16 SerialInterface_ProcessEvent( uint8 task_id, uint16 events );

/**
 * @defgroup serial_functions MyoBridge Serial Interface Functions
 * These functions can be used to send data via the serial (UART) interface to the host.
 * @{
 */

/**
 * @brief print a data buffer of given length to the serial interface
 */
uint8 printd(uint8* data, uint8 len);  

/**
 * @brief print a c string to the serial interface. Use only for debug.
 */
uint8 prints(char* data);  

/**
 * @brief send a ping response via the serial interface.
 */
void sendPingRsp(MYBPingRsp_t* pRsp);

/**
 * @brief send a data/value response via the serial interface.
 */
void sendDataRsp(MYBDataRsp_t* pRsp);

/**
 * @brief send a status response via the serial interface.
 */
void sendStatusRsp(MYBStatusRsp_t* pRsp);

///@}