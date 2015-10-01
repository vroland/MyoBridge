/**
 * @file   readEMGData.ino
 * @author Valentin Roland (webmaster at vroland.de)
 * @date   September-October 2015
 * @brief  Example file demonstrating the EMG data streaming feature.
 *
 * This file is part of the MyoBridge Project. For more information, visit https://github.com/vroland/MyoBridge.
 *
 * @include readEMGData.ino
 */
 
#include <MyoBridge.h>
#include <SoftwareSerial.h>

//SoftwareSerial connection to MyoBridge
SoftwareSerial bridgeSerial(2,3);

//initialize MyoBridge object with software serial connection
MyoBridge bridge(bridgeSerial);

//declare a function to handle EMG data
void handleEMGData(int8_t data[8]) {
  
  //the EMG data is 8bit signed integers. Just print them out:
  Serial.print(data[0]);
  Serial.print(" ");
  Serial.print(data[1]);
  Serial.print(" ");
  Serial.print(data[2]);
  Serial.print(" ");
  Serial.print(data[3]);
  Serial.print(" ");
  Serial.print(data[4]);
  Serial.print(" ");
  Serial.print(data[5]);
  Serial.print(" ");
  Serial.print(data[6]);
  Serial.print(" ");
  Serial.print(data[7]);
  Serial.println(" ");
}

void setup() {
  //initialize both serial connections
  Serial.begin(115200);
  bridgeSerial.begin(115200);

  //wait until MyoBridge has found Myo and is connected. Make sure Myo is not connected to anything else and not in standby!
  Serial.println("Searching for Myo...");
  bridge.begin();
  Serial.println("connected!");

  //set the function that handles EMG data
  bridge.setEMGDataCallBack(handleEMGData);
  //tell the Myo we want the filtered EMG data
  bridge.setEMGMode(EMG_MODE_SEND);
  //disable sleep mode, so we get continous data even when not synced 
  bridge.disableSleep();
}

void loop() {
  //update the connection to MyoBridge
  bridge.update();
}