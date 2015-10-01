/**
 * @file   MyoBridge.cpp
 * @author Valentin Roland (webmaster at vroland.de)
 * @date   September-October 2015
 * @brief  Implementation file of the MyoBridge class.
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


#include "MyoBridge.h"
#include "MyoBridgeTypes.h"

/// Supported poses.
const char* myo_pose_strings[] {
    "MYO_POSE_REST",
    "MYO_POSE_FIST",
    "MYO_POSE_WAVE_IN",
    "MYO_POSE_WAVE_OUT",
    "MYO_POSE_FINGERS_SPREAD",
    "MYO_POSE_DOUBLE_TAP",
};

/// Connection States
const char* myo_connection_state_strings[] {
	"CONN_STATUS_UNKNOWN",
	"CONN_STATUS_INIT",
	"CONN_STATUS_SCANNING",
	"CONN_STATUS_CONNECTING",
	"CONN_STATUS_DISCOVERING",
	"CONN_STATUS_BRIDGE_SETUP",
	"CONN_STATUS_READY",
	"CONN_STATUS_LOST",
};

MyoBridge::MyoBridge(Stream& serialConnection) {
	myo = &serialConnection;
	
	this->onIMUDataEvent = NULL;
	this->onIMUMotionEvent = NULL;
	this->onPoseEvent = NULL;
	this->onEMGDataEvent = NULL;
	
	connected = false;
	connection_status = CONN_STATUS_UNKNOWN;
	
	new_packet_available = false;
	result_available = false;
	write_complete = false;
	
	
	myo_mode.header.command = myohw_command_set_mode;
	myo_mode.header.payload_size = 3;
	myo_mode.emg_mode = myohw_emg_mode_none;
	myo_mode.imu_mode = myohw_imu_mode_none;
	myo_mode.classifier_mode = myohw_classifier_mode_disabled;
	
}

MyoBridgeSignal MyoBridge::begin(void (*conConnectionEvent)(MyoConnectionStatus)) {
	
	this->conConnectionEvent = conConnectionEvent;
	
	//begin() function exits only when the MyoBridge is ready to receive commands or a fatal error occurs
	
	MyoBridgeSignal status = SUCCESS;
	
	//request the current status first
	MYBGetStatusCmd_t get_status_cmd;
	get_status_cmd.hdr.cmd = MYB_CMD_GET_STATUS;
	status = sendCommand((uint8_t*) &get_status_cmd);
	
	//wait until the MyoBridge Chip is connected and ready.
	bool done = false;
	
	while (!done) {
		
		//receive and process serial data
		status = update();
		
		//exit on error
		if (status == ERR_RX_BUFFER_OVERFLOW) {
			return ERR_RX_BUFFER_OVERFLOW;
		}		
		
		//exit when MyoBridge is ready
		if (myobridge_status == MYB_STATUS_READY) {
			done = true;
		}
	}
	
	connection_status = CONN_STATUS_BRIDGE_SETUP;
						
	//call verbose callback function if available
	if (conConnectionEvent != NULL) {
		conConnectionEvent(connection_status);
	}
						
	/////////////////// Read the Firmware Version /////////////////////////
	
	MYBReadCmd_t read_fw_cmd;
	read_fw_cmd.hdr.cmd = MYB_CMD_READ_CHAR;
	read_fw_cmd.index = MYO_CHAR_FIRMWARE;
	
	status = doPersistentRead((uint8_t*) &read_fw_cmd);
	memcpy(&firmware_info, &result_buffer[0], result_length);
	
	/////////////////// Read the Firmware Info /////////////////////////
	
	MYBReadCmd_t read_info_cmd;
	read_info_cmd.hdr.cmd = MYB_CMD_READ_CHAR;
	read_info_cmd.index = MYO_CHAR_INFO;
	
	status = doPersistentRead((uint8_t*) &read_info_cmd);
	memcpy(&myo_info, &result_buffer[0], result_length);
	
	///////////////////// Enable All Asynchronous Messages ///////////////////
	
	updateMode();
	enableAllMessages();
	
	connected = true;
	
	connection_status = CONN_STATUS_READY;
						
	//call verbose callback function if available
	if (conConnectionEvent != NULL) {
		conConnectionEvent(connection_status);
	}
	
	return SUCCESS;
}

MyoBridgeSignal MyoBridge::update() {
	
	//receive serial data
	MyoBridgeSignal status = receiveSerial();
	//exit on error
	if (status == ERR_RX_BUFFER_OVERFLOW) {
		return ERR_RX_BUFFER_OVERFLOW;
	}
	
	//process the package data, if package arrived
	status = processPackage();	

	return status;
}

MyoBridgeSignal MyoBridge::processPackage() {
	
	if (!new_packet_available) return ERR_NO_PACKET;
	
	new_packet_available = false;
	
	//switch between package types
	switch (packet_buffer[0]) {
		
		case MYB_RSP_STATUS: {
			MYBStatusRsp_t* pRsp = (MYBStatusRsp_t*)&packet_buffer[0];
			
			//Is status valid?
			if (pRsp->status <= MYB_STATUS_BUSY) {
				uint8_t old_status = myobridge_status;
				myobridge_status = pRsp->status;
				
				//status changed?
				if (old_status != myobridge_status) {
					
					//is equivalent to MyoBridge system status until connected
					if (myobridge_status < MYB_STATUS_READY) {
						
						//when connecting initially
						if (!connected) {
							connection_status = (MyoConnectionStatus)myobridge_status;
							
							//call verbose callback function if available
							if (conConnectionEvent != NULL) {
								conConnectionEvent(connection_status);
							}
						} else {
							connection_status = CONN_STATUS_LOST;
							
							//call verbose callback function if available
							if (conConnectionEvent != NULL) {
								conConnectionEvent(connection_status);
							}
						}
					}
				}
				
			} else {
				return ERR_INVALID_PACKET;
			}
			
			break;
		}
		
		case MYB_RSP_VALUE: {
			MYBDataRsp_t* pRsp = (MYBDataRsp_t*)&packet_buffer[0];
			
			//store result data
			uint8_t* data = &packet_buffer[sizeof (MYBDataRsp_t) - sizeof(uint8_t*)];
			memcpy(&result_buffer[0], data, packet_length - 2 * sizeof(uint8_t));
			result_length = packet_length - 3 * sizeof(uint8_t);	
			//announce available result
			result_available = true;
			
			//is setup complete? this could be notification/indication data
			if (connected) {
				if ((pRsp->characteristic == MYO_CHAR_IMU_DATA) && (onIMUDataEvent != NULL)) {
					MyoIMUData* imu_data = (MyoIMUData*)&result_buffer[0];
					onIMUDataEvent(*imu_data);
				}
				if ((pRsp->characteristic == MYO_CHAR_IMU_EVENTS) && (onIMUMotionEvent != NULL)) {
					onIMUMotionEvent((MyoIMUMotion)result_buffer[0]);
				}
				if ((pRsp->characteristic == MYO_CHAR_CLASSIFIER_EVENTS) && (onPoseEvent != NULL)) {
					MyoPoseData* pose_data = (MyoPoseData*)&result_buffer[0];
					
					onPoseEvent(*pose_data);
				}
				if ((pRsp->characteristic >= MYO_CHAR_EMG_0) && (pRsp->characteristic <= MYO_CHAR_EMG_3) && (onEMGDataEvent != NULL)) {
					int8_t* emg_data = (int8_t*)&result_buffer[0];
					onEMGDataEvent(emg_data);
				}
			}
			
			break;
		}
		
		case MYB_RSP_PING: {
			
			//ping responses indicate a finished write process
			write_complete = true;
			break;
		}
	}
	
	return SUCCESS;
}

/**
 * Sends a command and waits for write confirmation, retrys if necessary.
 */
