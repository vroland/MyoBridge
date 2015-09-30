/*******************************************************************************
  Filename:       phy.c
  Revised:        $Date: 2014-10-27 10:01:57 -0700 (Mon, 27 Oct 2014) $
  Revision:       $Revision: 40811 $

  Description:    This file implements the Physical Layer (PHY) module.
                  This module provides an abstraction layer to the
                  radio hardware.

  Copyright 2009-2013 Texas Instruments Incorporated. All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights
  granted under the terms of a software license agreement between the user
  who downloaded the software, his/her employer (which must be your employer)
  and Texas Instruments Incorporated (the "License").  You may not use this
  Software unless you agree to abide by the terms of the License. The License
  limits your use, and you acknowledge, that the Software may not be modified,
  copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio
  frequency transceiver, which is integrated into your product.  Other than for
  the foregoing purpose, you may not use, reproduce, copy, prepare derivative
  works of, modify, distribute, perform, display or sell this Software and/or
  its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
  PROVIDED “AS IS” WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
  INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
  NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
  TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
  LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
  INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
  OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
  OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
  (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com.
*******************************************************************************/

/*******************************************************************************
 * INCLUDES
 */
#include "phy.h"
#include "phy_image.h"
#include "hal_assert.h"

/*******************************************************************************
 * MACROS
 */

#define RF_CHAN_TO_BLE_FREQ(x)  ( ((x) << 1) + PHY_BLE_BASE_FREQ )
#define BLE_FREQ_TO_RF_CHAN(x)  ( ((x) - PHY_BLE_BASE_FREQ) >> 1 )
//
#define PHY_ASSERT(x)           HAL_ASSERT(x)

/*******************************************************************************
 * CONSTANTS
 */

// Whitelist
#define PHY_MAX_NUM_OF_WL_ENTRIES        8
#define PHY_DEV_ADDR_SIZE                6
#define PHY_NO_WL_MATCH_FOUND            0xFF
#define PHY_SAVED_WL_BL_SIZE             ((PHY_MAX_NUM_OF_WL_ENTRIES * PHY_DEV_ADDR_SIZE) + 3)

// FIFO backup
#define PHY_BACKUP_CONNECTIONS           3

// FIFO Sequence Init
#define PHY_INIT_SEQSTAT_VALUE           0x0B;

// FIFO type
#define PHY_TXFIFO                       0x0
#define PHY_RXFIFO                       0x1

// Bit mappings for ADDRTYPE register
#define PHY_ADDRTYPE_OWN_BIT             0
#define PHY_ADDRTYPE_PEER_BIT            1

// Bit mappings for WLPOLICY register
#define PHY_ADV_WL_POLICY_BIT            0 // bits 0 and 1
#define PHY_SCAN_WL_POLICY_BIT           2
#define PHY_INIT_WL_POLICY_BIT           3

// Bit mappings for BLE_SCANCONF register
#define PHY_SCANCONF_ACTPASS_BIT         0
#define PHY_SCANCONF_END_BIT             1

// Bit mappings for SEQSTAT register
#define PHY_SEQSTAT_LAST_RX_SN_BIT       0
#define PHY_SEQSTAT_LAST_TX_SN_BIT       1
#define PHY_SEQSTAT_NEXT_TX_SN_BIT       2
#define PHY_SEQSTAT_FIRSTPKT_BIT         3
#define PHY_SEQSTAT_EMPTY_BIT            4
#define PHY_SEQSTAT_CTRL_BIT             5
#define PHY_SEQSTAT_CTRL_ACK_PENDING_BIT 6

// Bit mapping and mask for CONF register
#define PHY_CONF_ENDC_BIT                0
#define PHY_CONF_MD_MASK                 (BV(2) | BV(1))


// Raw Transmit/Receive Parameters
#define PHY_BLE_BASE_FREQ                2402  // MHz
#define PHY_RF_BASE_FREQ                 2379  // MHz
#define PHY_BASE_FREQ_OFFSET             (PHY_BLE_BASE_FREQ - PHY_RF_BASE_FREQ)
#define PHY_MAX_RF_CHAN                  39
#define PHY_WHITENER_INIT                0x65

// TX Tone
#define PHY_TX_TONE_ZERO_HZ              0x0A  // also sets RX mixer freq to +1Mhz
#define PHY_TX_TONE_MINUS_ONE_HZ         0x25  // also sets RX mixer freq to -1MHz
#define PHY_TX_TONE_PLUS_ONE_HZ          0x0F  // also sets RX mixer freq to +1MHz

/*******************************************************************************
 * TYPEDEFS
 */

// fifo params for each fifo
typedef struct
{
  uint8 wp;                              // write pointer
  uint8 swp;                             // start write pointer
  uint8 rp;                              // read pointer
  uint8 srp;                             // start read pointer
  uint8 bankId;                          // memory bank identifier
} fifoInfo_t;

// information about connection to be backed up
typedef struct
{
  fifoInfo_t txInfo;
  fifoInfo_t rxInfo;
} connInfo_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */

// array to store FIFO information for all connections
static connInfo_t connInfo[PHY_BACKUP_CONNECTIONS];


// This is a bit map of bank that are available for swapping the tx/rx FIFOs.
// Banks 1-5 are used. All banks are available at start up. They become
// unavailable as connection FIFOs are stored and once again become available
// as connection FIFOs are restored.
static uint8 phyFreeBanks = 0x3E;

/*******************************************************************************
 * GLOBAL VARIABLES
 */

// number of used enties in the white list
uint8 numWLEntries;

/*******************************************************************************
 * Prototypes
 */

static void  phyCopyFIFO( uint8 *destBasePtr, uint8 *srcBasePtr, uint8 wp, uint8 srp );
static uint8 phyGetFreeBank( void );
static void  phySaveFifo( uint8 fifoType, uint8 connId, uint8 bankId );
static void  phyRestoreFifo( uint8 fifoType, uint8 connId );
static uint8 phyFindWlEntry( uint8 *addr, phyAddrType_t addrType );

/*******************************************************************************
 * API
 */

/*******************************************************************************
 * Loads the nanoRisc image and initialises PHY module.
 *
 * Note: This routine does not verify the nR image after loading.
 *
 * Public function defined in phy.h.
 */
void PHY_Init( void )
{
#if !defined( CC2541 ) && !defined( CC2541S ) // CC2540
  // reset the nanoRisc and Load the nanoRisc image
  PHY_LoadNR();

  // load and verify the nR
  if ( PHY_VerifyNR() != PHY_STATUS_SUCCESS )
  {
    // fatal error; can not continue without nR
    PHY_ASSERT( FALSE );
  }
#endif // !CC2541 && !CC2541S

  // clear the nR BLE register memory and the TX and RX FIFOs.
  PHY_ClearAllRegsAndFifos();

  // release nR from Reset
  PHY_Reset();

  return;
}


/*******************************************************************************
 * Obtains the device chip ID.
 *
 * Public function defined in phy.h.
 */
uint8 PHY_GetChipID( void )
{
  uint8 chipID = PHY_GET_CHIP_ID();

  // verify it is valid
  if ( (chipID == PHY_CHIP_ID_CC2540) ||
       (chipID == PHY_CHIP_ID_CC2541) ||
       (chipID == PHY_CHIP_ID_CC2541S) )
  {
    // return valid chip ID
    return( chipID );
  }

  // fatal error; wrong chip!
  PHY_ASSERT( FALSE );

  return( 0xFF );
}


/*******************************************************************************
 * Obtains the device chip version.
 *
 * Public function defined in phy.h.
 */
uint8 PHY_GetChipVer( void )
{
  uint8 chipVer = PHY_GET_CHIP_VER();

  // verify it is valid
  if ( (chipVer == PHY_CHIP_VER_C) ||
       (chipVer == PHY_CHIP_VER_D) )
  {
    // return valid chip ID
    return( chipVer );
  }

  // fatal error; wrong chip version!
  PHY_ASSERT( FALSE );

  return( 0xFF );
}


/*******************************************************************************
 * Holds the nR in Reset, then releases it.
 *
 * Note: This will clear any nR pending interrupts.
 *
 * Public function defined in phy.h.
 */
void PHY_Reset( void )
{
  // hold nR in reset
  LLECTRL &= ~BV(0);

#if defined( CC2541) || defined( CC2541S )
  // select BLE and release nR from reset
  LLECTRL |= (BV(1) | BV(0));
#else // CC2540
  // release nR from Reset
  LLECTRL |= BV(0);
#endif // CC2541 || CC2541S

  // wait for nR to be ready
  while( LLESTAT != 0x04 );

  return;
}


/*******************************************************************************
 * Copies nanoRisc image from the array to nanoRisc memory location.
 *
 * Note: After the load, the nanoRisc is left in reset.
 *
 * Public function defined in phy.h.
 */
void PHY_LoadNR( void )
{
  uint16 i;

  // hold nR in reset
  LLECTRL &= ~BV(0);

  // set nR start address
  RFPMAH = 0x00; // high address byte
  RFPMAL = 0x00; // low address byte

  // copy image to LLE RAM
  // ALT: COULD USE DMA.
  for(i=0; i <NR_IMAGE_SIZE; i++)
  {
    // write to LLE program memory
    // Note: RFPMD is autoincremented.
    RFPMD = NanoRiscImage[i];
  }

  return;
}


