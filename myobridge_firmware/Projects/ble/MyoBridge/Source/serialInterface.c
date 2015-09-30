/**
 * @file   serialInterface.c
 * @author Valentin Roland (webmaster at vroland.de)
 * @date   September-October 2015
 * @brief  Implementation file of functions neccessary to communicate with the host device.
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
#include "serialInterface.h"
#include "MyoBridge.h"
#include "bcomdef.h"

#include <string.h>


/**
 * @brief serial receive buffer.
 */
uint8 serialBuffer[RX_BUFF_SIZE];

/**
 * @brief serial receive buffer offset (how many bytes are in buffer)?.
 */
uint16 buffer_offset = 0;

/// unused local function for OSAL Message processing. Currently empty.
static void SerialInterface_ProcessOSALMsg( osal_event_hdr_t *pMsg );

/// Task ID for internal task/event processing
uint8 serialInterface_TaskID;   
/// Task ID of the MyoBridge Application
extern uint8 MyoBridge_TaskID;



/**
 * @brief Initialization of the serial interface
 */
void SerialInterface_Init( uint8 task_id )
{
  serialInterface_TaskID = task_id;
    
  NPI_InitTransport(cSerialPacketParser);
}

/**
 * @brief Task Event Processor for the SerialInterface Application
 */
uint16 SerialInterface_ProcessEvent( uint8 task_id, uint16 events )
{
  
  VOID task_id; // OSAL required parameter that isn't used in this function
  
  if ( events & SYS_EVENT_MSG )
  {
    uint8 *pMsg;
    
    if ( (pMsg = osal_msg_receive( serialInterface_TaskID )) != NULL )
    {
      SerialInterface_ProcessOSALMsg( (osal_event_hdr_t *)pMsg );
      
      // Release the OSAL message
      VOID osal_msg_deallocate( pMsg );
    }
    
    // return unprocessed events
    return (events ^ SYS_EVENT_MSG);
  }
  
  // Discard unknown events
  return 0;
}

static void SerialInterface_ProcessOSALMsg( osal_event_hdr_t *pMsg )
{
  switch ( pMsg->event )
  {
  default:
    // do nothing
    break;
  }
}

/**
 * @brief erase a number of bytes from the RX buffer
 */
void eraseBuffer(uint16 bytes) {
  osal_memcpy(&serialBuffer[0], &serialBuffer[bytes], RX_BUFF_SIZE - bytes);
  buffer_offset -= bytes;
}

/**
 * @brief handle incoming serial data
 */
void cSerialPacketParser( uint8 port, uint8 events )
{  
   uint8 numBytes; 
   // get the number of available bytes to process
   numBytes = NPI_RxBufLen();
   // check if there's any serial port data to process
   if (((numBytes > 0) || (buffer_offset>0))) {
    
     uint16 len = HalUARTRead(port, &serialBuffer[buffer_offset], RX_BUFF_SIZE - buffer_offset);
     buffer_offset += len;
    
    //if there is something in the buffer
    if ( buffer_offset> 0) {
      int16 start_index = -1;
      int16 end_index = -1;
      
      uint16 i;
      //search for packet start
      for (i=0;i<buffer_offset;i++) {
        char c = serialBuffer[i];
        //potential packet start
        if ((c=='P') && (i<buffer_offset-1)) {
          uint8 blen = serialBuffer[i+1];
          uint16 temp_end = i+blen+2;
          //valid packet found?
          if ((temp_end < buffer_offset) && (serialBuffer[temp_end]=='E')) {
            start_index = i;
            end_index = temp_end;
            break;
          }
        }
      }
      
      //if packet is valid, trigger serial event and erase buffer
      if ((start_index>-1) && (end_index>-1) && (start_index<end_index)) {
        //cut P, E and package length from data
        
        //send notification data via OSAL system event
        MYBOSALSerialPacket_t* msg = (MYBOSALSerialPacket_t*)osal_msg_allocate(sizeof(MYBOSALSerialPacket_t));
        if (msg) {
          msg->hdr.event = MYB_SERIAL_EVENT;
          msg->hdr.status = SUCCESS;
          msg->pData = osal_mem_alloc(end_index-start_index-2);
          msg->len = end_index-start_index-1;
          osal_memcpy(msg->pData, &serialBuffer[start_index + 2], end_index-start_index-2);
          
          //send message with packet data
          osal_msg_send(MyoBridge_TaskID, (uint8*) msg);
          
          //erase everything before and the packet itself from the RX buffer.
          eraseBuffer(end_index + 1);
        } 
      }
    }
    if (buffer_offset > RX_BUFF_SIZE) {
      prints("RX Buffer Overflow!\n");
    }
  }
}

/**
 * @brief send a ping response via the serial interface.
 */
void sendPingRsp(MYBPingRsp_t* pRsp) {
  //Package start
  printd("P",1); 
  //Package length
  uint8 size = sizeof(MYBPingRsp_t);
  printd(&size,1);  
  //Package data
  printd((uint8*)pRsp, size);   
  //package end
  printd("E",1);
}

/**
 * @brief send a data/value response via the serial interface.
 */
void sendDataRsp(MYBDataRsp_t* pRsp) {
  //Package start
  printd("P",1); 
  //Package length
  uint8 size = sizeof(MYBDataRsp_t) - sizeof(uint8*) + pRsp->hdr.len - 1;
  printd(&size,1);  
  //Package header
  printd((uint8*)&pRsp->hdr, sizeof(MYBRspHdr_t));
  //characteristic index
  printd((uint8*)&pRsp->characteristic, 1);
  //characteristic data
  printd((uint8*)pRsp->pData, pRsp->hdr.len-1);
  
  //package end
  printd("E",1);
}

/**
 * @brief send a status response via the serial interface.
 */
void sendStatusRsp(MYBStatusRsp_t* pRsp) {
  //Package start
  printd("P",1); 
  //Package length
  uint8 size = sizeof(MYBStatusRsp_t);
  printd(&size,1);  
  //Package data
  printd((uint8*)pRsp, size);   
  //package end
  printd("E",1);
}

/**
 * @brief print a data buffer of given length to the serial interface
 */
uint8 printd(uint8* data, uint8 len)
{
  if (len==0) return SUCCESS;
  if (data==NULL) return bleMemAllocError;
  
  uint8* buf = osal_mem_alloc((len)*sizeof(uint8));
  if (&buf)  //if allocated
  {
    osal_memcpy(buf, data, len);
    uint8 bytes_sent = HalUARTWrite(NPI_UART_PORT, (uint8*)buf, len);

    osal_mem_free(buf);
    if (bytes_sent == len)
    {
      return SUCCESS;
    }
    else
    {
      return 1;  //data not sent over UART
    }    
  }
  else
  {
    return bleMemAllocError;
  }
}

/**
 * @brief print a c string to the serial interface. Use only for debug.
 */
uint8 prints(char* data)
{
  uint8 len = strlen(data) + 1;
  uint8* buf = osal_mem_alloc(len*sizeof(uint8));
  if (buf)  //if allocated
  {
    osal_memcpy(&buf[0], data, len);
    uint8 bytes_sent = HalUARTWrite(NPI_UART_PORT, (uint8*)buf, len);
    osal_mem_free(buf);
    if (bytes_sent == len)
    {
      return SUCCESS;
    }
    else
    {
      return 1;  //data not sent over UART
    }    
  }
  else
  {
    return bleMemAllocError;
  }
}