MyoBridgeSignal MyoBridge::doConfirmedWrite(uint8_t* pData) {
	
	
	bool done = false;
	while (!done) {
		
		sendCommand(pData);
		//wait for the write response
		
		//time it which the command was issued
		long time_cmd_started = millis();
		
		//wait for answer or resend timeout
		while (!done && (millis() - time_cmd_started < MYB_RETRY_TIMEOUT)) {
			
			//receive serial data
			MyoBridgeSignal status = receiveSerial();
			
			//exit on error
			if (status == ERR_RX_BUFFER_OVERFLOW) {
				return ERR_RX_BUFFER_OVERFLOW;
			}
			
			//process the package data, if package arrived
			status = processPackage();			
			
			//exit when MyoBridge is ready
			if ((write_complete) && (myobridge_status == MYB_STATUS_READY)) {
				done = true;
			}
		}
	}
	
	return SUCCESS;
}

/**
 * Sends a read command and waits for data response, retrys if necessary.
 */
MyoBridgeSignal MyoBridge::doPersistentRead(uint8_t* command) {
	
	
	bool done = false;
	while (!done) {
		
		sendCommand(command);
		//wait for the write response
		
		long time_cmd_started = millis();
		
		//wait for answer or resend timeout
		while (!done && (millis() - time_cmd_started < MYB_RETRY_TIMEOUT)) {
			
			//receive serial data
			MyoBridgeSignal status = receiveSerial();
			
			//exit on error
			if (status == ERR_RX_BUFFER_OVERFLOW) {
				return ERR_RX_BUFFER_OVERFLOW;
			}
			
			//process the package data, if package arrived
			status = processPackage();			
			
			//this is done when a result is available
			if (result_available) {
				
				//data is now available in result buffer
				done = true;
			}
		}
	}
	
	return SUCCESS;
}


