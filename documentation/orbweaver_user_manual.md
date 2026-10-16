# Table of Contents
- [Introduction](#introduction)
- [Orbweaver Networks Explained](#orbweaver-networks-explained)
- [Help, programming guide, etc.](#help-programming-guide-etc)

# Introduction
Orbweaver is an open source HAT+ designed to mount on a Raspberry Pi 5. It is based around a Texas Instruments CC1310. It allows user to set-up secure wireless networks using desktops, laptops, Amdroid-based mobile devices, and Raspberry Pis without relying on existing networks. Orbweaver comes equipped with a debug header, so end users may write their own firmware or use Orbweaver as an alternative to a Texas Instruments CC1310 Eval Kit when evaluating the CC1310.

The Orbweaver much easier to use than HATs based around transcievers like the CC1101. Much of the communication complexity is embedded in the CC1310 firmware, which keeps the software lightweight and easy to understand.

### Comparison to Other Common Networking Technology
The nearest networking technology to Orbweaver are the various LoRa-based technologies like Meshtastic. Both operate in the Sub-GHz ISM band, but Orbweaver supports higher bitrates than LoRa, so applications like audio are possible with Orbweaver. 

Compared to LoRa, Orbweaver is much more customizable at runtime, so the user can easily fit Orbweaver to their particular application. Bitrate, frequency hopping, encryption, and operating frequency are all configurable at runtime.

Orbweaver is fully open source and utilizes standard modulation methods, so the firmware can theoretically be ported to radio SoCs from different manufacturers which are capable of interoperating. LoRa modulation is proprietary, and the transceivers are only available from Semtech. Semtech LoRa modules tend to be more expensive than CC1310s and other Sub-GHz devices.

Compared to WiFi, Orbweaver is capable of communicating much farther, is better at pentrating barriers, and consumes less power. However, the maximum over-the-air bitrate supported by Orbweaver is 500 kbps, which is much lower than WiFi, and too low for streaming video of any reasonable quality.

# Power
There are two ways Orbweaver may be powered. If it is mounted on a Raspberry Pi 5 via the 2x20 pin connector, it will be powered via the Raspberry Pi's +5 VDC rail. If Orbweaver is not mounted, it may be powered via USB. If Orbweaver is mounted and USB is connected, USB power will be used. The USB port is an upstream facing port only. This means that it will always consume power and can't be used as a power source for other USB devices.

Any modern cell phone power adapter (or similar power source) should be capable of powering Orbweaver. However, a power supply designed for use with a Raspberry Pi is recommended, especially if the Raspberry Pi and Orbweaver are being powered at the same time. Orbweaver may also be powered via a USB connector on a Desktop, Laptop, or Android mobile device. We can't vouch for every device in the world, but most modern devices should be capable of powering Orbweaver. A portable cell phone charger should also work. 

Generally, Orbweaver may be safely powered and unpowered with no special shutdown proceedure. That is, you can just plug it in and unplug it without risk. However, the following two exceptions exist:

One, when mounting Orbweaver on a Raspberry Pi, make sure both Orbweaver and the Raspberry Pi are unpowered. Orbweaver is designed to resist damage from stray current spikes on the the 2x20 pin connector (up to a point), so it will likely survive, but the Raspberry Pi may or may not.

Two, during loading and configuration, the Orbweaver onboard flash memory is written. Losing power during these operations may result in corrupting the flash. While this might result in a loss of configuration or even a loss of the current application firmware, it should not result in anything unrecoverable. That is, it should not be possible to brick the device even if power is removed at the worst possible time, but you might have to reload the firmware and reconfigure the device. Within 20-40 milliseconds or so of the device being loaded and/or configured, power can safely be removed at any time with no risk of memory corruption.

# Orbweaver Interfaces
Orbweaver contains 5 connectors (one of which is unpopulated), 4 LEDs and two buttons.

### USB Port
The Orbweaver USB port utilizes a USB to serial bridge. This means Orbweaver is recognized by the host device as a serial device not a USB device. This also means the host device must have a compatible serial driver installed. Any modern Windows, Linux, and Android device should already have a compatible driver. See the Orbweaver Programming Guide for more on serial communication.

Orbweaver VID and PID are:

VID: 0x1209  
PID: 0x7800

### 2x20 Pin Header Socket
The 2x20 pin header connector allows Orbweaver to be mounted on a Raspberry Pi 5. Once mounted, no USB connection is necessary. Orbweaver will be powered via the Raspberry Pi +5 VDC rail. Orbweaver can communicate with the Raspberry Pi via SPI. See the Orbweaver Programming Guide for more on SPI communication.

### 2x5 Pin Header Pins
The 2x5 pin header connector allows an XDS200 debug probe to be connected to Orbweaver. This allows the CC1310 to be programmed and debugged using JTAG. Note: Programming and debugging Orbweaver via JTAG may cause any of several adverse results including bricking Orbweaver.

### 1x7 Pin Header Pins
The 1x7 pin header connector is unpopulated. Because XDS200 debug probes tend to be rather expensive, this connector is included so Orbweaver maybe be programmed and debugged using the much cheaper Texas Instruments CC1310 Eval Kit as a debugger. Consult the Orbweaver schematics and CC1310 eval kit documentation for how to use the eval kit to debug Orbweaver. Note: As with the primary JTAG header,programming and debugging Orbweaver in this way may cause any of several adverse results including bricking Orbweaver.

### SMA Connector
The SMA connector is where the antenna must be connected. A through-hole SMA connector (as opposed to a uFL or surface-mount connector) was selected as it will resist wearing out even if various antennas are removed and replaced many times over the life of the device. It is also more resistant to damage if the antenna is accidently snagged or bumped. There are many SMA antennas available for sale. See the antenna section of this document for more.

### LEDs
PWR LED will be illuminated whenever power is applied.  
LED 3 will flash at 1 Hz whenever the application firmware is active.  
LED 1 toggles whenever a command is sent or received.  
LEDs 3, 2, and 1 will cycle in that order when ever the firmware bootloader is active.  
If only the PWR LED is illuminated either the ROM bootloader is active or Orbweaver is malfunctioning.  
LEDs 1 and 2 will flash to indicate various application update and bootloading operations.

### Buttons
Pressing and releasing buttons 1 and 2 switches the states of LEDs 1 and 2 respective.
Pressing and holding both buttons for 3.1 seconds activates the firmware bootloader. Cycle power or reset Orbweaver to active the application again.

# Programming, Bootloading, and OTA updates
Orbweaver contains two bootloaders: A firmware bootloader and a ROM bootloader. It also contains two areas of flash: Program flash and external flash.

### Firmware Bootloader
The purpose of the firmware bootloader is to load an application into Orbweaver program memory either because there isn't one, or the current one needs to be updated. This bootloader can only communicate via USB or SPI. It cannot use the radio, so the initial application loading must be performed via USB or SPI. Orbweaver ships with the firmware bootloader already programmed, but without application firmware. So when a fresh from the factory Orbweaver is initially powered on, this bootloader should be active. Once the application firmware is loaded, the user should never have to interact directly with this bootloader (see the Orbweaver Quick Start Guide for instruction on loading and configuring Orbweaver). Orbweavers may be updated over-the-air using the radio once they have an active application.

All firmware updating via the firmware bootloader or application are fail-safe. Even if power is removed at the worst possible time during an update, Orbweaver should be able to recover and resume normal operation once power is restored.

If a user wants to load an application on an Orbweaver that contains an application that is not compatible with the Configurator that is being used, both buttons should be held for 1.3 seconds to activate the Orbweaver's firmware bootloader. The firmware bootloader can then be used to update the incompatible application.

### ROM Bootloader
The ROM bootloader exists as a fallback to the firmware bootloader. The ROM bootloader may be activated in two ways: One, if Orbweaver is mounted on a Raspberry Pi, the boot pin can be held low while the reset pin is pulled low then release. Two, the boot throughholes can be shorted together while power is cycled. Once the ROM bootloader is active, only PWR LED should be illuminated.

The ROM bootloader can be used to update the firmware bootloader (e.g. if Orbweaver program flash has become corrupted). Note: If done improperly, loading firmware with the ROM bootloader may result in adverse outcomes up to and including bricking Orbweaver.

### JTAG
The two JTAG connectors are explained in the Orbweaver Interfaces section of this document. They may be used along with an IDE for developing and debugging firmware. They may also be used as a last resort to recover corrupted devices if the ROM bootloader fails to do so.

### Program Flash
Program flash is physically located inside the CC1310. It is where the firmware bootloader, current application, and current application metadata reside. The firmware bootloader, ROM bootloader, and JTAG programmers can write to program flash. The application cannot write to program flash.

### External Flash
External flash is physically located on Orbweaver, but it is seperate from the CC1310. External flash is used to store Orbweaver configuration, a backup application binary, application metadata, and a bootloader command. The firmware bootloader and application can write to external flash. The ROM bootloader and JTAG programmers cannot write to external flash. The back-up application stored in external flash allows Orbweaver to updates to be fail-safe.

# Orbweaver Configurator
The Configurator is used to configure Orbweaver and update Orbweaver application and bootloader firmware. A desktop version may be downloaded from the Orbweaver GitHub repo: [Orbweaver Releases](https://github.com/vespid-tech/orbweaver/releases). A Python CLI version can be found in the repo: [config](https://github.com/vespid-tech/orbweaver/tree/main/software/Python/config). After cloning the repo run the script "ow_config.py" to run the CLI version. Both versions contain the same functionality.

The following shows the desktop version:
<p align="center">
  <img src="./images/configurator.jpg"
       alt="Configurator GUI">
</p>


# Orbweaver Networks Explained
Peer-to-Peer, Mobile, Ad Hoc, Hybrid, Mesh Network
## Peer-to-Peer Mesh Networks

## Orbweaver Network Topology
### Node
### Subnet
### Network
### Addresses

## Messaging 
### Forwarding and TTL

# Frequency Bands

# Encryption and Security
### Key Storage

# Frequency Hopping

# Bandwidth, Bitrate, and Data Throughput

# Radio Range and How to Optimize It

## Fresnel Zone
## Operating Enviroment
## Bitrate
## Frequency
## Antenna
## Bi-Directional Amplifier
## Real-World Test Results

# Mounting on a Raspberry Pi and Case Options

# Help, Programming guide, etc.

# Disclaimer and Legal Notice