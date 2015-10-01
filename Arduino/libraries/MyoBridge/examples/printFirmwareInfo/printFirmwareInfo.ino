#include <MyoBridge.h>
#include <SoftwareSerial.h>

//SoftwareSerial connection to MyoBridge
SoftwareSerial bridgeSerial(2,3);

//initialize MyoBridge object with software serial connection
MyoBridge bridge(bridgeSerial);

//a function to inform us about the current state and the progess of the connection to the Myo.
void printConnectionStatus(MyoConnectionStatus status) {
	
	//print the status constant as string
	Serial.println(bridge.connectionStatusToString(status));
}

void setup() {
  //initialize both serial connections
  Serial.begin(115200);
  bridgeSerial.begin(115200);

  //wait until MyoBridge has found Myo and is connected. Make sure Myo is not connected to anything else and not in standby!
  Serial.println("Searching for Myo...");
  
  //initiate the connection with the status callback function
  bridge.begin(printConnectionStatus);
  
  Serial.println("connected!");

  Serial.print("Firmware Version: ");
  Serial.print(bridge.getFirmwareMajor());
  Serial.print(".");
  Serial.print(bridge.getFirmwareMinor());
  Serial.print(".");
  Serial.println(bridge.getFirmwarePatch());
  Serial.print("Hardware Revision: ");
  Serial.println(bridge.getHardwareRevision());

  //declare a storage array for the hardware address
  byte address[6];

  //get the address and store it in our array
  bridge.getHardwareAddress(address);

  //print the hardware address in HEX format
  Serial.print("Hardware Address: 0x");
  for (int i=0;i<6;i++) {
    Serial.print(address[i], HEX);
  }
  Serial.println();

  //get the unlock pose
  MyoPose unlockPose;
  unlockPose = bridge.getUnlockPose();

  //print the name of the unlock pose as string
  Serial.print("Unlock Pose: ");
  Serial.println(bridge.poseToString(unlockPose));
}

void loop() {
  //update the connection to MyoBridge
  bridge.update();
}