/*******************************************************************************
 * Verifies that the nanoRisc image in the memory is not corrupted. Compares
 * the RAM memory data with the image array which is located in the code
 * section.
 *
 * Public function defined in phy.h.
 */
phyStatus_t PHY_VerifyNR( void )
{
  uint16 i;

  // set nR start address
  RFPMAH = 0x00; // high address byte
  RFPMAL = 0x00; // low address byte

  // read/compare image
  for(i=0; i<NR_IMAGE_SIZE; i++)
  {
    // Note: RFPMD is autoincremented.
    if ( RFPMD != NanoRiscImage[i] )
    {
      break;
    }
  }

  if ( i == NR_IMAGE_SIZE )
  {
    return PHY_STATUS_SUCCESS;
  }
  else
  {
    return PHY_STATUS_FAILURE;
  }
}


/*******************************************************************************
 * Clears the nanoRisc register bank, and the TX and RX FIFO banks.
 *
 * Public function defined in phy.h.
 */
void PHY_ClearAllRegsAndFifos( void )
{
  uint16 i;
  uint8 currentConfig = RFRAMCFG;

  // set the data bank to the nanoRisc registers (i.e. to bank 0)
  RFRAMCFG = 0;

  // clear three banks of memory (nR register memory, TX FIFO, and RX FIFO)
  for (i=0; i<(3*BLE_BANK_SIZE); i++)
  {
    *((uint8 *)(BLE_REG_BASE_ADDR + i)) = 0;
  }

  // restore data bank that was last mapped
  RFRAMCFG = currentConfig;

  // clear semaphores
  SEMAPHORE0 = 1;
  SEMAPHORE1 = 1;
  SEMAPHORE2 = 1;

  return;
}

/*******************************************************************************
 * Get the specified semaphore.
 *
 * RAM register SEMAPHORE 0/1/2 provides atomic read/modify/write
 * operation. If the register is read and value in the register was 1,
 * it returns 1 and sets the register value to 0. If the register is
 * read and the value was 0, it returns 0 and the reg value remains
 * unchanged.
 *
 * Public function defined in phy.h.
 */
phyStatus_t PHY_GetSem( phySemId_t semId )
{
  uint8 sem = 0;

#ifdef DEBUG
  PHY_ASSERT(semId <= PHY_SEM_2);
#endif // DEBUG

  if(semId == PHY_SEM_0)
  {
    sem = (SEMAPHORE0 & 0x1);
  }
  else if(semId == PHY_SEM_1)
  {
    sem = (SEMAPHORE1 & 0x1);
  }
  else if(semId == PHY_SEM_2)
  {
    sem = (SEMAPHORE2 & 0x1);
  }

  if(sem)
  {
    return PHY_STATUS_SUCCESS;
  }
  else
  {
    return PHY_STATUS_FAILURE;
  }
}


/*******************************************************************************
 * Free the specified semaphore.
 *
 * RAM register SEMAPHORE 0/1/2 provides atomic read/modify/write
 * operation. If the register is read and value in the register was 1,
 * it returns 1 and sets the register value to 0. If the register is
 * read and the value was 0, it returns 0 and the reg value remains
 * unchanged. To release the semaphore, write a 1 to the register.
 *
 * Note: This routine first reads the semaphore to check if it is already free.
 *
 * Public function defined in phy.h.
 */
phyStatus_t PHY_ReleaseSem( phySemId_t semId )
{
  phyStatus_t status = PHY_STATUS_SUCCESS;

#ifdef DEBUG
  PHY_ASSERT( semId <= PHY_SEM_2 );
#endif //DEBUG

  if ( semId == PHY_SEM_0 )
  {
    // check if the semaphore is already released (i.e. reg value is 1)
    // Note: If so, it is an error condition.
    // Note that by reading the semaphore register, we are modifying its state.
    // If register value is read as 1 (sem was already in released state),
    // the h/w will reset it back to 0. In ideal scenario, the register value
    // should read as 0.
    if(SEMAPHORE0 & 0x1)
    {
      // it is already released
      status = PHY_STATUS_FAILURE;
    }
    else
    {
      // release semaphore by writing 1 to the register
      SEMAPHORE0 |= 0x1;
    }
  }
  else if ( semId == PHY_SEM_1 )
  {
    if(SEMAPHORE1 & 0x1)
    {
      // it is already released
      status = PHY_STATUS_FAILURE;
    }
    else
    {
      // release semaphore by writing 1 to the register
      SEMAPHORE1 |= 0x1;
    }
  }
  else if ( semId == PHY_SEM_2 )
  {
    if(SEMAPHORE2 & 0x1)
    {
      // it is already released
      status = PHY_STATUS_FAILURE;
    }
    else
    {
      // release semaphore by writing 1 to the register
      SEMAPHORE2 |= 0x1;
    }
  }

  return status;
}


/*******************************************************************************
 * Set Own device address and address type.
 *
 * Public function defined in phy.h.
 */
void PHY_SetOwnAddr( uint8         *addr,
                     phyAddrType_t addrType )
{
#ifdef DEBUG
  PHY_ASSERT(addr != NULL);
#endif //DEBUG

  BLE_OWNADDR_0 = addr[0];
  BLE_OWNADDR_1 = addr[1];
  BLE_OWNADDR_2 = addr[2];
  BLE_OWNADDR_3 = addr[3];
  BLE_OWNADDR_4 = addr[4];
  BLE_OWNADDR_5 = addr[5];

  if(addrType == PHY_ADDR_TYPE_PUBLIC)
  {
    BLE_ADDRTYPE &= ~BV(PHY_ADDRTYPE_OWN_BIT);
  }
  else
  {
    BLE_ADDRTYPE |= BV(PHY_ADDRTYPE_OWN_BIT);
  }
}


/*******************************************************************************
 * Set Peer device address and address type.
 *
 * Public function defined in phy.h.
 */
void PHY_SetPeerAddr( uint8 *addr, phyAddrType_t addrType )
{
#ifdef DEBUG
  PHY_ASSERT(addr != NULL);
#endif //DEBUG

  BLE_PEERADDR_0 = addr[0];
  BLE_PEERADDR_1 = addr[1];
  BLE_PEERADDR_2 = addr[2];
  BLE_PEERADDR_3 = addr[3];
  BLE_PEERADDR_4 = addr[4];
  BLE_PEERADDR_5 = addr[5];

  if(addrType == PHY_ADDR_TYPE_PUBLIC)
  {
    BLE_ADDRTYPE &= ~BV(PHY_ADDRTYPE_PEER_BIT);
  }
  else
  {
    BLE_ADDRTYPE |= BV(PHY_ADDRTYPE_PEER_BIT);
  }
}


/*******************************************************************************
 * Set initial CRC value used in a connection.
 *
 * Public function defined in phy.h.
 */
void PHY_SetCRCInit( uint32 crcInit )
{
#ifdef DEBUG
  PHY_ASSERT(!(crcInit >> 24));
#endif //DEBUG

  // set initial CRC value
  BLE_CRCINIT_0 = (crcInit & 0xFF);
  BLE_CRCINIT_1 = (crcInit >>  8) & 0xFF;
  BLE_CRCINIT_2 = (crcInit >> 16) & 0xFF;
}


/*******************************************************************************
 * Read the configured advertising channels to be used.
 *
 * Public function defined in phy.h.
 */
phyChanMap_t PHY_GetAdvChans( void )
{
  return( BLE_CHANMAP );
}


/*******************************************************************************
 * Configure the advertising channels to be used.
 *
 * Public function defined in phy.h.
 */
void PHY_SetAdvChans( phyChanMap_t chanMap )
{
  BLE_CHANMAP = chanMap & 0x07;

  return;
}


/*******************************************************************************
 * Configure the data channel to be used:
 * Master/Slave use data channels 0-36.
 * Scanner/Initiator use data channels 37-39.
 *
 * Public function defined in phy.h.
 */
NEAR_FUNC void PHY_SetDataChan( uint8 dataChan )
{
#ifdef DEBUG
  PHY_ASSERT(dataChan <= 39);
#endif //DEBUG

  BLE_CHAN = dataChan;
}


/*******************************************************************************
 * Read the configured data channel used.
 *
 * Public function defined in phy.h.
 */
NEAR_FUNC uint8 PHY_GetDataChan( void )
{
  return( BLE_CHAN );
}


/*******************************************************************************
 * Configure the ENDC bit in pkt. If set, the connection is closed after the
 * next packet is received from the slave.
 *
 * Public function defined in phy.h.
 */
void PHY_SetEndConnection( phyEndConn_t endConn )
{
  if(endConn == PHY_END_CONN_AFTER_NEXT_PKT)
  {
    BLE_CONF |= BV(PHY_CONF_ENDC_BIT);
  }
  else
  {
    BLE_CONF &= ~BV(PHY_CONF_ENDC_BIT);
  }
}


