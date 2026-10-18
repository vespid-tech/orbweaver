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

Orbweaver is designed to optimize low latency response and ease of use, not power savings. The Orbweaver processor and trasciever are both always active as long as power is applied. RF Transmissions are always performed at maximum power.

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
Orbweaver contains two bootloaders: A firmware bootloader and a ROM bootloader. 

### Firmware Bootloader
The firmware and ROM bootloaders can only communicate via USB or SPI. They cannot use the radio. Orbweaver ships with the firmware bootloader already programmed, but without application firmware. So when a fresh from the factory Orbweaver is initially powered on, this bootloader should be active, and the application must be loaded via USB or SPI. Once the application firmware is loaded, the application can accept over-the-air updates using the radio. See the Orbweaver Quick Start Guide for instruction on loading and configuring Orbweaver.

All firmware updating via the firmware bootloader or application are fail-safe. Even if power is removed at the worst possible time during an update, Orbweaver should be able to recover and resume normal operation once power is restored.

If an Orbweaver is in a state that prevents the application from accepting an update (e.g. the configurator version being used is imcompatible with the active application), the firmware bootloader may be activated by holding both Orbweaver buttons for 1.3 seconds. The firmware bootloader can then be used to update the application.

### ROM Bootloader
The ROM bootloader exists as a fallback to the firmware bootloader. The ROM bootloader may be activated in two ways: One, if Orbweaver is mounted on a Raspberry Pi, the boot pin can be held low while the reset pin is pulled low then released. Two, the PCB boot throughholes can be shorted together while power is cycled. Once the ROM bootloader is active, only the PWR LED should be illuminated.

The ROM bootloader may be used to update the firmware bootloader. Note: If not performed properly, loading firmware with the ROM bootloader may result in adverse outcomes up to and including bricking Orbweaver.

### JTAG
The two JTAG connectors are explained in the Orbweaver Interfaces section of this document. They may be used along with a programmer and IDE for developing and debugging firmware.  They may also be used along with a programmer for erasing and programming the firmware bootloader and application using Texas Instruments SmartRF Programmer. Note: Using the JTAG connector to program and erase Orbweaver may result in adverse outcomes up to and including bricking Orbweaver.