MyoBridgeSignal MyoBridge::sendCommand(uint8_t* pData) {
  uint8_t type = pData[0];
  myo->write('P');
  switch (type) {
    case MYB_CMD_PING: {
      myo->write(sizeof(MYBPingCmd_t));
      myo->write(pData, sizeof(MYBPingCmd_t));
      break;
    }
    case MYB_CMD_WRITE_CHAR: {
      MYBWriteCmd_t* cmd = (MYBWriteCmd_t*)pData;  
      myo->write(sizeof(MYBWriteCmd_t) - sizeof(uint8_t*) + cmd->len);
      myo->write(cmd->hdr.cmd);
      myo->write(cmd->index);
      myo->write(cmd->len);
      myo->write(cmd->pData, cmd->len);
	  
	  write_complete = false;
      break;   
    }
    case MYB_CMD_READ_CHAR: {
      MYBReadCmd_t* cmd = (MYBReadCmd_t*)pData;  
      myo->write(sizeof(MYBReadCmd_t));
      myo->write(pData, sizeof(MYBReadCmd_t));
      break;   
    }
    case MYB_CMD_SET_ASYNC_STATUS: {
      MYBAsyncStatusCmd_t* cmd = (MYBAsyncStatusCmd_t*)pData;  
      myo->write(sizeof(MYBAsyncStatusCmd_t));
      myo->write(pData, sizeof(MYBAsyncStatusCmd_t));
	  
	  write_complete = false;
      break;   
    }
    case MYB_CMD_GET_STATUS: {
      MYBGetStatusCmd_t* cmd = (MYBGetStatusCmd_t*)pData;  
      myo->write(sizeof(MYBGetStatusCmd_t));
      myo->write(pData, sizeof(MYBGetStatusCmd_t));
      break;   
    }
  }
  myo->write('E');
  
  return SUCCESS;
}