/*******************************************************************************
 * Configures how the More Data (MD) bit is populated in the data PDU.
 *
 * Public function defined in phy.h.
 */
void PHY_ConfigureMD( phyConfMD_t moreData )
{
  BLE_CONF = (BLE_CONF & ~PHY_CONF_MD_MASK)| moreData;
}


/*******************************************************************************
 * Set the Scanner backoff count (1-256).
 *
 * Public function defined in phy.h.
 */
void PHY_SetBackoffCnt( uint16 count )
{
#ifdef DEBUG
  PHY_ASSERT( (count <= 256) && (count > 0));
#endif //DEBUG

  // multi-byte fields are little-endian (little end @ little address)
  BLE_BACKOFFCNT_0 = count & 0xFF;
  BLE_BACKOFFCNT_1 = (count >> 8) & 0x01;
}


/*******************************************************************************
 * Configure Scanner to scan active or passive.
 *
 * Public function defined in phy.h.
 */
void PHY_SetScanMode( phyScanMode_t scanMode )
{
  if ( scanMode == PHY_SCAN_MODE_PASSIVE )
  {
    BLE_SCANCONF &= ~BV(PHY_SCANCONF_ACTPASS_BIT);
  }
  else // PHY_SCAN_MODE_ACTIVE
  {
    BLE_SCANCONF |= BV(PHY_SCANCONF_ACTPASS_BIT);
  }
  return;
}

/*******************************************************************************
 * Configure Scanner to end after a Scan Request is received for which no Scan
 * Response will be sent.
 *
 * Public function defined in phy.h.
 */
void PHY_SetScanEnd( phyScanEnd_t scanEnd )
{
  if ( scanEnd == PHY_SCAN_END_ON_ADV_REPORT_DISABLED )
  {
    BLE_SCANCONF &= ~BV(PHY_SCANCONF_END_BIT);
  }
  else // PHY_SCAN_END_ON_ADV_REPORT_ENABLED
  {
    BLE_SCANCONF |= BV(PHY_SCANCONF_END_BIT);
  }
  return;
}


/*******************************************************************************
 * Returns the task end cause.
 *
 * Public function defined in phy.h.
 */
uint8 PHY_GetEndCause( void )
{
  return BLE_ENDCAUSE;
}


/*******************************************************************************
 * Check if there is a valid anchor point value.
 *
 * Public function defined in phy.h.
 */
uint8 PHY_AnchorValid( void )
{
  return BLE_ANCHOR_VALID;
}


/*******************************************************************************
 * Initialize the connection sequencing status to the default value.
 *
 * Public function defined in phy.h.
 */
void PHY_InitSeqStat( void )
{
  BLE_SEQSTAT = PHY_INIT_SEQSTAT_VALUE;
}


/*******************************************************************************
 * Returns an indication of whether the first packet in a new connection, or
 * updated connection, has been received.
 *
 * Note: The logic of returned value is true-low.
 *
 * Public function defined in phy.h.
 */
uint8 PHY_FirstPktReceived( void )
{
  return ((BLE_SEQSTAT >> PHY_SEQSTAT_FIRSTPKT_BIT) & 0x1);
}


/*******************************************************************************
 * Returns an indication of whether the last successfully received
 * packet that was a LL control packet has been ACK'ed or not.
 *
 * Public function defined in phy.h.
 */
uint8 PHY_CtrlAckPending( void )
{
  return ((BLE_SEQSTAT >> PHY_SEQSTAT_CTRL_ACK_PENDING_BIT) & 0x1);
}


/*******************************************************************************
 * Set synchronization word.
 *
 * Note: The synch word lenght is fixed at four bytes.
 *
 * Public function defined in phy.h.
 */
void PHY_SetSyncWord( uint32 syncWord )
{
#if !defined( CC2541 ) && !defined( CC2541S ) // CC2540
  // set sync word length to 32 bits
  // Note: Not used for CC2541. SW_CONF at same locatin is used instead.
  SW_LEN = 0x20;
#endif // !CC2541 && !CC2541S

  // set the sync word
  SW0 = syncWord & 0xFF;
  SW1 = (syncWord >>  8) & 0xFF;
  SW2 = (syncWord >> 16) & 0xFF;
  SW3 = (syncWord >> 24) & 0xFF;
}


/*******************************************************************************
 * Set the maximum number of NACKs allowed to be received before ending the
 * connection event.
 *
 * Note: A value of zero disables this feature.
 *
 * Public function defined in phy.h.
 */
void PHY_SetMaxNack( uint8 maxNackCount )
{
  BLE_MAXNACK = maxNackCount;
}


/*******************************************************************************
 * Clears the white list table and the number of table entries.
 *
 * Public function defined in phy.h.
 */
void PHY_ClearWhitelist( void )
{
  phyXRegPtr_t wlEntryPtr;
  uint8 i;

  // clear white list entry counter
  numWLEntries = 0;

  // clear the number of valid white list entries
  BLE_WLVALID = 0x00;

  // set all WL address types to public
  BLE_WLADDRTYPE = 0x00;

  // point to start of white list table
  wlEntryPtr = BLE_WL_PTR;

  // clear all entries in WL table
  for(i=0; i<(PHY_MAX_NUM_OF_WL_ENTRIES * PHY_DEV_ADDR_SIZE); i++)
  {
    *wlEntryPtr++ = 0x00;
  }
}


/*******************************************************************************
 * Clears the white list table entry.
 *
 * Public function defined in phy.h.
 */
void PHY_ClearWlEntry( uint8 wlEntryIndex )
{
  uint8 i;
  phyXRegPtr_t wlEntryPtr = BLE_WL_PTR + (wlEntryIndex*PHY_DEV_ADDR_SIZE);

  // clear the entry valid bit
  BLE_WLVALID &= ~BV( wlEntryIndex );

  // set the entry address types to public
  BLE_WLADDRTYPE &= BV( wlEntryIndex );

  // reduce the number of white list entries
  numWLEntries--;

  // clear address
  for(i=0; i<(PHY_MAX_NUM_OF_WL_ENTRIES * PHY_DEV_ADDR_SIZE); i++)
  {
    *wlEntryPtr++ = 0x00;
  }

  return;
}


/*******************************************************************************
 * Clears the white list table entries that correspond to blacklist entries.
 *
 * Public function defined in phy.h.
 */
void PHY_ClearWlBasedOnBl( void )
{
  uint8 i;

  // find the next available entry
  for (i=0; i<PHY_MAX_NUM_OF_WL_ENTRIES; i++)
  {
    // check if entry in blacklist is set
    if ( BLE_BLACKLIST & BV(i) )
    {
      // yes, so clear corresponding white list entry
      PHY_ClearWlEntry( i );
    }
  }

  return;
}


/*******************************************************************************
 * Returns the number of available white list entries.
 *
 * Public function defined in phy.h.
 */
uint8 PHY_GetNumFreeWlEntries( void )
{
  return ( (uint8)(PHY_MAX_NUM_OF_WL_ENTRIES - numWLEntries) );
}


/*******************************************************************************
 * Returns the number of white list entries.
 *
 * Public function defined in phy.h.
 */
uint8 PHY_GetWlSize( void )
{
  return ( PHY_MAX_NUM_OF_WL_ENTRIES );
}


/*******************************************************************************
 * Adds a white list entry. The address is added at the next available index.
 * If the table is full, it returns a failure.
 *
 * Public function defined in phy.h.
 */
phyStatus_t PHY_AddWlEntry( uint8          *addr,
                            phyAddrType_t  addrType,
                            phyBlacklist_t setBlacklist )
{
  uint8        i, j;
  phyXRegPtr_t wlEntryPtr;

#ifdef DEBUG
  PHY_ASSERT(addr != NULL);
#endif //DEBUG

  // check if there is room for the addition
  if( numWLEntries == PHY_MAX_NUM_OF_WL_ENTRIES )
  {
    // white list table is full
    return( PHY_STATUS_WL_FULL );
  }

  // check if this address and address type already exists in the white list
  if ( phyFindWlEntry( addr, addrType ) != PHY_NO_WL_MATCH_FOUND )
  {
    // yes, so return without doing anything
    return( PHY_STATUS_SUCCESS );
  }

  // find the next available entry
  for (i=0; i<PHY_MAX_NUM_OF_WL_ENTRIES; i++)
  {
    // is this entry free?
    if ( !(BLE_WLVALID & BV(i)) )
    {
      // yes, so set it as valid
      BLE_WLVALID |= BV(i);

      // set a pointer to the white list entry to be used
      wlEntryPtr = BLE_WL_PTR + (i*PHY_DEV_ADDR_SIZE);

      // copy the supplied address to the WL entry
      for(j=0; j<PHY_DEV_ADDR_SIZE; j++)
      {
        *wlEntryPtr++ = *addr++;
      }

      // specify whether this is public or random address
      // Note: Bits 0-7 represent the address type for the respective entry.
      if( addrType == PHY_ADDR_TYPE_PUBLIC )
      {
        // set the address type to public
        BLE_WLADDRTYPE &= ~BV(i);
      }
      else
      {
        // set the address type to random
        BLE_WLADDRTYPE |= BV(i);
      }

      // bump our count of valid white list entries
      numWLEntries++;

      // check if the corressponding black list flag should be set
      if ( setBlacklist == PHY_SET_BLACKLIST_ENABLE )
      {
        // yes, so set corresponding blacklist entry index
        PHY_SetBlacklistIndex( i );
      }
      else // disabled
      {
        // so clear the corresponding blacklist entry index
        PHY_ClearBlacklistIndex( i );
      }

      return( PHY_STATUS_SUCCESS );
    }
  }

  // fatal error - the WL table should have had at least one free entry
  PHY_ASSERT( FALSE );

  return( PHY_STATUS_FAILURE );
}


