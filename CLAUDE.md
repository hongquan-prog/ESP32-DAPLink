# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-DAPLink is an ESP32-S3 based hardware debugger implementing CMSIS-DAP protocol. It provides:
- DAPLink online debugging (HID/WinUSB modes)
- CDC serial communication (USB and Web Serial)
- Offline firmware programming with Keil FLM algorithm support
- Remote programming via web interface
- Wireless debugging via USBIP protocol

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
├── usbipd/        # USBIP server for wireless debugging
│   ├── include/   # Public headers
│   │   ├── usbip_server.h      # Server main interface
│   │   ├── usbip_protocol.h    # USBIP protocol definitions
│   │   ├── usbip_devmgr.h      # Device driver interface
│   │   ├── usbip_control.h     # Control transfer framework
│   │   ├── usbip_hid.h         # HID device base class
│   │   ├── usbip_common.h      # Common definitions
│   │   └── hal/                # Hardware abstraction layer
│   │       ├── usbip_osal.h    # OS abstraction (mutex, thread, etc.)
│   │       ├── usbip_transport.h # Transport abstraction
│   │       └── usbip_log.h     # Logging system
│   └── src/
│       ├── server/             # Server core
│       │   ├── usbip_server.c  # Connection management
│       │   ├── usbip_urb.c     # URB processing (producer-consumer)
│       │   ├── usbip_devmgr.c  # Device management
│       │   └── usbip_protocol.c # Protocol utilities
│       ├── device/             # Device base classes
│       │   ├── usbip_hid.c     # HID device implementation
│       │   └── usbip_bulk.c    # Bulk device implementation
│       ├── hal/                # HAL implementations
│       │   ├── usbip_osal.c    # OSAL wrapper functions
│       │   ├── usbip_mempool.c # Static memory pool
│       │   └── usbip_transport.c # Transport interface
│       ├── platform/espidf/    # ESP-IDF specific
│       │   ├── osal_espidf.c   # FreeRTOS implementation
│       │   └── transport_espidf.c # lwIP TCP transport
│       └── device_drivers/     # DAP device drivers
│           ├── hid_dap.c       # HID CMSIS-DAP (busid: 2-1)
│           └── bulk_dap.c      # Bulk CMSIS-DAP (busid: 2-2)
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

1. **DAP Debugging (USB)**: Host (Keil/OpenOCD) → USB (HID/Bulk) → DAP_ProcessCommand() → SWD/JTAG → Target
2. **DAP Debugging (USBIP)**: Host → USBIP protocol → usbip_server → hid_dap/bulk_dap → DAP_ProcessCommand() → SWD/JTAG → Target
3. **USB Serial**: Host CDC → tud_cdc_rx_cb → usb_cdc_send_to_uart() → UART → Target
4. **Web Serial**: Browser WebSocket → web_handler → SerialManager → UART → Target
5. **Serial Output**: UART RX → cdc_uart → (USB CDC || WebSocket broadcast)

### USBIP Architecture

The USBIP implementation uses a modular, multi-threaded architecture:

- **Server Thread**: Accepts TCP connections, handles OP_REQ_DEVLIST/OP_REQ_IMPORT
- **Connection Thread**: Per-client connection handling
- **URB Processor Thread**: Producer-consumer queue for URB processing
- **Device Drivers**: Auto-registered via constructor attribute

Key files:
- `usbip_server.c`: Main server loop, connection management
- `usbip_urb.c`: URB queue and processor thread
- `hid_dap.c`: HID device driver with MS OS 2.0 descriptors
- `bulk_dap.c`: Bulk device driver for WinUSB

### Serial Port Management

`SerialManager` (main/SerialManager.cpp) manages state transitions:
- `STATE_IDLE`: No client connected, UART data discarded
- `STATE_USB`: USB CDC connected, UART data → USB
- `STATE_WEB`: Web client connected, UART data → WebSocket

Priority: USB > Web > Idle

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
- `components/usbipd/Kconfig`: USBIP server configuration options
- `algorithm/`: FLM flash algorithm files for offline programming
