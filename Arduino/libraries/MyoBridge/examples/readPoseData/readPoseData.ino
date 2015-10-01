/**
 * @file   readPoseData.ino
 * @author Valentin Roland (webmaster at vroland.de)
 * @date   September-October 2015
 * @brief  Example file demonstrating the pose data reading.
 *
 * This file is part of the MyoBridge Project. For more information, visit https://github.com/vroland/MyoBridge.
 * 
 * @include readPoseData.ino
 */
 
 
#include <MyoBridge.h>
#include <SoftwareSerial.h>

//SoftwareSerial connection to MyoBridge
SoftwareSerial bridgeSerial(2,3);

//initialize MyoBridge object with software serial connection
MyoBridge bridge(bridgeSerial);

//declare a function to handle pose data
void handlePoseData(MyoPoseData& data) {
  
  //convert pose data to MyoPose
  MyoPose pose;
  pose = (MyoPose)data.pose;

  //print the pose
  Serial.println(bridge.poseToString(pose));
}

void setup() {
  //initialize both serial connections
  Serial.begin(115200);
  bridgeSerial.begin(115200);

  //wait until MyoBridge has found Myo and is connected. Make sure Myo is not connected to anything else and not in standby!
  Serial.println("Searching for Myo...");
  bridge.begin();
  Serial.println("connected!");

  //set the function that handles pose events
  bridge.setPoseEventCallBack(handlePoseData);
  //tell the Myo we want Pose data
  bridge.enablePoseData();
  //make sure Myo is unlocked
  bridge.unlockMyo();
  
  //You have to perform the sync gesture to receive Pose data!
}

void loop() {
  //update the connection to MyoBridge
  bridge.update();
}