MyoBridgeSignal MyoBridge::receiveSerial() {
	
  while (myo->available()) {
    char recv = myo->read();
    //store received byte in serial buffer
    serialBuffer[buffer_offset] = recv;
    buffer_offset++;
  }
  
  if ( buffer_offset> 0) {
    short start_index = -1;
    short end_index = -1;
    
    short i;
    //search for packet start
    for (i=0;i<buffer_offset;i++) {
      char c = serialBuffer[i];
      //potential packet start
      if ((c=='P') && (i<buffer_offset-1)) {
        uint8_t blen = serialBuffer[i+1];
        uint16_t temp_end = i+blen+2;
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
		packet_length = end_index - start_index - 2;
		memcpy(&packet_buffer[0], &serialBuffer[start_index + 2], end_index - start_index - 2);
		eraseBuffer(end_index + 1);
		
		
		//announce the new packet
		new_packet_available = true;
		//there cannot be a result available for this packet
		result_available = false;
		return SUCCESS;
    }

    if (buffer_offset > RX_BUFFER_SIZE) {
		return ERR_RX_BUFFER_OVERFLOW;
    }
	return ERR_NO_PACKET;
  }
}

//erase a number of bytes from the RX buffer
void MyoBridge::eraseBuffer(uint16_t bytes) {
	memcpy(&serialBuffer[0], &serialBuffer[bytes], RX_BUFFER_SIZE - bytes);
	buffer_offset -= bytes;
}

void MyoBridge::updateMode() {
	MYBWriteCmd_t cmd;
	cmd.hdr.cmd = MYB_CMD_WRITE_CHAR;
    cmd.index = MYO_CHAR_COMMAND;
    cmd.len = sizeof(myohw_command_set_mode_t);
    cmd.pData = (uint8_t*)&myo_mode;
    
	doConfirmedWrite((uint8_t*)&cmd);
}

/**
 * Sets the CCCB status for BLE Notifications and Indications for all relevant characteristics.
 * This includes IMU Data, IMU Events, Classifier Events, and EMG Data.
 */
MyoBridgeSignal MyoBridge::setAllMessagesStatus(uint16_t cccb_status) {
	
	//we need a bitmask to decide whether we want to set notification or indication byte. setting both causes problems with data streams.
	//1 means set indication flag, 0 means set notification flag. starting at MYO_CHAR_IMU_DATA with LSB.
	uint8_t notification_indication_bitmask = 0b00000110;
	
	const int NUM_RELEVANT_CHARS = 7;
	uint8_t relevant_characteristics[NUM_RELEVANT_CHARS] = {
		MYO_CHAR_IMU_DATA,
		MYO_CHAR_IMU_EVENTS,
		MYO_CHAR_CLASSIFIER_EVENTS,
		MYO_CHAR_EMG_0,
		MYO_CHAR_EMG_1,
		MYO_CHAR_EMG_2,
		MYO_CHAR_EMG_3,
	};
	
	MyoBridgeSignal status;
	
	for (int i=0;i<NUM_RELEVANT_CHARS;i++) {
		
		//request the current status first
		MYBAsyncStatusCmd_t cmd;
		cmd.hdr.cmd = MYB_CMD_SET_ASYNC_STATUS;
		cmd.index = relevant_characteristics[i];
		
		//enable indications or notifications based on bitmask, when cccb is 3, set 0 otherwise.
		cmd.status = cccb_status & 1 << (bool)(notification_indication_bitmask & (1 << i));
		
		doConfirmedWrite((uint8_t*)&cmd);
	}
	
	return SUCCESS;
}
/**
 * Enables BLE Notifications and Indications for all relevant characteristics.
 * This includes IMU Data, IMU Events, Classifier Events, and EMG Data.
 */
MyoBridgeSignal MyoBridge::enableAllMessages() {
	return setAllMessagesStatus(0x0003);
}

/**
 * Disables BLE Notifications and Indications for all relevant characteristics.
 * This includes IMU Data, IMU Events, Classifier Events, and EMG Data.
 */
MyoBridgeSignal MyoBridge::disableAllMessages() {
	return setAllMessagesStatus(0x0000);
}

/**
 * @brief unlocks the myo so pose data can be sent.
 */
void MyoBridge::unlockMyo() {
	myohw_command_unlock_t unlock_cmd;
	unlock_cmd.header.command = myohw_command_unlock;
	unlock_cmd.header.payload_size = 1;
	unlock_cmd.type = myohw_unlock_hold;
	
	MYBWriteCmd_t cmd;
	cmd.hdr.cmd = MYB_CMD_WRITE_CHAR;
    cmd.index = MYO_CHAR_COMMAND;
    cmd.len = sizeof(myohw_command_unlock_t);
    cmd.pData = (uint8_t*)&unlock_cmd;
	
	doConfirmedWrite((uint8_t*)&cmd);
}

/**
 * @brief locks the myo. No pose data can be sent.
 */
void MyoBridge::lockMyo() {
	myohw_command_unlock_t unlock_cmd;
	unlock_cmd.header.command = myohw_command_unlock;
	unlock_cmd.header.payload_size = 1;
	unlock_cmd.type = myohw_unlock_lock;
	
	MYBWriteCmd_t cmd;
	cmd.hdr.cmd = MYB_CMD_WRITE_CHAR;
    cmd.index = MYO_CHAR_COMMAND;
    cmd.len = sizeof(myohw_command_unlock_t);
    cmd.pData = (uint8_t*)&unlock_cmd;
	
	doConfirmedWrite((uint8_t*)&cmd);
}

/**
 * @brief disables sleep mode. This prevents Myo from entering sleep mode when connected.
 */
void MyoBridge::disableSleep() {
	myohw_command_set_sleep_mode_t sleep_mode_cmd;
	sleep_mode_cmd.header.command = myohw_command_set_sleep_mode;
	sleep_mode_cmd.header.payload_size = 1;
	sleep_mode_cmd.sleep_mode = myohw_sleep_mode_never_sleep;
	
	MYBWriteCmd_t cmd;
	cmd.hdr.cmd = MYB_CMD_WRITE_CHAR;
    cmd.index = MYO_CHAR_COMMAND;
    cmd.len = sizeof(myohw_command_set_sleep_mode_t);
    cmd.pData = (uint8_t*)&sleep_mode_cmd;
	
	doConfirmedWrite((uint8_t*)&cmd);
}

/**
 * @brief enables sleep mode
 */
void MyoBridge::enableSleep() {
	myohw_command_set_sleep_mode_t sleep_mode_cmd;
	sleep_mode_cmd.header.command = myohw_command_set_sleep_mode;
	sleep_mode_cmd.header.payload_size = 1;
	sleep_mode_cmd.sleep_mode = myohw_sleep_mode_normal;
	
	MYBWriteCmd_t cmd;
	cmd.hdr.cmd = MYB_CMD_WRITE_CHAR;
    cmd.index = MYO_CHAR_COMMAND;
    cmd.len = sizeof(myohw_command_set_sleep_mode_t);
    cmd.pData = (uint8_t*)&sleep_mode_cmd;
	
	doConfirmedWrite((uint8_t*)&cmd);
}
	

/**
 * Callback function for IMU data, packed in the MyoIMUData data type.
 */
void MyoBridge::setIMUDataCallBack(void (*onIMUDataEvent)(MyoIMUData&)) {
	this->onIMUDataEvent = onIMUDataEvent;
}

/**
 * Callback function for IMU motion data, packed in the MyoIMUMotion data type.
 */
void MyoBridge::setIMUMotionCallBack(void (*onIMUMotionEvent)(MyoIMUMotion)) {
	this->onIMUMotionEvent = onIMUMotionEvent;
}

/**
 * Callback function for Pose data, packed in the MyoPoseData data type.
 */
void MyoBridge::setPoseEventCallBack(void (*onPoseEvent)(MyoPoseData&)) {
	this->onPoseEvent = onPoseEvent;
}

/**
 * Callback function for EMG data, as array of 8-Bit signed numbers.
 */
void MyoBridge::setEMGDataCallBack(void (*onEMGDataEvent)(int8_t[8])) {
	this->onEMGDataEvent = onEMGDataEvent;
}

/**
 * @brief Get the major number of the firmware version.
 */
short MyoBridge::getFirmwareMajor() {
	return firmware_info.major;
}

/**
 * @brief Get the minor number of the firmware version.
 */
short MyoBridge::getFirmwareMinor() {
	return firmware_info.minor;
}

/**
 * @brief Get the patch number of the firmware version.
 */
short MyoBridge::getFirmwarePatch() {
	return firmware_info.patch;
}

/**
 * @brief Get the hardware revision of the myo armband.
 */
short MyoBridge::getHardwareRevision() {
	return firmware_info.hardware_rev;
}
	
/**
 * @brief Get the hardware (MAC) address of the myo. 
 * @param array_of_six_bytes The hardware address will be stored in a 6-Byte long array.
 */
void MyoBridge::getHardwareAddress(byte array_of_six_bytes[6]) {
	memcpy(&array_of_six_bytes[0], &myo_info.serial_number, 6);
}

/**
 * @brief The pose used for unlocking.
 */
MyoPose MyoBridge::getUnlockPose() {
	return (MyoPose)myo_info.unlock_pose;
}



/**
 * @brief sets the IMU data mode.
 */
void MyoBridge::setIMUMode(MyoIMUMode mode) {
	myo_mode.imu_mode = (uint8_t)mode;
	updateMode();
}

/**
 * @brief sets the EMG data mode.
 */
void MyoBridge::setEMGMode(MyoEMGMode mode) {
	myo_mode.emg_mode = (uint8_t)mode;
	updateMode();
}

/**
 * @brief enables pose data.
 */
void MyoBridge::enablePoseData() {
	myo_mode.classifier_mode = myohw_classifier_mode_enabled;
	updateMode();
}

/**
 * @brief enables pose data.
 */
void MyoBridge::disablePoseData() {
	myo_mode.classifier_mode = myohw_classifier_mode_disabled;
	updateMode();
}
	
	


/**
 * returns a string corresponding to the given pose constant.
 */
const char* MyoBridge::poseToString(MyoPose pose) {
	
	if (pose == MYO_POSE_UNKNOWN) {
		return "MYO_POSE_UNKNOWN";
	} else {
		return myo_pose_strings[pose];
	}
}

/**
 * returns a string corresponding to the given MyoConnectionStatus constant.
 */
const char* MyoBridge::connectionStatusToString(MyoConnectionStatus status) {
	return myo_connection_state_strings[status];
}
	