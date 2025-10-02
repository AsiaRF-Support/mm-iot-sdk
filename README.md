# AsiaRF MM-IoT-SDK

## Description

AsiaRF for Wi-Fi HaLow IoT SDK, based on [Morse Micro IoT Software Development Kit](https://github.com/MorseMicro/mm-iot-sdk?tab=readme-ov-file#morse-micro-iot-software-development-kit) v2.8.2. This package contains the Morse Micro IoT SDK, providing a framework for getting started with Morse Micro HaLow Wi-Fi. We develop new Wi-Fi IoT devices for a variety of Wi-Fi HaLow application scenarios, providing long-range transmission capabilities in large-scale, long-distance IoT deployments.



## Device Support

- **AWH575-MF1-V2**
> [Wi-Fi HaLow IoT Powerful Industrial Remote Control Kit with RS232 RS485 I2C SPI interfaces AWH575-MF1](https://asiarf.com/product/wifi-halow-industrial-remote-contral-awh575-mf1/)
- **AWH575-001**
> [Wi-Fi HaLow Industrial IoT Smart Control Kit – Long-Range, Multi-Interface Low Power Solution AWH575-001](https://asiarf.com/product/wi-fi-halow-iot-smart-control-kit-awh575-001/)
- **AWMHU5-001**



## AsiaRF Example

- tcpecho     : Basic TCP server application.
- tcpclient   : Basic TCP client application.
- cfg         : Wi-Fi HaLow setttings by USB CDC-ACM.
- modbus      : RTU Gateway with capability to converting RS-485/RS-232/UART protocol to TCP/IP.
- thingsboard : ThingsBoard example for AWH575-001. AWH575-001 remote control three relays and LED dimming by Thingsboard's dashboard.



## Usage

Quickly execute the example on the AsiaRF platform according to the [build_guide.txt](https://github.com/AsiaRF-Support/mm-iot-sdk/blob/asiarf-patched-2.8.2/build_guide.txt) file.

More details at [Morse.Micro.IoT.SDK.2.8.2.API.Reference.Manual.pdf](https://github.com/MorseMicro/mm-iot-sdk/releases/download/2.8.2/Morse.Micro.IoT.SDK.2.8.2.API.Reference.Manual.pdf) for the full MM-IoT-SDK user guide.

Follow [ThiingsBoard setup guide](https://github.com/AsiaRF-Support/mm-iot-sdk/blob/asiarf-patched-2.8.2/examples/thingsboard_demo/SETUP_GUIDE.md) to setup your AWH575-001 on ThingsBoard.io.



## What AsiaRF can Do

- Bring up to [Home Assistant](https://www.home-assistant.io/) application.
- Bring up to [ThingsBoard](https://thingsboard.io/) application.
- Mobile Wi-Fi HaLow IoT.
- Wi-Fi HaLow Ethernet interface for most platform by USB.
  [Wi-Fi HaLow USB Dongle HaLowFly](https://asiarf.com/product/wi-fi-halow-usb-dongle-halowfly/)
- Various industrial applications.



# License

This repository and its contents, including software component Makefiles and metadata, are licensed under the Apache 2.0, unless otherwise stated in individual files.

Software components from third party sources provide their own licenses.
