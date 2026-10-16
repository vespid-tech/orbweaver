## Table of Contents
- [Introduction](#introduction)
- [Configuration with Windows](#configuration-with-windows)
- [Configuration with Raspberry Pi](#configuration-with-raspberry-pi)
- [Help, further configuration, programming guide, etc.](#help-further-configuration-programming-guide-etc)

# Introduction

# Configuration with Windows

## What you'll need
1) Orbweaver
2) desktop or laptop computer running Windows
3) USB cable
4) antenna

## Loading Firmware
1) Download the latest Orbweaver binary: [Orbweaver binary](https://github.com/vespid-tech/orbweaver/releases/download/orbweaver-v0.0.1/orbweaver_v0.0.1.d810f2a.bin).
2) Download the latest Orbweaver configurator: [Orbweaver configurator](https://github.com/vespid-tech/orbweaver/releases/download/configurator-v0.0.1/orbweaver_configurator_v0.0.1.exe)
3) Run the Configurator. You should see the following user interface:

<p align="center">
  <img src="./images/configurator.jpg"
       alt="Configurator GUI">
</p>

4) **Connect the Orbweaver to the PC via a USB cable**. PWR LED should illuminate indicating Orbwaever is powered. LEDs 3, 2, and 1, should cycle on/off in that order indicating the bootloader is active.
5) In the Configurator next to Com port: dropdown, **press the Refresh button**. The dropdown should populate the comport dropdown Orbweaver is using. **Make sure the comport used by Orbweaver is selected**.
6) **Click the Browse button** and browse to the Orbweaver binary downloaded in step 1.
7) Click **"Load Binary"**. Loading should complete after several seconds. Once loading is complete, a load complete dialog box should pop up. **Press OK** on the Load Complete dialog box. LED 3 should now be flashing, and LEDs 1 and 2 should be off indicating the application is active. The binary is now loaded.
9) If you wish to configure Orbweaver proceed to the Configuration section of this document.

## Configuration
1) In the Configurator, assign an address to the Orbweaver **enter an address in the Address (1-254): box**. We recommend using address 1 as the first Orbweaver address in your network. The second Orbweaver should be address 2, and so on. **Do not use the same address as that displayed in  Source Address (1-254):**, which defaults to 254.
2) **Press "Set New Address"**.
3) **Select the Frequency Band** you plan to operate Orbweaver in. In North and South America, use ITU region 2. In Europe, Africa, and West Asia, use ITU region 1.
4) **Press "Set Frequency Band"**.
5) Press "Get Configuration" and ensure the Output matches the settings you selected above.
6) Orbweaver is now configured and ready for use.
7) If you wish to send a test message to verify Orbweaver is functional, proceed to the Test section of this document.

## Test
1) **Connect antennas** to two different Orbweavers.
2) **Connect one Orbweaver to your PC via USB**. 
3) In the Configurator next to Com port: dropdown, **press the Refresh button**. The dropdown should populate the comport dropdown Orbweaver is using. **Make sure the comport used by Orbweaver is selected**.
4) **Apply power to a second Orbweaver**. It may be connected to the same PC if no other source of power is available. **The two Orbweavers must have different addresses, and neither address should match the addressed displayed in Source Address (1-254):**, which defaults to 254.
5) **Using Radio Address (1-255): Select the address of the FIRST Orbweaver**.
6) **Press "Test Radio"**. A dialog should pop up indicating whether the test passed or failed.
7) **Using Radio Address (1-255): Select the address of the SECOND Orbweaver**.
8) **Press "Test Radio"**. A dialog should pop up indicating whether the test passed or failed. If both tests passed, both your Orbweavers are capable of bi-direciton communication.

# Configuration with Raspberry Pi

## What you'll need
1) Orbweaver
2) Raspberry Pi 5 running Linux
3) antenna

## Loading Firmware

## Configuration

## Test

# Help, further configuration, programming guide, etc.

For troubleshooting communication issues refer to the following document:  [Orbweaver Troubleshooting](https://github.com/vespid-tech/orbweaver/blob/main/documentation/orbweaver_troubleshooting.md)

For all other questions, concerns, etc., refer to the Orbweaver GitHub Q&A discussion:  [Orbweaver Q&A](https://github.com/vespid-tech/orbweaver/discussions/categories/q-a)

For more information on how to use Orbweaver (including configuration options not covered in this guide), refer to the following document: [Orbweaver User Manual](https://github.com/vespid-tech/orbweaver/blob/main/documentation/orbweaver_user_manual.md)

For information on writing communication software and integrating Orbweaver into your software application, refer to the following document: [Orbweaver Programming Guide](https://github.com/vespid-tech/orbweaver/blob/main/documentation/orbweaver_programming_guide.md)