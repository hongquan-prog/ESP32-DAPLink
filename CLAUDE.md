# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-DAPLink is an ESP32-S3 based hardware debugger implementing CMSIS-DAP protocol. It provides:
- DAPLink online debugging (HID/WinUSB modes)
- CDC serial communication (USB and Web Serial)
- Offline firmware programming with Keil FLM algorithm support
- Remote programming via web interface

## Build Commands

This is an ESP-IDF project. Ensure `IDF_PATH` environment variable is set.

```bash
source /home/lhq/Software/esp-idf/export.sh

# Build the project
idf.py build

# Flash to device
idf.py flash

# Build and flash
idf.py build flash

# Monitor serial output
idf.py monitor

# Build, flash, and monitor
idf.py build flash monitor

# Clean build
idf.py fullclean && idf.py build

# Configure (menuconfig)
idf.py menuconfig
```

## Architecture

### Component Structure

```
components/
├── debug_probe/   # CMSIS-DAP implementation (DAP.c, SW_DP.c, JTAG_DP.c, swd.c)
├── Program/       # Offline programming (flash algorithms, HEX/BIN parsing, SWD host)
├── USBIP/         # USBIP protocol for networked debugging
├── elaphureLink/  # elaphureLink protocol
├── kcp/           # KCP protocol for reliable transport
└── util/          # Utilities (LED control)

main/              # Application entry point and handlers
├── main.cpp       # Entry point, USB init, WiFi, task creation
├── cdc_uart.c     # UART bridge with dual output (USB/Web)
├── usb_cdc_handler.c  # USB CDC callbacks
├── web_handler.cpp    # WebSocket serial handler
├── web_server.c       # HTTP/WebSocket server
├── SerialManager.cpp  # Serial port state management (USB vs Web)
└── programmer.cpp     # Offline programming state machine
```

### Key Data Flows

1. **DAP Debugging**: Host (Keil/OpenOCD) → USB (HID/WinUSB) → DAP_ProcessCommand() → SWD/JTAG → Target
2. **USB Serial**: Host CDC → tud_cdc_rx_cb → usb_cdc_send_to_uart() → UART → Target
3. **Web Serial**: Browser WebSocket → web_handler → SerialManager → UART → Target
4. **Serial Output**: UART RX → cdc_uart → (USB CDC || WebSocket broadcast)

### Serial Port Management

`SerialManager` (main/SerialManager.cpp) manages state transitions:
- `STATE_IDLE`: No client connected, UART data discarded
- `STATE_USB`: USB CDC connected, UART data → USB
- `STATE_WEB`: Web client connected, UART data → WebSocket

Priority: USB > Web > Idle

### USB Modes

Controlled by `USBIP_ENABLE_WINUSB` in `main/dap_configuration.h`:
- `USBIP_ENABLE_WINUSB=1`: WinUSB mode (recommended, DAP_PACKET_SIZE=512)
- `USBIP_ENABLE_WINUSB=0`: HID mode (Keil MDK requires this, DAP_PACKET_SIZE=255)

### Programming Architecture

The `Program` component implements a streaming programmer pattern:
- `StreamProgrammer`: Main coordinator
- `TargetFlash`: Flash operations with sector management
- `FlashAccessor`: Low-level flash access
- `SWDIface` / `TargetSWD`: SWD interface abstraction
- `AlgoExtractor`: Extracts flash algorithms from FLM files

## Code Style

See `docs/CODE_STYLE.md` for the complete style guide. Key points:

- **Naming**: snake_case for functions/variables, PascalCase for classes, `_prefix` for private members
- **Braces**: Allman style (opening brace on new line)
- **Indentation**: 4 spaces, no tabs
- **Headers**: Use `#pragma once`
- **C/C++**: C headers use `extern "C"` guards
- **No global variables**: Use dependency injection

## Dependencies

Managed via ESP-IDF component manager (`dependencies.lock`):
- `espressif/tinyusb`: TinyUSB stack
- `espressif/esp_tinyusb`: ESP-IDF TinyUSB integration

## Important Files

- `components/debug_probe/DAP/Config/DAP_config.h`: DAP pin assignments
- `algorithm/`: FLM flash algorithm files for offline programming