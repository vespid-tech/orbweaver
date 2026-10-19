## Table of Contents
- [Introduction](#introduction)
- [Configuration with Windows](#configuration-with-windows)
- [Configuration with Raspberry Pi](#configuration-with-raspberry-pi)
- [Help, Further Documentation, etc.](#help-further-documentation-etc)
- [Disclaimer, Licenses, and Legal Notice](#disclaimer-licenses-and-legal-notice)

# Introduction

# Configuration with Windows

## What you'll need
1) Orbweaver
2) desktop or laptop computer running Windows
3) USB cable
4) antenna (if you want to want to test the radio)

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
3) antenna (if you want to want to test the radio)

## Loading Firmware
1) **With power off, mount the Orbweaver on the Raspberry Pi**.
2) **Power on the Raspberry Pi.** Orbweaver LEDs 3, 2, 1 should start cycling in that order indicating the firmware bootloader is active.
3) **Download the latest Orbweaver binary to the Raspberry Pi**: [Orbweaver binary](https://github.com/vespid-tech/orbweaver/releases/download/orbweaver-v0.0.1/orbweaver_v0.0.1.d810f2a.bin).
4) **Clone or downloader the Orbweaver repo**: [Orbweaver GitHub](https://github.com/vespid-tech/orbweaver)
5) **Navigate to the /orbweaver/software/Python/config folder**.
6) **Run the load_via_bl_spi.py script**. Use the path to the firmware binary downloaded in step 3. For instance:

$ python load_via_bl_spi.py -f [path to application binary] 

If Orbweaver is connected via USB, use the load_via_bl_usb.py script, and provide a comport as well as a file path as follows:

$ python load_via_bl_usb.py -p [comport] -f [path to application binary] 

7) If you wish to configure Orbweaver proceed to the Configuration section of this document.

## Configuration and Test
1) **Navigate to the /orbweaver/software/Python/config folder** if you aren't already there.
6) **Run the ow_config.py script**.  
The ow_config has the same options and functionality as the desktop Configurator, so please refer to the Desktop Configuration section in this document for configuration and test instructions.

# Help, Further Documentation, etc.
Additional documentation can be found on our GitHub in the documentation folder: [Orbweaver GitHub](https://github.com/vespid-tech/orbweaver).

For additional help, create a post on the Discussion forum: [Orbweaver Forum](https://github.com/vespid-tech/orbweaver/discussions).

# Disclaimer, Licenses, and Legal Notice
Orbweaver is distributed under the following open source licenses: 

Hardware license: CERN Open Hardware Licence Version 2 – Strongly Reciprocal (CERN-OHL-S-2.0)
Software and firmware license: GNU General Public License v3.0 (GPL-3.0-only)
Documentation: Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)

For the full licenses see our GitHub: [Orbweaver GitHub](https://github.com/vespid-tech/orbweaver)

"Vespid," "Orbweaver," and associated logos are trademarks of Vespid.

Orbweaver is provided “as is,” without warranty of any kind, express or implied. In no event shall Orbweaver developers or contributors be liable for any claim, damages, or other liability arising from its use. Orbweaver is intended for evaluation and development use only. It is not supplied as a finished product and is not certified for use in any product made for sale to the general public. The purchaser is responsible for ensuring that the final configuration (including but not limited to firmware configuration, transmission power, antenna selection, transmission frequency, etc.), complies with applicable regulatory requirements.