# Orbweaver Configurator
The Configurator is used to configure Orbweaver and update Orbweaver application and bootloader firmware. Orbweaver Configurator is treated as a node on your network, therefore it must be assigned a unique address as described below in the Configurator Address section. A desktop version may be downloaded from the Orbweaver GitHub repo: [Orbweaver Releases](https://github.com/vespid-tech/orbweaver/releases). A Python CLI version can be found in the repo: [config](https://github.com/vespid-tech/orbweaver/tree/main/software/Python/config). Using the CLI version is detailed in the "CLI Updates and Configuration" section of this document. After cloning the repo run the script "ow_config.py" to run the CLI version. Both versions contain the same functionality.

The following shows the desktop version:
<p align="center">
  <img src="./images/configurator.jpg"
       alt="Configurator GUI">
</p>

### Configurator Address
This is the address used by Orbweaver Configurator during update operations.

### Radio Address
This is the address of the target Orbweaver that is being updated or configured. If the target Orbweaver is directly connected via USB, 255 may be used regardless of the Orbweavers actual address. Any Orbweaver may be used to send updates over-the-air to remote Orbweavers on the same network. In that case, use the address of the remote Orbweaver. The USB connected Orbweaver will relay messages back and forth between the remote Orbweaver and the Configurator.

### COM port
The comport used to communicate to Orbweaver. Pressing refresh will cause the Configurator to search for connected Orbweavers and populate the dropdown control with the results of the search.

## Load
The controls in this section are used to load new application firmware into Orbweaver. they may also be used to load a new firmware bootloader using the ROM bootloader.

### Load Mode
Select Bootloader if the target Orbweaver has the bootloader active. Select Application if the target Orbweaver has an active application. Select ROM bootloader if the ROM bootloader is active. Note: over-the-air updates can only be sent to remote Orbweavers that have the application active. Orbweavers connected via USB may be updates with any of the three methods as necessary.

### Binary File Path
The path of the binary to load into the target Orbweaver.

Pressing "Load Binary" starts the loading process.

## Configuration
An Orbweaver will only respond to the controls in this section if an application is running on the Orbweaver. If either bootloader is active, the target Orbweaver will not respond to any of these controls.

### Address
The address to assign to the target Orbweaver. Pressing "Set New Address" sets the address. Note: If "Orbweaver Address" is set to the Orbweaver address instead of 255, and a new address is assigned, the Orbweaver will stop responding until "Radio Address" is set to the new address.

### Bitrate
The bitrate to assign to the target Orbweaver. All Orbweaver on the same network must have the same bitrate. Lower bitrates will generally allow for longer range communication but less bandwidth and vice versa. Pressing "Set Bitrate" sets the radio bitrate.

### Cipher Key
This is the key used to encrypt and decrypt messages sent via RF when encryption is enabled. All Orbweavers on the same network must use the same Cipher key. There is no command to retrieve a cipher key from an Orbweaver. It is up to the user to save the cipher key, and make sure the same key is loaded into every Orbweaver. Pressing "Set Cipher Key" will load the key, but it will not be used until encryption is enabled. Pressing the "Encryption Enabled/Encryption Disabled" button will toggle encryption.

### Frequency Band
In order to comply to local regulations, this should be set to reflect the ITU band the Orbweaver network will operate in. ITU Region 1 encompasses Europe, Africa, and West Asia. ITU Region 2 encompasses North and South America. For all other regions, select a band and Default Channel that is appropriate for the region. See the "Disclaimer and Legal Notice" section of this document for more.

"Available Channels" displays the channels available in the selected region, and "Default Channel" selects the channel that the Orbweaver will use by default (i.e. until it is changed by software or frequency hopping is enabled). The default channel frequency is displayed next to the "Default Channel." Pressing "Set Frequency Band" Sets the frequency band and default channel.

### Get Configuration
Retrieves the current Configuration.

### Test Orbweaver
Sends a test message to the radio. If a response is received it means radio communication is nominally functional. It does not perform a comprehensive system test. Test Radio will only work if the target Orbweaver has an application active.

### Reset Orbweaver
Sends a reset command to the target Orbweaver. After an Orbweaver is loaded and configured, it may be reset. As a final check, the configuration may be retrieved and verified against what was set. If the received configuration matches what was sent, it indicates the proper firmware was loaded and the configuration was successfully saved in flash memeory.

# CLI Updates and Configuration

# Orbweaver Networks Explained
Orbweavers are designed to allow anyone to set-up a peer-to-peer, ad Hoc, mobile, hybrid, mesh network. Here's what those term mean:

**peer-to-peer**: There are no designated clients or servers. Every node can request and provide data.   

**ad hoc**: No dedicated routers or switches exist. Any node may communicate with any other node in range regardless of how the nodes are arranged.  

**mobile**: The positions of the nodes are not static. Nodes may move relative to each other or all together while continuing to communicate.

**hybrid**: The individual nodes on a network may interact using different communication mediums. In the case of Orbweaver networks, they may use RF, USB, or SPI.

**mesh**: Every node is also a router. Each node in the network is capable of routing messages to nodes that would otherwise be unreachable.

### Orbweaver Network Examples
**Remote Sensing**: A desktop computer is connected to Ethernet. A Raspberry Pi is connecting data that needs to be sent to the desktop computer. WiFi and Cellular cannot be used (e.g. the Raspberry Pi is too far away, does not have a cellular module, etc.). If an Orbweaver is connected to both the Raspberry Pi and desktop, the two can send and receive data via the Orbweaver. If they are so far away that not even the Orbweavers can communicate, as many Orbweavers as necessary can be placed between the two in order to establish a network.

**Secure Private Network**: A number of people in a given area would like to communicate via their Android mobile device, but they are unwilling or unable to use a cellular network and no WiFi is available. Each person may install an app on their mobile device that acts as an Orbweaver node. They can then plug an Orbweaver into each of their cell phones and communicate freely on their own private network. 

**Raspberry Pi Backdoor**: A Raspberry Pi has been deploy in a building, but it has dropped off the WiFi network. A daemon that acts as an Orbweaver node may be installed on the Raspberry Pi that communicates to an attached Orbweaver. Any computer with an Orbweaver can access and troubleshoot the Raspberry Pi without WiFi.

## Orbweaver Network Topology
### Node
A node is any device (real or virtual) that is connected to an Orbweaver network. It may be an Orbweaver, an Android mobile device, a desktop (or software running on a desktop), or anything else equiped with a compatible USB serial port driver or capable of communicating via SPI that can send and receive properly formated messages.
### Subnet
A subnet consists of an Orbweaver, and all nodes connected to the Orbweaver via USB or SPI. Only one node can be physically connected via SPI one one via USB, but each of those devices may have up to 5 additional nodes connected to (or running on) them that the Orbweaver can route message to and from, so a single subnet may contain up to 13 nodes.
### Network
All of the nodes on all of the subnets capable of communicating with each other. An Orbweaver can only be a part of a single network. However, other computers (like desktops and Raspberry Pis) may be connected to more than one network at a time. For instance, if a Raspberry Pi has an Orbweaver mounted and a second connected via USB. The two Orbweaver can be node on two different networks. The Raspberry Pi can communicate to both. In this way networks may be chained together allowing the address space to be extended beyond 254 nodes.
### Addresses
Every node on a network must have a unique address. There are 255 addresses available (1-255). Address 0 is invalid, and cannot be used by any node. Address 255 is the broadcast address. Messages sent to this address will be received and processed by any node that receives it. All other numbers (1-254) may be used as node addresses. By convention, Orbweavers start with address 1 and count up. All non-Orbweaver nodes start at address 254 and count down. For instance, if you have a network consisting of 3 Orbweavers, a Raspberry Pi, and a desktop running a communication script and the Orbweaver Configurator. The Orbweavers may use addresses 1, 2, and 3. The Raspberry Pi may use address 254. The desktop communication script may use address 253, and the Orbweaver Configurator that is also running on the desktop may use address 252.

## Messaging 
All Orbweaver network communication is packet based. That is, all messages sent between nodes must be sent as properly formated packets called "system commands." For more on messaging see the Orbweaver Programming Guide.

# Frequency Bands
Orbeaver radios operate in one of two unlicensed ISM bands. ITU region 1 (Europe, Africa, and West Asian) has 15 channels ranging from 863 MHz to 870 MHz. ITU Region 2 (North and South America) has 53 channels ranging from 902 MHz to 928 MHz. Many if not most other regions of the world have unlicense bands that overlap with the frequencies used by ITU region 1 or 2, so a default channel from those bands may be used. If frequency hopping is enabled, the Orbweavers will hop through all the available channels in the select ITU region in random order.

See the Disclaimer and Legal Notice section of this document for more on this topic.

# Encryption and Security
Orbweaver uses AES-128 encryption for radio messages. Cracking a message encrypted with AES-128 with a properly randomized cipher key is considered infeasible with modern computer technology. It may be possible to crack AES-128 encrypted messages in the future using quantum computer (if quantum compters are ever successfully developed). Orbweaver encryption is performed with true random numbers. Authorization tags and nonces are used to prevent tampering of clear text routing and addressing data as well as to precent replay attacks.

Wired communication via USB and SPI is never encrypted. This means nodes that are not Orbweavers do not need cipher keys or encryption algorythms. This also means if an encryption key is lost, an Orbweaver can still be communicated with via USB or SPI (e.g. in order to turn off encryption or configure a new key).

Every Orbweaver on an encrypted network must have the same cipher key configured. The cipher key is stored on Orbweaver in plain text. There is no native command that can retrieve the cipher key. However, a moderately saphisticated attacker that gains physical access to an Orbweaver is capable of retrieving the key by writing and loading custom firmware on Orbweaver. So if an Orbweaver is physically lost or compromised, its cipher key should be considered compromised.

# Frequency Hopping
In order to reduce latency and increase bandwidth, Orbweaver uses a unique frequency hopping algorithm. When frequency hopping in enabled, every Orbweaver in a network will randomly hop through every channel available based on the ITU region selected. Each channel is dwelled on for 400 ms. However, if a message is in the process of being received when a channel hop is due, the channel hop will be delayed until the entire message is received. This helps reduce latency by allowing Orbweavers to send long messages as soon as possible even if the network is about to hop to the next frequency. 

### Frequency Hopping Pitfalls
Before frequency hopping is enabled, every Orbweaver in a network must have the same channel map saved. Any Orbweaver may share a channel map with every Orbweaver on the network. Any Orbweaver that does not have the same channel map will not be able to maintain synchronization with the other Orbweavers in the network. When an Orbweaver initiates frequency hopping, a synchronization command is broadcast. Any Orbweaver that fails to receive this command will not begin hopping. However, synchronization commands are sent every 5-10 seconds. Eventually the unsynchronized radio should receive a synchronization command on the channel it is current tuned to. Assuming it possesses the same channel map as the other nodes, it will then synchronize and begin hopping. However, since synchronization timing is random, it is theoretically possbile (though highly unlikely) that synchronization will never happen. See the Orbweaver Programming Guide for more information about how to initiate frequency hopping.

In order to stay synchronized, synchronization commands are broadcast on the network every 5 to 10 seconds. Any Orbweaver might broadcast the synchronization command. Receipt of a synchronization command causes an Orbweaver to synchronize their hop clock and reset their synchronization command send timer. This means that which Orbweaver actually sends the synchronization command is random, so it is possible (though highly unlikely) that any given Orbweaver might never send a synchronization command. Therefore, if a network exists where two or more clusters of Orbweavers are only connected by a single Orbweaver, the clusters will stay synchronized internally, but if the one Orbweaver that bridges both clusters never gets a chance to send a synchronization command, the clusters' timing might drift away from each other until the various cluster are unable to communicate with other clusters even though they can still communicate with other members of their own cluster.

# Bandwidth, Bitrate, and Data Throughput
Orbweaver is capable of communicating at maximum over-the-air (OTA) bitrate of 500 kbps. 

Radio data is packetized. Along with every packet of data, the following is sent by the radio:

4 bytes: preamble  
4 bytes: sync word  
2 bytes: packet length  
2 bytes: CRC  

If encryption is used, the following data is added to each raadio packet:

13 bytes: nonce  
16 bytes: authorization tag

Orbweaver messages are always transmitted as system commands. In addition to the data being sent each system command includes the following header data:

1 byte: sequence number  
1 byte: source address  
1 byte: destination address  
1 byte: TTL  
1 byte: system command length  
1 byte: system command  

The maximum length of an Orbweaver system command is 255 bytes including the header, which leaves 249 bytes available for payload data. 

If frequency hopping is used, additional bandwidth will be consumed by periodic channel hopping (which takes 3 ms per hop) as well as synchronization messages which are sent or received every 5 to 15 seconds. Every packet sent and received also consumes an additional ~250 us and up depending on whether encryption is used and how long the packet is as well as other variables.

If we ignore frequency hopping and packet processing overhead, assume maximum system command size for every command sent, and zero packet loss, the following usable data throughput is theoretically possible:

**Encryption disabled data throughput: ~94% of OTA bitrate**  
**Encryption enabled data throughput: ~84% of OTA bitrate**

In practice, some packet loss is unavoidable. Furthermore, the number of packets lost is directly proportional to communication distance and packet length. That is, the longer individual packets are, and the further is the distance between Orbweavers, the more packet loss (and less data throughput) is to be expected. So longer packets don't necessarily lead to higher data throughput.

# Radio Range and How to Optimize It
In order to optimize transmission range, do the following:  
- **Ensure a clear Fresnel zone**  
- **Operate in a low-noise environment (if possible)**
- **Use the lowest possible bitrate**
- **Use the lowest possible frequency**
- **Use a high-quality (and if possible directional) antenna**
- **Use a bi-directional amplifier**

The details of the above suggestions are as follows:

### Fresnel Zone
Radios can communicate farther if they not only have a clear line-of-sight between them but also a clear Fresnel Zone. Anything in the Fresnel zone (like the ground) might interfer with a radio's signal. If you've ever wondered why antennas tend to be placed on towers and rooftops, this is why.

Learn more about Fresnel Zone here: [Fresnel Zone](https://en.wikipedia.org/wiki/Fresnel_zone).

### Operating Enviroment
Environments that contain a lot of RF noise will reduce range. Orbweaver operates in the unlicensed ISM band. Because it is unlicensed lots of other things use it also. Dense urban environments tend to have high background noise levels in the frequency ranges Orbweaver utilizes, which reduces receiver sensitivity. Communication range will tend to be farther in rural (or other electro-magnetically quiet) environments.

### Bitrate
Lower bitrates allow for long range communication. One of the benefits of Orbweaver is that the bitrate is easily configured. In order to extend range, the lowest bitrate that supports a given use case should be used.

### Frequency
Under most circumstance, lower frequencies are capable of longer range communication. It might not make a noticible difference, but 902 MHz might add 2-3% to communication range.

### Antenna
For maximum communication range, a high-quality 50 Ohm antenna designed to operate at the frequency the radio utilizes must be used. For an omni-directional 915 MHz antenna, we recommend something like the following: [ANT-916-OC-LG-SMA](https://www.te.com/en/product-ANT-916-OC-LG-SMA.html) Note: This isn't an endorsement, we have no affiliation with TE Connectivity. Also, this antenna can usually be purchased cheaper from other vendors, and cheaper antennas from other manufacturers are also easily available. Just ensure whatever antenna you select has the followig attribute:

Impedence: 50 Ohms  
Operating Frequency: 915 Mhz (or 866 Mhz for ITU region 1)  
Connector: SMA  

Communication range can be significantly extended (in theory more than doubled) if one or both omni-directional antennas are replaced with directional antennas. However, the range is only extended in one direction obviously.

### Bi-directional Amplifier
Adding a bi-directional amplifier between the Orbweaver and Antenna to one or both Orbweavers can signifcantly increase the range. If an Orbweaver will always be receiving and not transmitting, a low noise amplifier can be attached. If an Orbweaver will always be transmitting and not receiving a power amplifier can be attached. All three of these devices with SMA connectors should be available for purchase online.

Boosting transmission power using a power or bi-directional amplifier may cause Orbweaver to interfer with the operation of other devices in area that utilize the same frequency. To avoid jamming other devices (and to avoid violating local laws and regulations) frequency hopping may be necessary while using an amplifier. Without an amplifier, local regulations likely don't require frequency hopping. See the disclaimer, license, and legal notice section of this document for more.

### Real-World Test Results
We equiped two Orbweavers with rubber ducky omni-directional antennas (no amplifiers) in a fairly dense urban environment with the Orbweavers 2-3 meters off the ground and an otherwise fairly clear Fresnel zone. With this set-up we were able to communicate reliably at 9105 MHz and 50 kbs at a distance of 1 kilometer. In the same environment at 2 kilometers, we were able to send and receive occasional messages at 2.5 kbps, but most of the packets were lost. So the maximum effective communication range in this environment was somewhere bewteen 1 and 2 kilometers.

We feel this was a fairly realistic (i.e. far from ideal but not terrible) set-up. However, many variables may affect communication range both positively and negatively, so these results are not intended as any sort of promise of gaurantee of performance.

# Mounting on a Raspberry Pi and Case Options
Orbweaver can be mounted on a Raspberry Pi 5 with 11 mm standoffs. If the Raspberry Pi is equiped with active cooling, 13 mm standoffs will be necessary. This will leave a 2 mm gap where the header connector mates with the Raspberry Pi, but there will still be plenty of surface area for the pins to contact.

The HighPi Pro Case 5S for Raspberry Pi5 with Universal Opening will accomodate both the Raspberry Pi 5 and the mounted Orbweaver: [HighPi Pro Case 5S](https://shop.hipi.io/product/highpi-pro-case-5s-for-raspberry-pi5-with-universal-opening). In order to provide clearance for the antenna connector use 13 mm standoffs. Note: This is not an endorsement of HiPi.io. We are not affiliated with them in any way.

Open-sided arcylic cases like the following may be used to encase an unmounted Orbweaver: [Raspberry Pi HAT Size Protective Acrylic Case](https://geekworm.com/products/raspberry-pi-x900-x920-dac-protective-acrylic-case?variant=17919722192954&country=US&currency=USD&utm_medium=product_sync&utm_source=google&utm_content=sag_organic&utm_campaign=sag_organic). The Orbweaver can be attached to the acrylic panels without the Raspberry Pi by using 11 mm standoffs on the bottom. Note: This is not an endorsement of Geekworm. We are not affiliated with Geekworm in any way.

Many other case options may be purchased online.

# Help, Further Documentation, etc.
Additional documentation can be found on our GitHub in the documentation folder: [Orbweaver GitHub](https://github.com/vespid-tech/orbweaver).

For additional help, creat a post on the Discussion forum: [Orbweaver Forum](https://github.com/vespid-tech/orbweaver/discussions).

# Disclaimer, License, and Legal Notice
Orbweaver is distributed under the following open source licenses: 

Hardware license: CERN Open Hardware Licence Version 2 – Strongly Reciprocal (CERN-OHL-S-2.0)
Software and firmware license: GNU General Public License v3.0 (GPL-3.0-only)
Documentation: Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)

For the full licenses see our GitHub: [Orbweaver GitHub](https://github.com/vespid-tech/orbweaver)

"Vespid," "Orbweaver," and associated logos are trademarks of Vespid.

Orbweaver is provided “as is,” without warranty of any kind, express or implied. In no event shall Orbweaver developers or contributors be liable for any claim, damages, or other liability arising from its use. Orbweaver is intended for evaluation and development use only. It is not supplied as a finished product and is not certified for use in any product made for sale to the general public. The purchaser is responsible for ensuring that the final configuration (including but not limited to firmware configuration, transmission power, antenna selection, transmission frequency, etc.), complies with applicable regulatory requirements.