/*******************************************************************************
 * Check for an already existing white list entry.
 *
 * Public function defined in phy.h.
 */
/*
phyStatus_t PHY_CheckWlEntry( uint8         *addr,
                              phyAddrType_t addrType )
{
  uint8 i, j;
  phyXRegPtr_t wlEntryPtr;

  // check each valid entry
  for (i=0; i<PHY_MAX_NUM_OF_WL_ENTRIES; i++)
  {
    // is this entry is in use
    if ( BLE_WLVALID & BV(i) )
    {
      // yes, so set a pointer to the white list entry to be checked
      wlEntryPtr = BLE_WL_PTR + (i*PHY_DEV_ADDR_SIZE);

      // and see if it there's a match with input address and type
      if ( osal_memcmp( wlEntryPtr, addr, PHY_DEV_ADDR_SIZE )
      {
        // it's a match, so check the type
        if ( ((BLE_WLADDRTYPE >> i) & 0x01) == addrType )
        {
          // yes, so we have a duplicate entry, so return
          return( PHY_STATUS_WL_ENTRY_FOUND );
        }
      }
    }
  }

  return( PHY_STATUS_WL_ENTRY_NOT_FOUND );
}
*/


/*******************************************************************************
 * Removes a white list entry based on its address and address type. If located,
 * then the entry is set invalid, and the address is cleared. If the table is
 * empty, or the address and address type is not located, it returns a failure.
 *
 * Public function defined in phy.h.
 */
phyStatus_t PHY_RemoveWlEntry( uint8         *addr,
                               phyAddrType_t addrType )
{
  uint8 i, wlEntryIndex;
  phyXRegPtr_t wlEntryPtr;

#ifdef DEBUG
  PHY_ASSERT(addr != NULL);
#endif //DEBUG

  // check if there's at least one entry in the table
  if( numWLEntries == 0 )
  {
    // the white list table is empty
    return PHY_STATUS_WL_EMPTY;
  }

  // search for an address and address type match
  wlEntryIndex = phyFindWlEntry( addr, addrType );

  // was a match found?
  if ( wlEntryIndex != PHY_NO_WL_MATCH_FOUND )
  {
    // yes, so invalidate it
    BLE_WLVALID &= ~BV(wlEntryIndex);

    // set a pointer to the white list entry to be used
    wlEntryPtr = BLE_WL_PTR + (wlEntryIndex*PHY_DEV_ADDR_SIZE);

    // clear the address
    for(i=0; i<PHY_DEV_ADDR_SIZE; i++)
    {
      *wlEntryPtr++ = 0;
    }

    // set the address type to public
    BLE_WLADDRTYPE &= ~BV(wlEntryIndex);

    // decrease our count of valid white list entries
    numWLEntries--;

    // clear the corresponding blacklist entry index
    // Note: This is done in case a user removes an entry from the WL table
    //       while Scanning with a WL policy of "Use WL".
    PHY_ClearBlacklistIndex( wlEntryIndex );

    return PHY_STATUS_SUCCESS;
  }
  else // the entry was not found in the white list table
  {
    return PHY_STATUS_WL_ENTRY_NOT_FOUND;
  }
}


/*******************************************************************************
 * Set the Advertiser's white list policy.
 *
 * Public function defined in phy.h.
 */
void PHY_SetAdvWlPolicy( phyAdvWlPolicy_t wlPolicy )
{
  // clear the Adv WL Policy field
  BLE_WLPOLICY &= 0xFC;

  // set the Adv WL Policy
  BLE_WLPOLICY |= wlPolicy;
}


/*******************************************************************************
 * Set the Scanner's white list policy.
 *
 * Public function defined in phy.h.
 */
void PHY_SetScanWlPolicy( phyScanWlPolicy_t wlPolicy )
{
  if( wlPolicy == PHY_SCANNER_ALLOW_ALL_ADV_PKTS )
  {
    BLE_WLPOLICY &= ~BV(PHY_SCAN_WL_POLICY_BIT);
  }
  else // PHY_SCANNER_USE_WHITE_LIST
  {
    BLE_WLPOLICY |= BV(PHY_SCAN_WL_POLICY_BIT);
  }
}


/*******************************************************************************
 * Set the Initiator's white list policy.
 *
 * Public function defined in phy.h.
 */
void PHY_SetInitWlPolicy( phyInitWlPolicy_t wlPolicy )
{
  if( wlPolicy == PHY_INITIATOR_USE_PEER_ADDR )
  {
    // use the PEERADDR to determine which Adv to connect to
    BLE_WLPOLICY &= ~BV(PHY_INIT_WL_POLICY_BIT);
  }
  else // PHY_INITIATOR_USE_WL
  {
    // the white list is used to determine which Adv to connect to
    BLE_WLPOLICY |= BV(PHY_INIT_WL_POLICY_BIT);
  }
}


/*******************************************************************************
 * Set a white list table entry as being black listed. That is, Advertising
 * packets received with the address and address type of the white list table
 * entry that is black listed are ignored. This feature is only used by the
 * Scanner.
 *
 * Note: The black list bits correspond to the white list table entries. These
 *       are the same table entries used for white listing.
 *
 * Public function defined in phy.h.
 */
void PHY_SetBlacklistIndex( uint8 blackListIndex )
{
  // set the flag in the Blacklist that corresponds to the index
  BLE_BLACKLIST |= BV(blackListIndex);
}


/*******************************************************************************
 * Search the white list table for an entry, and if found, set the corresponding
 * blacklist entry index.
 *
 * Note: The black list bits correspond to the white list table entries. These
 *       are the same table entries used for white listing.
 *
 * Public function defined in phy.h.
 */
phyStatus_t PHY_SetBlacklistEntry( uint8         *addr,
                                   phyAddrType_t addrType )
{
  uint8 wlEntryIndex;

  // search for an address and address type match
  wlEntryIndex = phyFindWlEntry( addr, addrType );

  // was a match found?
  if ( wlEntryIndex != PHY_NO_WL_MATCH_FOUND )
  {
    // we have a valid entry, so set corresponding blacklist entry index
    PHY_SetBlacklistIndex( wlEntryIndex );

    return( PHY_STATUS_SUCCESS );
  }
  else // the entry was not found in the white list table
  {
    return( PHY_STATUS_WL_ENTRY_NOT_FOUND );
  }
}


/*******************************************************************************
 * Clear a blacklist table entry based on a white list entry index.
 *
 * Public function defined in phy.h.
 */
void PHY_ClearBlacklistIndex( uint8 blackListIndex )
{
  // set the flag in the Blacklist that corresponds to the index
  BLE_BLACKLIST &= ~BV(blackListIndex);
}

/*******************************************************************************
 * Clear the black list. All white list table entries are used as normal white
 * list entries. This feature is only used by the Scanner.
 *
 * Public function defined in phy.h.
 */
void PHY_ClearBlacklist( void )
{
  // clear all Blacklist flags
  BLE_BLACKLIST = 0;
}


/*******************************************************************************
 * Save the white list to Bank 1. This feature is only used by the Scanner.
 *
 * Public function defined in phy.h.
 */
void PHY_SaveWhiteList( uint8 wlOwner )
{
  uint8 i, j;
  uint8 wlValid;
  uint8 wlAddrType;
  uint8 bl;
  uint8 *destPtr = (uint8 *)BLE_REG_BASE_ADDR;

  // check who's WL/BL we are saving
  if ( wlOwner == PHY_SCAN_WL )
  {
    // then this is a Scanner's WL/BL we must save, so offset data past user's;
    // the BL, WL Valid bits, and WL Addr Type bits are each one byte - the WL
    // is 48 byte (8 x 6 bytes), so the total Host space is is 51 bytes
    destPtr += PHY_SAVED_WL_BL_SIZE;
  }

  // copy WL valid bits and address type bits from current bank (Bank 0)
  bl         = BLE_BLACKLIST;
  wlValid    = BLE_WLVALID;
  wlAddrType = BLE_WLADDRTYPE;

  // map in Bank 1
  PHY_MapDataBank( PHY_RF_RAM_BANK_1 );

  // save the valid bits and WL address type bits
  *destPtr++ = bl;
  *destPtr++ = wlValid;
  *destPtr++ = wlAddrType;

  // map in Bank 0
  PHY_MapDataBank( PHY_RF_RAM_BANK_0 );

  // copy any valid entries from Bank 0 to Bank 1
  for (i=0; i<PHY_MAX_NUM_OF_WL_ENTRIES; i++)
  {
    uint8 wlData[ PHY_DEV_ADDR_SIZE ];

    // check if entry is valid
    if ( wlValid & BV(i) )
    {
      // copy address
      for (j=0; j<PHY_DEV_ADDR_SIZE; j++)
      {
        wlData[j] = *(BLE_WL_PTR+(i*PHY_DEV_ADDR_SIZE)+j);
      }

      // map in Bank 1
      PHY_MapDataBank( PHY_RF_RAM_BANK_1 );

      // save address
      for (j=0; j<PHY_DEV_ADDR_SIZE; j++)
      {
        *(destPtr+((i*PHY_DEV_ADDR_SIZE)+j)) = wlData[j];
      }

      // map in Bank 0
      PHY_MapDataBank( PHY_RF_RAM_BANK_0 );
    }
  }

  return;
}


