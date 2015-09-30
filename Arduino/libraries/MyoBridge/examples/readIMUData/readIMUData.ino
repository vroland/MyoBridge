#include <MyoBridge.h>
#include <SoftwareSerial.h>

//SoftwareSerial connection to MyoBridge
SoftwareSerial bridgeSerial(2,3);

//initialize MyoBridge object with software serial connection
MyoBridge bridge(bridgeSerial);

//declare a function to handle IMU data
void handleIMUData(MyoIMUData& data) {
  
  //Accelerometer data is scaled from 0 to 2048. But just print it out for the Moment:
  Serial.print(data.accelerometer[0]);
  Serial.print(" ");
  Serial.print(data.accelerometer[1]);
  Serial.print(" ");
  Serial.println(data.accelerometer[2]);
}

void setup() {
  //initialize both serial connections
  Serial.begin(115200);
  bridgeSerial.begin(115200);

  //wait until MyoBridge has found Myo and is connected. Make sure Myo is not connected to anything else and not in standby!
  Serial.println("Searching for Myo...");
  bridge.begin();
  Serial.println("connected!");

  //set the function that handles the IMU data
  bridge.setIMUDataCallBack(handleIMUData);
  //tell the Myo we just want the IMU data
  bridge.setIMUMode(IMU_MODE_SEND_DATA);
  //disable sleep mode, so we get continous data even when not synced 
  bridge.disableSleep();
}

void loop() {
  //update the connection to MyoBridge
  bridge.update();
}