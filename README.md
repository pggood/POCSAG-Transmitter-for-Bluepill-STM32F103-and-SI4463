POCSAG Transmitter with STM32F103 and SI4463
============================================

A complete POCSAG (POCSAG 512/1200) transmitter implementation using STM32F103 (Blue Pill) and SI4463 RF module. This project allows you to send pager messages using the POCSAG protocol in the 433MHz ISM band.

Features
--------

-   POCSAG 512/1200 baud rate support

-   Frequency range: 135-175MHz, 400-470MHz, 850-930MHz

-   Up to 40 character messages

-   Support for all 4 POCSAG address sources (0-3)

-   Repeat transmission capability

-   Serial command interface

-   Pure C implementation for STM32CubeIDE

Hardware Requirements
---------------------

### Components

-   STM32F103C8T6 Blue Pill board

-   SI4463 or SI4464 RF module (433MHz)

-   3.3V compatible antenna

-   USB to Serial adapter (for programming and communication)

-   Breadboard and jumper wires

### Wiring Diagram

| Blue Pill Pin | SI4463/SI4464 Pin | Function |
| --- | --- | --- |
| 3.3V | VCC | Power |
| GND | GND | Ground |
| PB0 | SDN | Shutdown |
| PB1 | nIRQ | Interrupt (optional) |
| PB10 | nSEL | Chip Select |
| PA5 | SCK | SPI Clock |
| PA6 | MISO | SPI Data Out |
| PA7 | MOSI | SPI Data In |

Software Requirements
---------------------

-   STM32CubeIDE

-   STM32CubeMX (for configuration)

-   Serial terminal program (PuTTY, Tera Term, etc.)

Installation
------------

1.  Clone the repository

    bash

    git clone https://github.com/pggood/stm32-pocsag-transmitter.git
    cd stm32-pocsag-transmitter

2.  Open in STM32CubeIDE

    -   Import as existing STM32CubeIDE project

    -   Ensure all source files are in correct locations

3.  Build the project

    -   Clean and build the project in STM32CubeIDE

    -   Ensure no compilation errors

4.  Flash to Blue Pill

    -   Connect ST-Link programmer

    -   Flash the compiled binary

Usage
-----

### Serial Commands

Connect to the Blue Pill via USB-to-Serial at 9600 baud.

#### Send POCSAG Message

text

P <address> <source> <repeat> <message>

Parameters:

-   `address`: 21-bit POCSAG address (1-2097151)

-   `source`: Address source (0-3)

-   `repeat`: Number of repeats (0-9)

-   `message`: Text message (up to 40 characters)

Example:

text

P 123456 0 1 "Hello Pager World!"

#### Set Frequency

text

F <freqmhz> <freq100Hz>

Parameters:

-   `freqmhz`: MHz part of frequency

-   `freq100Hz`: 100Hz part (4 digits)

Examples:

text

F 433 9200    # 433.9200 MHz
F 148 0250    # 148.0250 MHz (traditional pager frequency)

### Example Session

text

POCSAG text-message tool v0.1 (STM32F103 + SI4463)
https://github.com/pggood/stm32-pocsag-transmitter
Format:
P <address> <source> <repeat> <message>
F <freqmhz> <freq100Hz>

P 123456 0 1 "Test Message"
address: 123456
addresssource: 0
repeat: 1
message: Test Message
POCSAG message created: 140 bytes
POCSAG SEND with SI4463 (transmission 1)
SI4463: Transmitting 140 bytes (~1033 ms)
SI4463: Transmission complete
Transmission complete

Project Structure
-----------------

text

stm32-pocsag-transmitter/
├── Core/
│   ├── Inc/
│   │   ├── pocsag.h
│   │   ├── si4463_driver.h
│   │   └── radio_config_Si4463.h
│   ├── Src/
│   │   ├── main.c
│   │   ├── pocsag.c
│   │   └── si4463_driver.c
│   └── Startup/
├── Drivers/
├── 103POCSAG_transmitter.ioc
└── README.md

File Descriptions
-----------------

-   `main.c` - Main application with serial command parser

-   `pocsag.h/c` - POCSAG encoding implementation (pure C)

-   `si4463_driver.h/c` - SI4463 radio driver

-   `radio_config_Si4463.h` - SI4463 configuration for POCSAG

-   `103POCSAG_transmitter.ioc` - STM32CubeMX configuration file

Configuration
-------------

### POCSAG Parameters

-   Data Rate: 1200 bps (configurable to 512 bps)

-   Modulation: 2-FSK

-   Deviation: ~4.5 kHz

-   Sync Word: 0x7CD215D8 (POCSAG standard)

### SI4463 Configuration

-   Crystal: 30 MHz

-   RF Power: Configurable (default for EU ISM compliance)

-   Modulation: FSK

-   Frequency Bands: 135-175MHz, 400-470MHz, 850-930MHz

Legal Notice
------------

Important: Check your local regulations before transmitting:

-   Transmit power limits vary by country

-   Frequency restrictions apply in most regions

-   Licensing requirements may apply for certain frequencies

-   Interference with licensed services must be avoided

This project is for educational and ham radio use only. Ensure compliance with your local telecommunications regulations.

Troubleshooting
---------------

### Common Issues

1.  No transmission

    -   Verify SI4463 wiring

    -   Check 3.3V power supply

    -   Verify antenna connection

2.  SPI communication errors

    -   Check SCK, MOSI, MISO connections

    -   Verify nSEL (PB10) is toggling

3.  Serial communication issues

    -   Verify baud rate (9600)

    -   Check TX/RX connections

    -   Ensure proper ground connection

4.  Poor range

    -   Check antenna type and connection

    -   Verify frequency setting

    -   Ensure adequate power supply

### Debugging

Enable debug messages in the serial terminal to monitor:

-   SI4463 initialization status

-   POCSAG message creation

-   Transmission progress

Contributing
------------

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

License
-------

This project is licensed under the MIT License - see the LICENSE file for details.

Acknowledgments
---------------

-   Based on original POCSAG work by Kristoff Bonne (ON1ARF)

-   SI4463 configuration inspired by Silicon Labs WDS

-   STM32 HAL drivers by STMicroelectronics

Support
-------

For support and questions:

-   Open an issue on GitHub

-   Check the troubleshooting section above

-   Refer to STM32 and SI4463 documentation

* * * * *

Happy transmitting! 📡