/*******************************************************************************
 * Restore the white list from Bank 1. This feature is only used by the Scanner.
 *
 * Public function defined in phy.h.
 */
void PHY_RestoreWhiteList( uint8 wlOwner )
{
  uint8 i, j;
  uint8 wlValid;
  uint8 wlAddrType;
  uint8 bl;
  uint8 *destPtr = (uint8 *)BLE_REG_BASE_ADDR;

  // check who's WL/BL we are saving
  if ( wlOwner == PHY_SCAN_WL )
  {
    // then this is a Scanner's WL/BL we must restore, so offset data past user;
    // the BL, WL Valid bits, and WL Addr Type bits are each one byte - the WL
    // is 48 byte (8 x 6 bytes), so the total Host space is is 51 bytes
    destPtr += PHY_SAVED_WL_BL_SIZE;
  }

  // map in Bank 1
  PHY_MapDataBank( PHY_RF_RAM_BANK_1 );

  // read saved WL valid bits and address type bits
  bl         = *destPtr++;
  wlValid    = *destPtr++;
  wlAddrType = *destPtr++;

  // map in Bank 0
  PHY_MapDataBank( PHY_RF_RAM_BANK_0 );

  // copy the valid bits and WL address type bits
  BLE_BLACKLIST  = bl;
  BLE_WLVALID    = wlValid;
  BLE_WLADDRTYPE = wlAddrType;

  // copy any valid entries from Bank 1 to Bank 0
  for (i=0; i<PHY_MAX_NUM_OF_WL_ENTRIES; i++)
  {
    uint8 wlData[ PHY_DEV_ADDR_SIZE ];

    // check if entry is valid
    if ( wlValid & BV(i) )
    {
      // map in Bank 1
      PHY_MapDataBank( PHY_RF_RAM_BANK_1 );

      // copy saved address
      for (j=0; j<PHY_DEV_ADDR_SIZE; j++)
      {
        wlData[j] = *(destPtr+(i*PHY_DEV_ADDR_SIZE)+j);
      }

      // map in Bank 0
      PHY_MapDataBank( PHY_RF_RAM_BANK_0 );

      // restore saved address
      for (j=0; j<PHY_DEV_ADDR_SIZE; j++)
      {
        *(BLE_WL_PTR+((i*PHY_DEV_ADDR_SIZE)+j)) = wlData[j];
      }

      // and count the entry
      numWLEntries++;
    }
  }

  return;
}


/*******************************************************************************
 * Clears the save WL/BL context in Bank 1. This feature is only used by the
 * Scanner.
 *
 * Public function defined in phy.h.
 */
void PHY_ClearSavedWhiteList( uint8 wlOwner )
{
  uint8 *destPtr = (uint8 *)BLE_REG_BASE_ADDR;

  // check who's WL/BL we are saving
  if ( wlOwner == PHY_SCAN_WL )
  {
    // then this is a Scanner's WL/BL we must save, so offset data past user's;
    // the BL, WL Valid bits, and WL Addr Type bits are each one byte - the WL
    // is 48 byte (8 x 6 bytes), so the total Host space is is 51 bytes
    destPtr += PHY_SAVED_WL_BL_SIZE;
  }

  // map in Bank 1
  PHY_MapDataBank( PHY_RF_RAM_BANK_1 );

  // clear the WL Valid Bits, WL Address Types, and BL
  *destPtr++ = 0;
  *destPtr++ = 0;
  *destPtr++ = 0;

  // map in Bank 0
  PHY_MapDataBank( PHY_RF_RAM_BANK_0 );

  return;
}


/*******************************************************************************
 * Send a command to the LLE nanoRisc.
 *
 * Public function defined in phy.h.
 */
void PHY_Command( phyCmd_t cmd )
{
  while (RFST != 0);

  // only issue the command if it is valid
  if ( cmd != PHY_CMD_INVALID )
  {
    RFST = cmd;
  }
}


/*******************************************************************************
 * Clears all LLE nanoRisc counters.
 *
 * Public function defined in phy.h.
 */
void PHY_ClearCounters( void )
{
  // transmitter
  BLE_NTXDONE     = 0;
  BLE_NTXACK      = 0;
  BLE_NTXCTRLACK  = 0;
  BLE_NTXCTRL     = 0;
  BLE_NTXRETRANS  = 0;
  BLE_NTX         = 0;

  // receiver
  BLE_NRXOK       = 0;
  BLE_NRXCTRL     = 0;
  BLE_NRXNOK      = 0;
  BLE_NRXIGNORED  = 0;
  BLE_NRXEMPTY    = 0;
  BLE_NRXFIFOFULL = 0;
}


/*******************************************************************************
 * Returns all the counter values.
 *
 * Public function defined in phy.h.
 */
void PHY_ReadCounters( uint8 *cntPtr )
{
#ifdef DEBUG
  PHY_ASSERT( cntPtr != NULL );
#endif //DEBUG

  // transmitter
  *cntPtr++ = BLE_NTXDONE;
  *cntPtr++ = BLE_NTXACK;
  *cntPtr++ = BLE_NTXCTRLACK;
  *cntPtr++ = BLE_NTXCTRL;
  *cntPtr++ = BLE_NTXRETRANS;
  *cntPtr++ = BLE_NTX;

  // receiver
  *cntPtr++ = BLE_NRXOK;
  *cntPtr++ = BLE_NRXCTRL;
  *cntPtr++ = BLE_NRXNOK;
  *cntPtr++ = BLE_NRXIGNORED;
  *cntPtr++ = BLE_NRXEMPTY;
  *cntPtr   = BLE_NRXFIFOFULL;
}


/*******************************************************************************
 * Configure FIFO data processing. This includes flushing ignored packets,
 * packets that fail the CRC, and empty packets. Also, whether a RX status
 * containing the packets's data channel number and RSSI should be appended
 * to the end of the packet.
 *
 * Public function defined in phy.h.
 */
void PHY_ConfigureFifoDataProcessing( phyFifoData_t fifoDataConf )
{
  BLE_FIFO_CONF = fifoDataConf;
}


/*******************************************************************************
 * Configure if nanoRisc should append the RF status to the end of received
 * packets.
 *
 * Public function defined in phy.h.
 */
void PHY_ConfigureAppendRfStatus(phyAppendRfStatus_t appendRfStatus)
{
#ifdef DEBUG
  PHY_ASSERT( (appendRfStatus == PHY_ENABLE_APPEND_RF_STATUS) ||
              (appendRfStatus == PHY_DISABLE_APPEND_RF_STATUS) );
#endif //DEBUG

  if ( appendRfStatus == PHY_ENABLE_APPEND_RF_STATUS )
  {
    BLE_FIFO_CONF |= PHY_FIFO_DATA_APPEND_RX_STATUS;
  }
  else  // PHY_DISABLE_APPEND_RF_STATUS
  {
    BLE_FIFO_CONF &= ~PHY_FIFO_DATA_APPEND_RX_STATUS;
  }
}


/*******************************************************************************
 * Configure FIFO configuration operations. This includes automatic deallocation
 * of RX packet and commit of TX packet by MCU, and automatic commit of RX
 * packet and deallocate of TX packet by nR.
 *
 * Public function defined in phy.h.
 */
void PHY_SetFifoConfig( phyFifoConfig_t fifoConfig )
{
  RFFCFG = fifoConfig;
}


/*******************************************************************************
 * Set the Last RSSI value to invalid.
 *
 * Public function defined in phy.h.
 */
void PHY_InitLastRssi( void )
{
  BLE_LAST_RSSI = PHY_RSSI_VALUE_INVALID;
}


/*******************************************************************************
 * Get the Last RSSI value captured by the nanoRisc.
 *
 * Note: The returned value is returned as an unsigned integer!
 * Note: The returned value is not corrected for RX gain.
 *
 * Public function defined in phy.h.
 */
uint8 PHY_GetLastRssi( void )
{
  return BLE_LAST_RSSI;
}


/*******************************************************************************
 * Get the RF RSSI value.
 *
 * Note: The returned value is returned as an unsigned integer!
 * Note: This call should only be made when RX is active.
 * Note: An invalid value is -128.
 * Note: The returned value is not corrected for RX gain.
 *
 * Public function defined in phy.h.
 */
