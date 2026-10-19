## Table of Contents
- [Introduction](#introduction)

# Introduction
All Orbweaver communication is packetized. A packet is referred to as a "system command". System commands are formatted the same regardless of.

If you are just interested in how to send and receive data, and don't care about the details, skip ahead to the Python API section of this document: [Python API](#python-api).

# Physical Interfaces
Software running on a Raspberry Pi, desktop, mobile device, etc. may send a receive system commands via SPI or USB. 

### USB
When connected via USB, Orbweaver is recognized as a serial device with the following properties: VID 0x1209 and PID 0x7800. When communicating with an Orbweaver, the serial port should be configured as follows:

baud rate: 460800  
data bite: 8  
parity: None  
stop byte: 1  
hardware flow control: yes (RTS/CTS)

### SPI
Orbweaver can communicate with a Raspberry Pi via SPI. Orbweaver is always configured as a SPI slave. The SPI clock operates at 1 MHz. In additional to the standard SPI pins two pins are used to facilitate communication: Data Ready and Busy. Both pins are open drain inputs to the Raspberry Pi. Orbweaver pulls Data Ready low when it has a system command to send to the Raspberry Pi. Orbweaver pulls Busy low went it begins receiving a system command, and holds it low until the command is processed. No additional system commands should be sent until this pin is set high.

# Orbweaver Protocol
Every piece of software communicating to an Orbweaver (as well as every Orbweaver) is a node. All communication bewteen nodes is accomplished using system commands. Once initialized, Orbweavers are always listening for system commands via USB and SPI. They also always listen via RF unless transmitting or changing channels. 

All system commands sent and received via USB and SPI will be proceeded by a start byte and followed by a stop byte. Start and stop bytes are not considered parts of system commands. Start and stop bytes are not used in RF communication. 

Orbweaver data is little endian. 

Communication via USB or SPI is never encrypted.

### Start and Stop Bytes
A start byte must be sent before every system command. A stop byte must be sent after every system command.

start byte: 0xAA
stop byte: 0xCC

## System Command Structure
A system command is composed of a fixed length header and a variable length parameter. The fields are defined as follows:

byte 0: sequence number
byte 1: source address
byte 2: destination address
byte 3: TTL (Time To Live)
byte 4: length (includes the length of the header and parameter)
byte 5: command
byte 6..255: parameter (optional and variable length)

### Sequence
Should be incremented with each message sent.

When Orbweaver receives a message not intended for any member of it's subnet, it will forward it. In order to prevent Orbweaver from repeatedly forwarding the same system command, Orbweaver holds the last 5 system commands received. If two messages are sent to an Orbweaver with the same source and destination address, the sequence must be different, or Orbweaver will not forward the message. Sequence doesn't have to be increment, but to keep things simple, just increment it each time a message is constructed.

### Source Address
The address of the node that generated the system command.

### Destination Address
The intended final receipiant of the system command.

### Time To Live
How many times to forward a message received via RF. If Orbweaver receives a system command via USB or SPI, and the system command isn't addressed to itself, Orbweaver will forward the message even if TTL is 0 and will not decrement TTL.

Each time Orbweaver receives a message via RF that is not addressed to itself, and TTL is greater than 0, Orbweaver will decrement TTL and retransmit the message.

### Length
The length of the entire system command including the header and parameter. The shortest possible system command is the header, so length will always be at least 6.

### Command ID
The command identifier. 

The command ID combined with the length is used to decode the parameter (if there is one).

Any command can be sent to any node, but not every node is capable of processing every command. If an Orbweaver receives a command it cannot process, it will ignore it. 

A full list of system commands can be found in the repo in the following file: /orbweaver/tree/main/software/Python/shared/ow_protocol.py.

### Parameter
The parameter is optional and variable in length.

Many system commands require parameters. When parameters are required, they must be formatted properly. A partial list of the parameters may be found in the repo in the following file: /orbweaver/tree/main/software/Python/shared/ow_protocol.py.

# Selected System Commands

For the full list of system commands and a partial list of parameters see the following file in the repo: /orbweaver/tree/main/software/Python/shared/ow_protocol.py.

## Selected System Commands

### SPI/UART Peer Commands
Orbweaver can communicate to up to 6 peer nodes via USB and 6 peer nodes via SPI (for a total of 13 nodes on a subnet). During initialization, Orbweaver will attempt to discover how many peer nodes are on its subnet. In order to do this it will send a "SPI Peers Request" (ID: 36) via SPI and a "UART Peers Request" (ID: 34) via USB. Any nodes that are attached should respond with either a "UART Peers" (ID: 35) or a "SPI Peers" (ID: 37) command depending on how they are connected. Connected nodes are not required to respond to peer request commands, but Orbweaver will not send messages to them unless it knows they exist, and it can't know they exist if it never receives a peers command. Each time a node is connected to an already running Orbweaver, the node should send a peers command to let the Orbweaver know it is connected. The parameter of both the "SPI Peers" and "UART Peers" commands contains a list of up to 6 peer addresses. It must contain at least 1 peer address.

Orbweaver will not respond to SPI or UART peer commands. In order to ensure the commands were successfully sent, an echo command can be used to test communication.

Example 1:

A desktop program is initialized. The program uses address 0xFE and also wishes to receive systems commands for address 0xFD from an Orbweaver that is connected via USB and is configured with address 0x06. The following bytes should be sent to the Orbweaver:

0xAA    start byte
0x00    sequence
0xFE    source address
0x06    destination address
0x00    TTL
0x08    length
0x23    command (UART peers)
0xFE    parameter byte 0 (first peer address)
0xFD    parameter byte 1 (second peer address)
0xCC    stop byte

Example 2:

A Raspberry Pi program is initialized that wishes to use address 0xFD. The Raspberry Pi has an Orbweaver mounted, but the Orbweaver's address is unknown. The following bytes should be sent to the Orbweaver:

0xAA    start byte
0x00    sequence
0xFD    source address
0x06    destination address
0x00    TTL
0x07    length
0x25    command (SPI peers)
0xFD    parameter byte 0 (first peer address)
0xCC    stop byte

### Blob
Blob commands (ID: 61) are used to send arbitrary data between nodes that are not Orbweavers. Orbweaver's do not process blob commands. For instance, if a program running on a Raspberry Pi wants to send data to a program running on a desktop via an Orbweaver network, use a blob command and put the arbitrary data in the parameter.

### Echo
Echo commands (ID: 1) can be sent to any node and can have any parameter. Any node receiving an echo command will respond with an "Echo Response" (ID: 2) command and the same parameter it received. Echo commands are useful for communication testing.

### Error
Error commands (ID: 3) are sent to all peers on a subnet when an Orbweaver encounters certain errors. Orbweaver never send error commands via RF. Every error command will have a 1 byte parameter that specifies the error. These commands don't need to be responded to, so they can be safely ignored. 

A list of errors can be found in the repo in the following file: /orbweaver/tree/main/firmware/orbweaver/source/status.h.

### RSSI Commands
RSSI Get commands (ID: 39) are used to request the RSSI value of the last message the Orbweaver received via RF. When an RSSI Get command is received by an Orbweaver, it will respond with an RSSI command (ID: 40). The parameter of the RSSI command is 1 signed byte with the units dBm.

### Radio Stats
Orbweaver keeps track of how many RF messages have been sent and received since start-up. These counts can be requested by sending a "Radio Stats Get" (ID: 48) command. If an Orbweaver receives a Radio Stats Get command it will respond with a "Radio Stats" (ID: 49) command. The Radio Stats command parameter is composed of 2 unsigned 32-bit numbers. The first number is the sent count. The second number is the received count. It order to clear both counts, a "Radio Stats Clear" (ID: 49) command can be sent to the Orbweaver.

# Python API
Python Code demonstrating how to interact with an Orbweaver can be found in the repository in the following folder: /orbweaver/tree/main/software/Python/examples. The scripts in the folder show how use the above selected commands. Each script has plenty of comments that explain what it is doing, and and how to use it.

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
