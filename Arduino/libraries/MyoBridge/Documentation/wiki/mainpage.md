# Getting Started

## Instaling the MyoBridge Arduino Library

The MyoBridge library can be installed like any other Arduino library. Just copy the "MyoBridge" folder in your library directory in your sketchbook.

## Hardware Setup

Set up your MyoBridge module as described in the MyoBridge Firmware wiki. For use with the Arduino, you just need 4 pins:

 * VCC The power supply for the Bluetooth module. Be sure to **supply only 3.3V!**
 * GND The ground of the bluetooth module. Connect it with the Ground of your Arduino.
 * TX  The transmit data pin of the Bluetooth module. Connect it to the RX pin of your serial port.
 * RX  The receive data pin of the Bluetooth module. Only connect it through a **voltage devider** with your Arduino!
 
The library examples use a software serial connection with this setup:

![](../images/simple_hw_setup_bb.png)

## Load an Example Sketch

Restart your Arduino IDE and go to File->Examples->MyoBridge and open the printFirmwareInfo example.
Upload it to your Arduino and open a serial monitor.
You should now see a message saying:

`Searching for Myo...`

Now make sure your Myo is not connected to your computer any more. Either end all Myo Connect processes, or just remove the dongle.
When the led bar at the bottom of the main segment of your myo lights blue, Myo is connected. Now the MyoBridge needs some time to
discover the Bluetooth profile of the Myo, this will take several seconds. 

```
connected!
Firmware Version: 1.5.1931
Hardware Revision: 2
Hardware Address: 0xA6918C33A5F4
Unlock Pose: MYO_POSE_DOUBLE_TAP
```

This message should appear on your serial monitor after a few seconds, some information about your Myo.
If not, try to reset the Bluetooth module (just disconnect it from power) and/or your Arduino.

*Congratulations, MyoBridge is now ready to use for your own projects!*

You can also try the other examples delivered with the MyoBridge library.