uint8 PHY_GetRssi( void )
{
  return RSSI;
}


/*******************************************************************************
 * Clear Window Configuration
 *
 * Public function defined in phy.h.
 */
void PHY_ClearWinConfig( void )
{
  BLE_WINCONF = 0;

  return;
}


/*******************************************************************************
 * Set Dynamic Window Offset
 *
 * Public function defined in phy.h.
 */
void PHY_SetDynamicWinOffset( uint16 winOffset,
                              uint16 connInterval )
{
  // set the initial window offset
  BLE_WINOFFSET_0 = winOffset & 0xFF;
  BLE_WINOFFSET_1 = winOffset >> 8;

  // set the modulo value of the window offset, which corresponds to the
  // new connection's interval
  BLE_WINMOD_0 = connInterval & 0xFF;
  BLE_WINMOD_1 = connInterval >> 8;

  // direct the nR to use dynamic window offset feature
  BLE_WINCONF = 0xC1;

  return;
}


/*******************************************************************************
 * Configure the whitener as none, only BLE Pseudo Number 7 (PN7), only the TI
 * Pseudo Number 9 (PN9), or both.
 *
 * Public function defined in phy.h.
 */
void PHY_ConfigWhitener( phyWhitener_t whitener )
{
  switch( whitener )
  {
    case PHY_DISABLE_ALL_WHITENER:
      BSP_MODE &= ~(PHY_PN7_WHITENER_BIT | PHY_PN9_WHITENER_BIT);
      break;

    case PHY_ENABLE_BLE_WHITENER:
      BSP_MODE |= PHY_PN7_WHITENER_BIT;
      break;

    case PHY_DISABLE_BLE_WHITENER:
      BSP_MODE &= ~PHY_PN7_WHITENER_BIT;
      break;

    case PHY_ENABLE_PN9_WHITENER:
      BSP_MODE |= PHY_PN9_WHITENER_BIT;
      break;

    case PHY_DISABLE_PN9_WHITENER:
      BSP_MODE &= ~PHY_PN9_WHITENER_BIT;
      break;

    case PHY_ENABLE_ONLY_BLE_WHITENER:
      BSP_MODE &= ~PHY_PN9_WHITENER_BIT;
      BSP_MODE |= PHY_PN7_WHITENER_BIT;
      break;

    case PHY_ENABLE_ONLY_PN9_WHITENER:
      BSP_MODE &= ~PHY_PN7_WHITENER_BIT;
      BSP_MODE |= PHY_PN9_WHITENER_BIT;
      break;

    case PHY_ENABLE_ALL_WHITENER:
      BSP_MODE |= (PHY_PN7_WHITENER_BIT | PHY_PN9_WHITENER_BIT);
      break;

    default:
      // fatal error - not possible
      PHY_ASSERT( FALSE );
  }

  return;
}


/*******************************************************************************
 * Reset the TX FIFO (i.e. TX RP=SRP=WP=SWP=0).
 *
 * Public function defined in phy.h.
 */
void PHY_ResetTxFifo( void )
{
  RFST = PHY_FIFO_TX_RESET; // formally RFFCMD
}


/*******************************************************************************
 * Reset the RX FIFO (i.e. RX RP=SRP=WP=SWP=0).
 *
 * Public function defined in phy.h.
 */
void PHY_ResetRxFifo( void )
{
  RFST = PHY_FIFO_RX_RESET; // formally RFFCMD
}


/*******************************************************************************
 * Reset the TX and RX FIFOs (i.e. TX and RX RP=SRP=WP=SWP=0).
 *
 * Public function defined in phy.h.
 */
void PHY_ResetTxRxFifo( void )
{
  RFST = PHY_FIFO_RESET; // formally RFFCMD
}


/*******************************************************************************
 * Reads the number of available bytes in the TX FIFO.
 *
 * Public function defined in phy.h.
 */
uint8 PHY_TxFifoBytesFree( void )
{
  return( PHY_FIFO_SIZE - RFTXFLEN );
}


/*******************************************************************************
 * Reads the number of used bytes in the TX FIFO.
 *
 * Public function defined in phy.h.
 */
uint8 PHY_TxFifoLen( void )
{
  return RFTXFLEN;
}


/*******************************************************************************
 * Reads the number of used bytes in the RX FIFO.
 *
 * Public function defined in phy.h.
 */
uint8 PHY_RxFifoLen( void )
{
  return RFRXFLEN;
}


/*******************************************************************************
 * Issue a TX FIFO Retry operation (i.e. TX RP=SRP).
 *
 * Public function defined in phy.h.
 */
void PHY_RetryTxFifo( void )
{
  RFST = PHY_FIFO_TX_RETRY; // formally RFFCMD
}

/*******************************************************************************
 * Issue a RX FIFO Retry operation (i.e. RX RP=SRP).
 *
 * Public function defined in phy.h.
 */
void PHY_RetryRxFifo( void )
{
  RFST = PHY_FIFO_RX_RETRY; // formally RFFCMD
}


/*******************************************************************************
 * Issue a TX FIFO Commit operation (i.e. TX SWP=WP).
 *
 * Public function defined in phy.h.
 */
void PHY_CommitTxFifo( void )
{
  RFST = PHY_FIFO_TX_COMMIT; // formally RFFCMD
}


/*******************************************************************************
 * Issue a RX FIFO Commit operation (i.e. RX SWP=WP).
 *
 * Public function defined in phy.h.
 */
void PHY_CommitRxFifo( void )
{
  RFST = PHY_FIFO_RX_COMMIT; // formally RFFCMD
}


/*******************************************************************************
 * Issue a RX FIFO Discard operation (i.e. RX WP=SWP).
 *
 * Public function defined in phy.h.
 */
void PHY_DiscardTxFifo( void )
{
  RFST = PHY_FIFO_TX_DISCARD; // formally RFFCMD
}


/*******************************************************************************
 * Issue a RX FIFO Deallocate operation (i.e. RX SRP=RP).
 *
 * Public function defined in phy.h.
 */
void PHY_DeallocateRxFifo( void )
{
  RFST = PHY_FIFO_RX_DEALLOC; // formally RFFCMD
}


/*******************************************************************************
 * Set the RX FIFO Threshold.
 *
 * Public function defined in phy.h.
 */
void PHY_SetRxFifoThreshold( uint8 thresLen )
{
  RFRXFTHRS = (thresLen & 0x7F);

  return;
}


/*******************************************************************************
 * Get the TX FIFO Start Write Pointer.
 *
 * Public function defined in phy.h.
 */
uint8 *PHY_GetTxSWP( void )
{
  return( (uint8 *)(RFTXFSWP | BLE_REG_BASE_ADDR) );
}


/*******************************************************************************
 * Get the RX FIFO Start Read Pointer.
 *
 * Public function defined in phy.h.
 */
uint8 *PHY_GetRxSRP( void )
{
  return( (uint8 *)(RFRXFSRP | BLE_REG_BASE_ADDR) );
}


/*******************************************************************************
 * Write multiple bytes of data to TX FIFO.
 *
 * Note: It is assumed the caller does not write more than the amount of free
 *       space available in the TX FIFO.
 *
 * Public function defined in phy.h.
 */
void PHY_WriteByte( uint8 *data, uint8 len )
{
#ifdef DEBUG
  PHY_ASSERT(data != NULL);
#endif //DEBUG

  while(len--)
  {
    RFD = *data++; // formally RFDATA
  }
}


/*******************************************************************************
 * Write a single byte of data to TX FIFO.
 *
 * Note: It is assumed the caller does not write more than the amount of free
 *       space available in the TX FIFO.
 *
 * Public function defined in phy.h.
 */
void PHY_WriteByteVal( uint8 value )
{
  RFD = value; // formally RFDATA
}


/*******************************************************************************
 * Read multiple bytes of data from the RX FIFO.
 *
 * Note: It is assumed the caller does not read more than the amount of
 *       available data in the RX FIFO.
 *
 * Note: A NULL pointer still initiates the read, but discards the data.
 *
 * Public function defined in phy.h.
 */
void PHY_ReadByte( uint8 *ptr,
                   uint8 len )
{
  // check if pointer is NULL
  if ( ptr == NULL )
  {
    // yes, so toss the data
    while(len--)
    {
      RFD; // formally RFDATA
    }
  }
  else // we have a buffer to fill
  {
    // so fill
    while(len--)
    {
      *ptr++ = RFD; // formally RFDATA
    }
  }
}


/*******************************************************************************
 * Read a single byte of data from the RX FIFO.
 *
 * Note: It is assumed the caller does not read more than the amount of
 *       available data in the RX FIFO.
 *
 * Public function defined in phy.h.
 */
uint8 PHY_ReadByteVal( void )
{
  return RFD; // formally RFDATA
}


/*******************************************************************************
 * Map a data bank to the specified RF RAM bank.
 *
 * Public function defined in phy.h.
 */
void PHY_MapDataBank( phyRfRamBank_t bankNum )
{
  RFRAMCFG = bankNum;
}


