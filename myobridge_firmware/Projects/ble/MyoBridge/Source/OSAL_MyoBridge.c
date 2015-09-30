/**
 * @file   OSAL_MyoBridge.c
 * @author Valentin Roland (webmaster at vroland.de)
 * @date   September-October 2015
 * @brief  Implementation file handling the startup of the OSAL system.
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

#include "hal_types.h"
#include "OSAL.h"
#include "OSAL_Tasks.h"

/* HAL */
#include "hal_drivers.h"

/* LL */
#include "ll.h"

/* HCI */
#include "hci_tl.h"

#if defined ( OSAL_CBTIMER_NUM_TASKS )
  #include "osal_cbTimer.h"
#endif

/* L2CAP */
#include "l2cap.h"

/* gap */
#include "gap.h"
#include "gapgattserver.h"
#include "gapbondmgr.h"

/* GATT */
#include "gatt.h"
#include "gattservapp.h"

/* Profiles */
#include "central.h"

/* Application */
#include "MyoBridge.h"

#include "serialInterface.h"

/*********************************************************************
 * GLOBAL VARIABLES
 */

/// List of task event handlers.
/// The order in this table must be identical to the task initialization calls below in osalInitTask.
const pTaskEventHandlerFn tasksArr[] =
{
  LL_ProcessEvent,                                              // task 1
  Hal_ProcessEvent,                                             // task 2
  HCI_ProcessEvent,                                             // task 3
#if defined ( OSAL_CBTIMER_NUM_TASKS )                          
  OSAL_CBTIMER_PROCESS_EVENT( osal_CbTimerProcessEvent ),       // task 4
#endif
  L2CAP_ProcessEvent,                                           // task 5
  GAP_ProcessEvent,                                             // task 6
  GATT_ProcessEvent,                                            // task 7
  SM_ProcessEvent,                                              // task 8
  GAPCentralRole_ProcessEvent,                                  // task 9
  GAPBondMgr_ProcessEvent,                                      // task 10
  GATTServApp_ProcessEvent,                                     // task 11
  SerialInterface_ProcessEvent,                                     // task 12
  MyoBridge_ProcessEvent                                           // task 13
};

const uint8 tasksCnt = sizeof( tasksArr ) / sizeof( tasksArr[0] );
uint16 *tasksEvents;

/*********************************************************************
 * FUNCTIONS
 *********************************************************************/

/**
 * @brief   This function invokes the initialization function for each task.
 */
void osalInitTasks( void )
{
  uint8 taskID = 0;

  tasksEvents = (uint16 *)osal_mem_alloc( sizeof( uint16 ) * tasksCnt);
  osal_memset( tasksEvents, 0, (sizeof( uint16 ) * tasksCnt));

  /* LL Task */
  LL_Init( taskID++ );

  /* Hal Task */
  Hal_Init( taskID++ );

  /* HCI Task */
  HCI_Init( taskID++ );

#if defined ( OSAL_CBTIMER_NUM_TASKS )
  /* Callback Timer Tasks */
  osal_CbTimerInit( taskID );
  taskID += OSAL_CBTIMER_NUM_TASKS;
#endif

  /* L2CAP Task */
  L2CAP_Init( taskID++ );

  /* GAP Task */
  GAP_Init( taskID++ );

  /* GATT Task */
  GATT_Init( taskID++ );

  /* SM Task */
  SM_Init( taskID++ );

  /* Profiles */
  GAPCentralRole_Init( taskID++ );
  GAPBondMgr_Init( taskID++ );

  GATTServApp_Init( taskID++ );

  SerialInterface_Init(taskID++ );
  
  /* Application */
  MyoBridge_Init( taskID );
}

/*********************************************************************
*********************************************************************/
