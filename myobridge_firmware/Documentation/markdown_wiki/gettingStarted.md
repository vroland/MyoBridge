# Getting Started {#gettingStarted}

![MyoBridge Firmware](../images/mblogo_256.png "MyoBridge Firmware")

This section describes how to use the MyoBridge Firmware with a HM-11 module.
For other devices using a TI CC2541 / CC2540 BLE SoC, pin layouts, etc will
be different, buf the general steps would be similar. If you are not using
an HM-11 BLE Module, you might have to adjust the MyoBridge Firmware to 
your pin setup.

## Flashing the MyoBridge Firmware

Things you need: 

 * HM-11 or other compatible BLE module
 * CC Debugger, but there seem to be implementations of the debug protocol for Arduino around (not tested).
 * Soldering equipment, breadbord is probably useful.
 
### Wiring the HM-11 Module

This image shows the necessary pins you have to have access to in order to use the module:

![HM-11 Wiring](../images/wiki/wiring.jpg)

Also refer to the [HM-11 Datasheet](http://txyz.info/b10n1c/datasheets/hm-11_bluetooth40_en.pdf) or 
nickswalkers page showing the HM-10 pinout [(https://github.com/nickswalker/ble-dev-kit/wiki/HM-10-Pinout)](https://github.com/nickswalker/ble-dev-kit/wiki/HM-10-Pinout).

### Connecting to CC Debugger

Connect the soldered wires to the cc debugger as discribed in the [CC Debugger User's Guide] (http://www.ti.com/lit/ug/swru197h/swru197h.pdf) on page 8.
Here is an image showing this for the HM-10 (image from http://hackersome.com/p/rampadc/cc2540-hidKbdM](http://hackersome.com/p/rampadc/cc2540-hidKbdM) ):

![] (../images/wiki/cc_debug_wire.png)

After a reset the CC Debugger LED should be green. Your can now proceed flashing the firmware.

### Flashing the Firmware

For flashing the firmware your need the [SmartRF Flash Programmer] (http://www.ti.com/tool/flash-programmer). Be sure to use Version 1!
Follow the instructions on how to use SmartRF Flash Programmer [here.](https://github.com/aanon4/BlueBasic/wiki/Getting-started:-Hello-World)
Use the MyoBridge_<your_chip>.hex file. 

If there is no such file for your specific chip or you want to modify the firmware, you have to use [IAR Embedded Workbench for 8051.] (http://supp.iar.com/Download/SW/?item=EW8051-EVAL)
Be sure to use the time-limited evaluation license, since the 8KB limit of the Kickstart version won't be enough for this firmware.

After successfully flashing the firmware, your MyoBridge is ready to use!


## Connecting to Arduino

Now you have to connect the HM-11/MyoBridge Module to your Arduino. You need to connect the following pins:

 * VCC to the 3.3V output of your Arduino
 * GND to the Ground (GND) of your Arduino
 * TX to the RX pin of your Arduino (this example uses the hardware serial, SoftwareSerial with other pins should work as well)
 * RX via a **voltage divider** to the TX pin of your Arduino. The voltage divider protects the HM-11 from the 5V of the Arduino.

The hardware setup should look like this (The HM-11 IC is pinwise equivalent to the back side of the real module, with the antenna facing upwards):

 ![] (../images/wiki/simple_hw_setup.png)
 