/*******************************************************************************
 * Saves and restores the TX and RX FIFO for the specified connection to/from
 * the LLE RAM bank memory. Called when the current connection's FIFO data is
 * to be backed up and another connections FIFO is to be restored. The restored
 * connection could be a connection that did not exist in the backup. In this
 * case, the FIFO parameters will be set to reset conditions.
 *
 * Public function defined in phy.h.
 */
void PHY_SaveRestoreConn( uint8 saveConnId,
                          uint8 restoreConnId )
{
  uint8 bankId;

#ifdef DEBUG
  PHY_ASSERT( saveConnId < PHY_BACKUP_CONNECTIONS );
  PHY_ASSERT( restoreConnId < PHY_BACKUP_CONNECTIONS );
#endif //DEBUG

  // We have only 5 memory banks that can be used for FIFO backups. Plus there
  // is the RX FIFO and TX FIFO memory. When we have 3 connections, the active
  // connection's data will be in the TX and RX FIFO. 2 connections will be
  // in 4 backup memory banks and 1 bank will be empty to allow FIFO swapping.
  //
  // A restore operation will free up the backup bank that it was occupying.
  // Also note that the restore will overwrite the active connection's FIFO.
  // A save operation will consume one backup bank.

  // get a memory bank that can be used to backup the connection's TX FIFO and
  // check that at least one bank is available
  bankId = phyGetFreeBank();

#ifdef DEBUG
  PHY_ASSERT(bankId == 0);
#endif //DEBUG

   // First the TX FIFO is saved (for the current conn) and restored
   // (for the next conn) and then the RX FIFO is saved and restored.
   // This is to ensure that in the case when there are 3 active connections,
   // at least one memory bank will be free to swap the FIFOs.
  phySaveFifo( PHY_TXFIFO, saveConnId, bankId );
  phyRestoreFifo( PHY_TXFIFO, restoreConnId );

  // get a memory bank that can be used to backup the connection's RX FIFO and
  // also check that at least one bank is available

#ifdef DEBUG
  PHY_ASSERT( bankId = phyGetFreeBank() );
#endif //DEBUG

  // save one FIFO
  phySaveFifo( PHY_RXFIFO, saveConnId, bankId );

  // restore the other
  phyRestoreFifo( PHY_RXFIFO, restoreConnId) ;
}


/*******************************************************************************
 * Restores TX and RX FIFOs for the specified connection. The data currently in
 * the TX and RX FIFO is lost/overwritten. Call this when the current FIFO data
 * does not need to be saved.
 *
 * Public function defined in phy.h.
 */
void PHY_RestoreConn( uint8 connId )
{
  phyRestoreFifo(PHY_TXFIFO, connId);
  phyRestoreFifo(PHY_RXFIFO, connId);
}


/*******************************************************************************
 * Initializes Raw TX Task start operation (immediate or based on a timer event)
 * and run operation (single shot, or repeat).
 *
 * Public function defined in phy.h.
 */
void PHY_RawTxInit( phyRawStartMode_t startMode,
                    phyRawRunMode_t   runMode )
{
  // configure the TX radio
  BLE_RAW_CONF = 0;
  BLE_RAW_CONF = (startMode << 1) | runMode;

  // set the FIFO command to use after each packet to Retry TX FIFO
  BLE_RAW_FIFOCMD = PHY_FIFO_TX_RETRY;

  // initialize the CRC
  BLE_RAW_CRCINIT0 = 0x00;
  BLE_RAW_CRCINIT1 = 0x55;
  BLE_RAW_CRCINIT2 = 0x55;
  BLE_RAW_CRCINIT3 = 0x55;

  // set the CRC length
  BLE_RAW_TX_CRC_LEN = 3;

  // set the whitening init
  BLE_RAW_WINIT = PHY_WHITENER_INIT;

  // clear the number of TX packets counter
  BLE_RAW_NTX0 = 0x00;
  BLE_RAW_NTX1 = 0x00;

  return;
}


/*******************************************************************************
 * Initializes Raw RX Task start operation (immediate or based on a timer event)
 * and run operation (single shot, or repeat).
 *
 * Public function defined in phy.h.
 */
void PHY_RawRxInit( phyRawStartMode_t startMode,
                    phyRawRunMode_t   runMode )

{
  // configure the RX radio
  BLE_RAW_CONF = 0;
  BLE_RAW_CONF = (startMode << 1) | runMode;

  // set the FIFO command to use after each packet to Discard RX FIFO
  BLE_RAW_FIFOCMD = PHY_FIFO_RX_DISCARD;

  // initialize the CRC
  // Note: This value is per the BLE Core specification.
  BLE_RAW_CRCINIT0 = 0x00;
  BLE_RAW_CRCINIT1 = 0x55;
  BLE_RAW_CRCINIT2 = 0x55;
  BLE_RAW_CRCINIT3 = 0x55;

  // initialize RX length to zero to indicate length is in packet
  BLE_RAW_RX_LEN = 0;

  // set the CRC length
  // Note: The CRC size is per the BLE Core specification.
  BLE_RAW_TX_CRC_LEN = 3;

  // set the whitening init
  BLE_RAW_WINIT = PHY_WHITENER_INIT;

  // clear the RX counters
  BLE_RAW_NRXOK0  = 0x00;
  BLE_RAW_NRXOK1  = 0x00;
  BLE_RAW_NRXNOK0 = 0x00;
  BLE_RAW_NRXNOK1 = 0x00;

  return;
}


/*******************************************************************************
 * Gets the number of transmitted packets.
 *
 * Public function defined in phy.h.
 */
void PHY_RawGetNumTxPkts( uint16 *numTxPkts )
{
  ((uint8 *)numTxPkts)[0] = BLE_RAW_NTX0;
  ((uint8 *)numTxPkts)[1] = BLE_RAW_NTX1;

  return;
}


/*******************************************************************************
 * Gets the number of successfully received packets.
 *
 * Public function defined in phy.h.
 */
void PHY_RawGetNumRxPkts( uint16 *numRxPkts )
{
  ((uint8 *)numRxPkts)[0] = BLE_RAW_NRXOK0;
  ((uint8 *)numRxPkts)[1] = BLE_RAW_NRXOK1;

  return;
}

/*******************************************************************************
 * Gets the number of successfully received packets, the number of packets
 * received with a CRC error, and the last received RSSI (as an unsigned
 * integer).
 *
 * Public function defined in phy.h.
 */
void PHY_RawGetRxData( uint16 *numRxPkts,
                       uint16 *numRxErrPkts,
                       uint8  *rxRSSI )
{
  ((uint8 *)numRxPkts)[0]    = BLE_RAW_NRXOK0;
  ((uint8 *)numRxPkts)[1]    = BLE_RAW_NRXOK1;
  ((uint8 *)numRxErrPkts)[0] = BLE_RAW_NRXNOK0;
  ((uint8 *)numRxErrPkts)[1] = BLE_RAW_NRXNOK1;
  rxRSSI[0]                  = BLE_LAST_RSSI;

  return;
}

/*******************************************************************************
 * Sets the RF frequency for a given RF channel.
 *
 * Note: BLE frequency range is 2402..2480MHz over 40 RF channels.
 *
 * Note: The RF channel is given, not the BLE channel.
 *
 * Public function defined in phy.h.
 */
void PHY_SetRfFreq( uint8         rfChan,
                    phyFreqMode_t freqMode )
{
#ifdef DEBUG
  PHY_ASSERT( rfChan <= PHY_MAX_RF_CHAN );
  PHY_ASSERT( (freqMode == PHY_FREQ_MODE_UNMODULATED_CW) ||
              (freqMode == PHY_FREQ_MODE_MODULATED_CW) );
#endif //DEBUG

  // set frequency independently of CW modulation
  FREQCTRL = (rfChan << 1) + PHY_BASE_FREQ_OFFSET;

  // make adjustements to frequency offset based on CW modulation
  if ( freqMode == PHY_FREQ_MODE_UNMODULATED_CW )
  {
    // set the TX/RX frequency offset to -1MHz
    MDMTEST1 = PHY_TX_TONE_ZERO_HZ;
  }
  else // PHY_FREQ_MODE_MODULATED_CW
  {
    // the BLE transmits -1MHz on RF channels 0..19, and +1MHz on RF channels
    // 20..39, so tune frequency to compensate
    if ( rfChan <= 19 )
    {
      // the BLE transmits at -1MHz, so tune frequency to +1MHz to compensate
      FREQCTRL += 1;

      // set the TX/RX frequency offset to -1MHz
      MDMTEST1 = PHY_TX_TONE_MINUS_ONE_HZ;
    }
    else // RF chan 20..39
    {
      // the BLE transmits at +1MHz, so tune frequency to -1MHz to compensate
      FREQCTRL -= 1;

      // set the TX/RX frequency offset to +1MHz
      MDMTEST1 = PHY_TX_TONE_PLUS_ONE_HZ;
    }
  }

  return;
}


/*
** Local Functions
*/

