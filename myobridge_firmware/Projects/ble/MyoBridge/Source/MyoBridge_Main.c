/**
 * @file   MyoBridge_Main.c
 * @author Valentin Roland (webmaster at vroland.de)
 * @date   September-October 2015
 * @brief  Implementation file of the main function handling device startup.
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

/**
 * @mainpage
 * @author Valentin Roland (webmaster at vroland.de)
 * @date   September-October 2015
 * 
 * MyoBridge is a firmware for the CC2541/40 from Texas Instruments,
 * particularly on the HM-11 BLE Module. With this software, such a device can operate as an intermediate mediator
 * between a Myo Gesture Control Armband (https://www.myo.com/) and another device capable of sending and receiving
 * data via a two-wire (RX/TX) serial connection, like Arduino. The usage with an Arduino device will be the primary
 * application and the target platform of a high-level library to use together with the firmware. 
 * 
 * This project is developed for research and educational purposes at Victoria University of Wellington, 
 * School of Engineering and Computer Science.
 */


/* Hal Drivers */
#include "hal_types.h"
#include "hal_timer.h"
#include "hal_drivers.h"
#include "hci.h"

/* OSAL */
#include "OSAL.h"
#include "OSAL_Tasks.h"
#include "OSAL_PwrMgr.h"
#include "osal_snv.h"
#include "OnBoard.h"

#include "serialInterface.h"

/**
 * @brief       Start of application.
 */
int main(void)
{
  /* Initialize hardware */
   HAL_BOARD_INIT();

  // Initialize board I/O
  InitBoard( OB_COLD );

  /* Initialze the HAL driver */
  HalDriverInit();

  /* Initialize NV system */
  osal_snv_init();

  /* Initialize LL */

  /* Initialize the operating system */
  osal_init_system();

  /* Enable interrupts */
  HAL_ENABLE_INTERRUPTS();

  // Final board initialization
  InitBoard( OB_READY );

  osal_pwrmgr_device( PWRMGR_ALWAYS_ON );
  
  HCI_EXT_ClkDivOnHaltCmd(HCI_EXT_DISABLE_CLK_DIVIDE_ON_HALT);

    
  /* Start OSAL */
  osal_start_system(); // No Return from here

  return 0;
}

