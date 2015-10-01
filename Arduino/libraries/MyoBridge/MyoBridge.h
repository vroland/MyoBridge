/**
 * @file   MyoBridge.h
 * @author Valentin Roland (webmaster at vroland.de)
 * @date   September-October 2015
 * @brief  Header declaring the MyoBridge class with all necessary constants and types.
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


#ifndef MyoBridge_h
#define MyoBridge_h

#include "Arduino.h"
#include "myohw.h"
#include "MyoBridgeTypes.h"

#define RX_BUFFER_SIZE 128
///Size of the packet buffer. 23 BLE mesage bytes plus some serial overhead.
#define PACKET_BUFFER_SIZE 32
///Size of the result buffer. 23 BLE mesage bytes plus some serial overhead.
#define RESULT_BUFFER_SIZE 32

/// Possible function returns
typedef enum {
	SUCCESS = 0,
	ERR_RX_BUFFER_OVERFLOW = 1,
	ERR_INVALID_PACKET = 2,
	ERR_NO_PACKET = 3,
} MyoBridgeSignal;

/// Supported poses.
typedef enum {
    MYO_POSE_REST             = myohw_pose_rest,
    MYO_POSE_FIST             = myohw_pose_fist,
    MYO_POSE_WAVE_IN          = myohw_pose_wave_in,
    MYO_POSE_WAVE_OUT         = myohw_pose_wave_out,
    MYO_POSE_FINGERS_SPREAD   = myohw_pose_fingers_spread,
    MYO_POSE_DOUBLE_TAP       = myohw_pose_double_tap,
    MYO_POSE_UNKNOWN          = myohw_pose_unknown
} MyoPose;

/// Supported IMU modes.
typedef enum {
	IMU_MODE_NONE 			= myohw_imu_mode_none, 			///< Do not send IMU data or events.
	IMU_MODE_SEND_DATA 		= myohw_imu_mode_send_data, 	///< Send IMU data streams (accelerometer, gyroscope, and orientation).
	IMU_MODE_SEND_EVENTS 	= myohw_imu_mode_send_events, 	///< Send motion events detected by the IMU (e.g. taps).
	IMU_MODE_SEND_ALL 		= myohw_imu_mode_send_all, 		///< Send both IMU data streams and motion events.
	IMU_MODE_SEND_RAW 		= myohw_imu_mode_send_raw, 		///< Send raw IMU data streams.
} MyoIMUMode;

/// Supported EMG modes.
typedef enum {
	EMG_MODE_NONE			= myohw_emg_mode_none, 			///< Do not send EMG data.
    EMG_MODE_SEND			= myohw_emg_mode_send_emg, 		///< Send filtered EMG data.
    EMG_MODE_RAW 			= myohw_emg_mode_send_emg_raw, 	///< Send raw (unfiltered) EMG data.
} MyoEMGMode;

/// Supported IMU motion events
typedef enum {
	IMU_MOTION_TAP			= myohw_motion_event_tap,		///< IMU Tap Motion event
} MyoIMUMotion;

typedef myohw_imu_data_t MyoIMUData;
typedef myohw_classifier_event_t MyoPoseData;

/// Connection States
typedef enum {
	CONN_STATUS_UNKNOWN        = MYB_STATUS_UNKNOWN,			///< Status of the Bluetooth module is unknown.
	CONN_STATUS_INIT           = MYB_STATUS_INIT,      			///< The Bluetooth module is initializing.
	CONN_STATUS_SCANNING       = MYB_STATUS_SCANNING,      		///< The Bluetooth module is scanning for the Myo.
	CONN_STATUS_CONNECTING     = MYB_STATUS_CONNECTING,     	///< The Bluetooth module is connecting to the Myo.
	CONN_STATUS_DISCOVERING    = MYB_STATUS_DISCOVERING,    	///< The Bluetooth module is discovering the Myo's Bluetooth LE profile.
	CONN_STATUS_BRIDGE_SETUP   = MYB_STATUS_DISCOVERING + 1,	///< MyoBridge sets up connection parameters and retrieves information.
	CONN_STATUS_READY		   = MYB_STATUS_DISCOVERING + 2,	///< MyoBridge is ready to use!
	CONN_STATUS_LOST		   = MYB_STATUS_DISCOVERING + 3,	///< MyoBridge lost the connection. You can't use this connection any more, unless you call begin() again.
} MyoConnectionStatus;

class MyoBridge {
  public:
    
	/**
	 * @defgroup basic_funcs Functions for Basic Usage
	 * These functions represent the basic functionality of the MyoBridge library
	 * and are designed to be easy to use.
	 * @{
	 */
	
	/**
	 * @brief Create a MyoBridge object. This object stores all data needed and provices functions
	 * to control and access the data provided my the MyoArmband.
	 *
	 * @param serialConnection The serial connection to use. Can be software serial.
	 */
	MyoBridge(Stream& serialConnection);
	
	/**
	 * @brief Wait for the MyoBridge to connect to the MyoArmband and retrieve initial data.
	 * @param conConnectionEvent [optional] Callback function to deliver verbose information during during the connection process.
	 */
	MyoBridgeSignal begin(void (*conConnectionEvent)(MyoConnectionStatus) = NULL);
	
	/**
	 * @brief Update the communication with the MyoBridge module. Call as often as possible! The loop function is a appropiate position.
	 * This function checks the serial connection for new data, and keeps the library up to date.
	 * You can only receive data if you call this function frequently. If you dont, the receive buffer of the serial
	 * connection might overflow and cause serious issues.
	 */
	MyoBridgeSignal update();
	
	/**
	 * @brief Get the major number of the firmware version.
	 */
	short getFirmwareMajor();
	
	/**
	 * @brief Get the minor number of the firmware version.
	 */
	short getFirmwareMinor();
	
	/**
	 * @brief Get the patch number of the firmware version.
	 */
	short getFirmwarePatch();
	
	/**
	 * @brief Get the hardware revision of the myo armband.
	 */
	short getHardwareRevision();
	 
	/**
	 * @brief Get the hardware (MAC) address of the myo. 
	 * @param array_of_six_bytes The hardware address will be stored in a 6-Byte long array.
	 */
	void getHardwareAddress(byte array_of_six_bytes[6]);
	
	/**
	 * @brief The pose used for unlocking.
	 */
	MyoPose getUnlockPose();
	
	
	
	/**
	 * @brief sets the IMU data mode.
	 */
	void setIMUMode(MyoIMUMode mode);
	
	/**
	 * @brief sets the EMG data mode.
	 */
	void setEMGMode(MyoEMGMode mode);
	
	/**
	 * @brief enables pose data.
	 */
	void enablePoseData();
	
	/**
	 * @brief enables pose data.
	 */
	void disablePoseData();
	
	/**
	 * @brief unlocks the myo so pose data can be sent.
	 */
	void unlockMyo();
	
	/**
	 * @brief locks the myo. No pose data can be sent.
	 */
	void lockMyo();
	
	/**
	 * @brief disables sleep mode. This prevents Myo from entering sleep mode when connected.
	 */
	void disableSleep();
	
	/**
	 * @brief enables sleep mode. Myo goes to leep if not synced and not moving.
	 */
	void enableSleep();
	
	/// @}

	/**
	 * @defgroup callback_funcs Event Callback Functions
	 * These functions set the so called callback functions for specific events. If you do not
	 * know what a callback function does, read about that first (http://playground.arduino.cc/Main/MIDILibraryCallbacks).
	 * They are the functions called, when a specific event happens, like data arriving from the Armband.
	 * Dont let your program spend too much time in these functions, because sensor data might arrive at a
	 * fast pace. If you need some time, consider disabling data messages by calling disableAllMessages() first.
	 * @{
	 */
	
	/**
	 * Callback function for IMU data, packed in the MyoIMUData data type.
	 */
	void setIMUDataCallBack(void (*onIMUDataEvent)(MyoIMUData&));
	/**
	 * Callback function for IMU motion data, packed in the MyoIMUMotion data type.
	 */
    void setIMUMotionCallBack(void (*onIMUMotionEvent)(MyoIMUMotion));
	/**
	 * Callback function for Pose data, packed in the MyoPoseData data type.
	 */
	void setPoseEventCallBack(void (*onPoseEvent)(MyoPoseData&));
	/**
	 * Callback function for EMG data, as array of 8-Bit signed numbers.
	 */
	void setEMGDataCallBack(void (*onEMGDataEvent)(int8_t[8]));
	/// @}
	
	
	/**
	 * @defgroup advanced_funcs Functions for Advanced Usage
	 * These functions represent advanced functionality of the MyoBridge library
	 * and should only be used by users with knowledge about the Myo Bluetooth Protocol
	 * and the MyoBridge firmware. Use the MyoBridgeTypes.h header for command types,
	 * Enumerations and constants you might want to use and also consider the MyoBridge
	 * and Myo Bluetooth Protocol documentations.
	 * @{
	 */
	 
	/**
	 * @brief send a command as defined in MyoBridgeTypes.h to the MyoBridge. Not recommended. Use doComfirmedWrite or doPersistentRead instead.
	 */
	MyoBridgeSignal sendCommand(uint8_t* pData);
	
	/**
	 * Sends a command and waits for write confirmation, retrys if necessary.
	 */
	MyoBridgeSignal doConfirmedWrite(uint8_t* pData);
	
	/**
	 * Sends a read command and waits for data response, retrys if necessary.
	 */
	MyoBridgeSignal doPersistentRead(uint8_t* command);
	 
	/**
	 * Enables BLE Notifications and Indications for all relevant characteristics.
	 * This includes IMU Data, IMU Events, Classifier Events, and EMG Data.
	 */
	MyoBridgeSignal enableAllMessages();
	
	/**
	 * Disables BLE Notifications and Indications for all relevant characteristics.
	 * This includes IMU Data, IMU Events, Classifier Events, and EMG Data.
	 */
	MyoBridgeSignal disableAllMessages();
	
	/// @}
	
	/**
	 * @defgroup utility_funcs Utility Functions
	 * These functions are utility functions to help the user perform common, simple tasks.
	 * @{
	 */
	
	/**
	 * returns a string corresponding to the given pose constant.
	 */
	const char* poseToString(MyoPose pose);
	
	/**
	 * returns a string corresponding to the given MyoConnectionStatus constant.
	 */
	const char* connectionStatusToString(MyoConnectionStatus status);
	
	/// @}
	
  private:
	///The stream to read myo data from.
	Stream* myo;
	
    uint8_t myobridge_status;
    uint8_t serialBuffer[RX_BUFFER_SIZE];
    uint16_t buffer_offset;
	///buffer for storing packet data.
	uint8_t packet_buffer[PACKET_BUFFER_SIZE];
	///length of the current serial packet.
	uint8_t packet_length;
	///did a new serial packet arrive?
	bool new_packet_available;
	
	///buffer for storing and delivering the extracted message data.
	uint8_t result_buffer[RESULT_BUFFER_SIZE];
	///length of result data;
	uint8_t result_length;
	///is a result for the current packet available?
	bool result_available;
	
	///storage for the firmware version, which is read in begin().
	myohw_fw_version_t firmware_info;
	///storage for the myo info, which is read in begin(). It contains serial number (Address) and configuration info.
	myohw_fw_info_t myo_info;
	///storage for the current IMU, EMG and Pose modes
	myohw_command_set_mode_t myo_mode;
	
	///indicates whether a write process is complete
	bool write_complete;
	
	///Is the MyoBridge connected with the Myo and ready to take commands?
	bool connected;
	///the more verbose connection status
	MyoConnectionStatus connection_status;

    //callback functions
    void (*onIMUDataEvent)(MyoIMUData&);
    void (*onIMUMotionEvent)(MyoIMUMotion);
	void (*onPoseEvent)(MyoPoseData&);
	void (*onEMGDataEvent)(int8_t[8]);
	/**
	 * Callback function to deliver verbose information during during the connection process;
	 */
	void (*conConnectionEvent)(MyoConnectionStatus);
	
    //utility
    void eraseBuffer(uint16_t bytes);
    static void pollMyoBridgeStatus();
    MyoBridgeSignal processPackage();
	MyoBridgeSignal receiveSerial();
	void updateMode();
	
	/**
	 * Sets the CCCB status for BLE Notifications and Indications for all relevant characteristics.
	 * This includes IMU Data, IMU Events, Classifier Events, and EMG Data.
	 */
	MyoBridgeSignal setAllMessagesStatus(uint16_t cccb_status);
    
};

#endif //MyoBridge_h