/*******************************************************************************
 * @fn          phyFindWlEntry
 *
 * @brief       This routine searches the white list table for an address and
 *              address type match. If found, the index of the table entry is
 *              returned. If not found, a fail value is returned.
 *
 * input parameters
 *
 * @param       addr     - Pointer to device address.
 * @param       addrType - PHY_ADDR_TYPE_PUBLIC, PHY_ADDR_TYPE_RANDOM.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      A valid white list table index, or PHY_NO_WL_MATCH_FOUND.
 */
static uint8 phyFindWlEntry( uint8         *addr,
                             phyAddrType_t addrType )
{
  uint8        i, j;
  phyXRegPtr_t wlEntryPtr;

  // search for an address and address type match
  for (i=0; i<PHY_MAX_NUM_OF_WL_ENTRIES; i++)
  {
    // is this entry used?
    if ( BLE_WLVALID & BV(i) )
    {
      // yes, so set the a pointer to the white list entry to be used
      wlEntryPtr = BLE_WL_PTR + (i*PHY_DEV_ADDR_SIZE);

      // compare the address
      for(j=0; j<PHY_DEV_ADDR_SIZE; j++)
      {
        if ( *(wlEntryPtr+j) != *(addr+j) )
        {
          // we have a miss match, so we're done here
          break;
        }
      }

      // did we get a match?
      if ( j == PHY_DEV_ADDR_SIZE )
      {
        // yes, but what about the address type?
        if ( ((BLE_WLADDRTYPE >> i) & 0x01) == addrType )
        {
          // yes, so we found our entry, so return the index
          return( i );

        } // address type match?
      } // address match?
    } // entry valid?
  } // for each entry

  return( PHY_NO_WL_MATCH_FOUND );
}


/*******************************************************************************
 * @fn          phyCopyFIFO
 *
 * @brief       Copies FIFO data. This general purpose function is used to move
 *              data from TX/RX FIFO to the backup memory banks and vice versa.
 *              The "write ptr" and "start of read ptr" params are used to
 *              optimize by copying only the used portion of the FIFO.
 *
 * input parameters
 *
 * @param       destBasePtr - Start address of the destination bank.
 * @param       srcBasePtr  - Start address of the source bank.
 * @param       wp          - FIFO Write Pointer.
 * @param       srp         - FIFO Start Read Pointer.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
static void phyCopyFIFO( uint8 *destBasePtr,
                         uint8 *srcBasePtr,
                         uint8 wp,
                         uint8 srp )
{
  uint8 i;
  uint8 *srcPtr  = srcBasePtr + srp;
  uint8 *destPtr = destBasePtr + srp;

  // ALT: COULD USE DMA.
  if( wp >= srp )
  {
    for(i=srp; i<wp; i++)
    {
      *destPtr++ = *srcPtr++;
    }
  }
  else // wrap around case
  {
    for(i=srp; i<PHY_FIFO_SIZE; i++)
    {
      *destPtr++ = *srcPtr++;
    }

    // now we need to copy the wrapped around part of the FIFO so reset both
    // pointers to the beginning of their respective banks
    srcPtr  = srcBasePtr;
    destPtr = destBasePtr;

    for(i=0; i<wp; i++)
    {
      *destPtr++ = *srcPtr++;
    }
  }
}


/*******************************************************************************
 * @fn          phyGetFreeBank
 *
 * @brief       Returns the bank that is currently not being used for backup.
 *              Also removes it from the list of available banks.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The bank ID, 1-5. A zero value means no bank is available.
 */
static uint8 phyGetFreeBank( void )
{
  uint8 i = 0;
  do
  {
    if( phyFreeBanks & (1 << ++i) )
    {
      // remove it from the free pool
      phyFreeBanks &= ~(1 << i);
      return i;
    }
  } while( i<6 );

 return( 0 );
}


/*******************************************************************************
 * @fn          phySaveFifo
 *
 * @brief       Saves TX/RX FIFO for the specified connection. The backup bank
 *              where this FIFO is to be stored is assigned before calling this
 *              function.
 *
 * input parameters
 *
 * @param       fifoType - PHY_TXFIFO, PHY_RXFIFO
 * @param       connId   - ID of connection whose FIFO is to be restored.
 * @param       bankId   - The bank to be stored to.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
static void phySaveFifo( uint8 fifoType,
                         uint8 connId,
                         uint8 bankId )
{
  uint8 wp;
  uint8 srp;
  uint8 *srcPtr;
  uint8 *destPtr = (uint8 *)PHY_DATA_BANK_PTR;
  uint8 currentConfig;

  // save the FIFO params for this connection
  if ( fifoType == PHY_TXFIFO )
  {
    connInfo[connId].txInfo.wp      = wp  = RFTXFWP;
    connInfo[connId].txInfo.swp     = RFTXFSWP;
    connInfo[connId].txInfo.rp      = RFTXFRP;
    connInfo[connId].txInfo.srp     = srp = RFTXFSRP;
    connInfo[connId].txInfo.bankId  = bankId;
    srcPtr = (uint8 *)PHY_TX_FIFO_PTR;
  }
  else // PHY_RXFIFO
  {
    connInfo[connId].rxInfo.wp      = wp  = RFRXFWP;
    connInfo[connId].rxInfo.swp     = RFRXFSWP;
    connInfo[connId].rxInfo.rp      = RFRXFRP;
    connInfo[connId].rxInfo.srp     = srp = RFRXFSRP;
    connInfo[connId].rxInfo.bankId  = bankId;
    srcPtr = (uint8 *)PHY_RX_FIFO_PTR;
  }

  // save the current configuration
  currentConfig = RFRAMCFG;

  // map in the bank where the FIFO data will be stored
  RFRAMCFG = (currentConfig & 0xF8) | bankId;

  // copy the FIFO data to the backup bank
  phyCopyFIFO(destPtr, srcPtr, wp, srp);

  // restore the original configuration
  RFRAMCFG = currentConfig;
}


/*******************************************************************************
 * @fn          phyRestoreFifo
 *
 * @brief       Restores TX or RX FIFO of the specified connection. This is
 *              called from within the PHY module. When the FIFO is restored,
 *              the backup bank that it was occupying is freed up to be used by
 *              other connections. This can also be called on the connection
 *              that does not exist in the backup pool. In that case, the FIFO
 *              parameters will be set to their reset values.
 *
 * input parameters
 *
 * @param       fifoType - PHY_TXFIFO, PHY_RXFIFO
 * @param       connId   - ID of connection whose FIFO is to be restored.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
static void phyRestoreFifo( uint8 fifoType,
                            uint8 connId )
{
  uint8 wp;
  uint8 srp;
  uint8 bankId;
  uint8 *destPtr;
  uint8 *srcPtr = (uint8 *)PHY_DATA_BANK_PTR;
  uint8 currentConfig;

  // restore the FIFO params for this connection
  if ( fifoType == PHY_TXFIFO )
  {
    RFTXFWP  = wp   = connInfo[connId].txInfo.wp;
    RFTXFSWP        = connInfo[connId].txInfo.swp;
    RFTXFRP         = connInfo[connId].txInfo.rp;
    RFTXFSRP = srp  = connInfo[connId].txInfo.srp;
    bankId          = connInfo[connId].txInfo.bankId;
    destPtr         = (uint8 *)PHY_TX_FIFO_PTR;

    // Set the FIFO params to reset values. When a restore operation is done
    // on a connection that did not exist in the backup, the FIFO will be
    // set up with reset conditions.
    connInfo[connId].txInfo.wp      = 0;
    connInfo[connId].txInfo.swp     = 0;
    connInfo[connId].txInfo.rp      = 0;
    connInfo[connId].txInfo.srp     = 0;
    connInfo[connId].txInfo.bankId  = 0;
  }
  else // PHY_RXFIFO
  {
    RFRXFWP  = wp   = connInfo[connId].rxInfo.wp;
    RFRXFSWP        = connInfo[connId].rxInfo.swp;
    RFRXFRP         = connInfo[connId].rxInfo.rp;
    RFRXFSRP = srp  = connInfo[connId].rxInfo.srp;
    bankId          = connInfo[connId].rxInfo.bankId;
    destPtr         = (uint8 *)PHY_RX_FIFO_PTR;

    // Set the FIFO params to reset values. When a restore operation is done
    // on a connection that did not exist in the backup, the FIFO will be
    // set up with reset conditions.
    connInfo[connId].rxInfo.wp      = 0;
    connInfo[connId].rxInfo.swp     = 0;
    connInfo[connId].rxInfo.rp      = 0;
    connInfo[connId].rxInfo.srp     = 0;
    connInfo[connId].rxInfo.bankId  = 0;
  }

  // bankId will be 0 when this is called on a connection that did not exist
  // in the backup bank. In that there, there is no FIFO data to copy.
  if ( bankId != 0 )
  {
    // save the current configuration
    currentConfig = RFRAMCFG;

    // map in the bank where the FIFO data is stored
    RFRAMCFG = (currentConfig & 0xF8) | bankId;

    // copy the FIFO data from the backup bank to the FIFO
    phyCopyFIFO( destPtr, srcPtr, wp, srp );

    // restore the original configuration
    RFRAMCFG = currentConfig;

    // add this bank to the free bank pool
    phyFreeBanks |= (1 << bankId);
  }
}


/*******************************************************************************
 */

