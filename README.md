# MyoBridge

A high-level Arduino library and custom firmware for the HM-11 (CC2541 SoC) to enable direct Myo Armband and Arduino communication.
Please visit the [wiki](https://github.com/vroland/MyoBridge/wiki) for more information.

# Concept of MyoBridge

![MyoBridge Concept](https://raw.githubusercontent.com/wiki/vroland/MyoBridge/images/wiki/mbconcept.png)

The Myo Gesture Control Armband features gesture recognition, motion tracking and muscular activity measurement. 
This data is very useful in various applications, since sensor data and events are sent wirelessly through a Bluetooth Low Energy
connection to a host device and no complex wiring is required. Especially embedded systems can profit from the portability 
of the device, like the Arduino platform. Unfortunately, most BLE hardware has a very limited feature set, none of the
available BLE modules could directly connect to the Myo Armband out of the box. 

But this can be solved by developing a custom firmware for these modules, which then provides access to the Myo data
and delivers it to another device, like an Arduino. The **MyoBridge Firmware** fits exactly this purpose. 
It is a custom firmware for the HM-11 BLE Module, which uses the Texas Instruments CC2541 Bluetooth LE System on a Chip. 

When the MyoBridge is connected to a Myo Gesture Control Armband, the Arduino can send commands or receive
sensor data from the Myo using the MyoBridge library. The communication between HM-11/MyoBridge and Arduino uses
a simple two-wire serial connection, while the HM-11 module connects directly to the Myo Armband.

**When using the Arduino library, you can easily read data from the Myo, without the need of any intermediate
PC or phone.**

You want to use the MyoBridge Firmware? [Get Started!](Getting Started with MyoBridge Firmware)
