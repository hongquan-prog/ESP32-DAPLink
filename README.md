# ESP32-DAPLink

ESP32-DAPLink is an open-source hardware debugger based on ESP32-S3, implementing the CMSIS-DAP protocol. It provides comprehensive features for embedded systems development and debugging.

## Features

- **DAPLink Online Debugging**: Supports both HID and WinUSB/Bulk modes for online debugging with Keil MDK, OpenOCD, and other debug tools
- **CDC Serial Communication**: USB CDC serial communication for seamless data transfer between host and target device
- **Web Serial Support**: Web browser-based serial communication via WebSocket, enabling remote monitoring and control
- **Wireless Debugging**: WiFi-based wireless debugging through USBIP (USB over IP) protocol
- **Offline Programming**: Standalone firmware programming with automatic Keil FLM algorithm parsing
- **Remote Programming**: Web interface for remote firmware updates and device programming

## Architecture

```
ESP32-DAPLink/
├── components/
│   ├── debug_probe/    # CMSIS-DAP implementation (DAP.c, SW_DP.c, JTAG_DP.c)
│   ├── Program/        # Offline programming (flash algorithms, HEX/BIN parsing)
│   ├── usbipd/         # USBIP server for wireless debugging
│   │   ├── include/    # Public headers (usbip_server.h, usbip_protocol.h, etc.)
│   │   └── src/        # Source files
│   │       ├── server/     # USBIP server core (connection, URB handling)
│   │       ├── device/     # HID/Bulk device base classes
│   │       ├── hal/        # OS abstraction layer (OSAL, mempool, transport)
│   │       ├── platform/   # ESP-IDF specific implementations
│   │       └── device_drivers/  # HID DAP and Bulk DAP drivers
│   └── util/           # Utilities (LED control)
├── main/
│   ├── main.cpp        # Entry point, USB init, WiFi, task creation
│   ├── serial/         # Serial port management
│   │   ├── cdc_uart.c/.h      # UART bridge implementation
│   │   └── serial_manager.c/.h # USB/Web serial state management
│   ├── usb/            # USB device handling
│   │   ├── usb_desc.c/.h      # USB descriptors
│   │   ├── usb_cdc_handler.c/.h # USB CDC callbacks
│   │   └── msc_disk.c/.h      # Mass storage emulation
│   ├── web/            # Web server and handlers
│   │   ├── web_server.c/.h     # HTTP/WebSocket server
│   │   └── web_handler.cpp/.h  # WebSocket serial and programming handlers
│   └── programmer/     # Offline/Online programming
│       └── programmer.cpp/.h   # Programming state machine
├── algorithm/          # FLM flash algorithm files
├── html/               # Web interface files
└── docs/               # Documentation
```

## Serial Port Management

The `SerialManager` manages serial port state with priority-based switching. UART is always initialized, but data routing depends on connection state:

| State | Description |
|-------|-------------|
| `SERIAL_STATE_IDLE` | No client connected, UART data discarded |
| `SERIAL_STATE_USB` | USB CDC connected, UART data → USB |
| `SERIAL_STATE_WEB` | Web client connected, UART data → WebSocket |

Priority: USB > Web > Idle

State transitions automatically notify web clients, enabling real-time status updates in the browser interface.

## Build

This is an ESP-IDF project. Ensure `IDF_PATH` environment variable is set.

```bash
# Source ESP-IDF
source /path/to/esp-idf/export.sh

# Build the project
idf.py build

# Flash to device
idf.py flash

# Build and flash
idf.py build flash

# Monitor serial output
idf.py monitor

# Build, flash, and monitor
idf.py -p /dev/ttyUSB0 build flash monitor

# Clean build
idf.py fullclean && idf.py build

# Configuration menu
idf.py menuconfig
```

## Configuration

All configurations can be accessed via `idf.py menuconfig`.

### DAPLink Connection Method

Navigate to `ESP32 DAPLink Configuration` → `DAPLink connection method`:

| Option | Description |
|--------|-------------|
| **HID** | Connect to computer by HID (Required for Keil MDK) |
| **Bulk** | Connect to computer by bulk transmission (Recommended) |

### USBIP Server Settings

Navigate to `USBIP Server Configuration`:

| Option | Description |
|--------|-------------|
| `Enable USBIP server` | Enable USBIP for wireless debugging |
| `Server port` | TCP port for USBIP server (default: 3240) |
| `URB thread stack size` | Stack size for URB processing thread |
| `URB thread priority` | Priority for URB processing thread |

### Debug Probe Settings

Navigate to `Debug Probe Configuration`:

| Option | Description |
|--------|-------------|
| `Debug interface` | Select ESP-USB JTAG or CMSIS-DAP SWD |
| `GPIO pins` | Configure TDI, TDO, TCK, TMS pins |
| `Default SWJ Clock` | Set SWJ clock frequency (500KHz - 10MHz) |

## Usage Notes

### Keil MDK Compatibility

Keil MDK requires **HID mode**. In `idf.py menuconfig`:
1. Go to `ESP32 DAPLink Configuration`
2. Select `Connect to computer by HID`

### Linux Permission Setup

Add udev rules for CMSIS-DAP device access:

```bash
# Create udev rule file
sudo nano /etc/udev/rules.d/60-cmsis-dap.rules

# Add the following content (for elaphureLink VID/PID)
SUBSYSTEM=="usb", ATTR{idVendor}=="faed", ATTR{idProduct}=="4873", MODE="0666", GROUP="plugdev"
SUBSYSTEM=="usb", ATTR{idVendor}=="faed", ATTR{idProduct}=="4873", TAG+="uaccess"

# Apply the rules
sudo udevadm control --reload-rules && sudo udevadm trigger

# Add user to plugdev group
sudo usermod -aG plugdev $USER
```

## Offline Programming Porting

The offline programming functionality is separated from the main DAPLink code. To implement your own offline programmer, implement the following interfaces in `swd_iface.cpp`:

```cpp
virtual void msleep(uint32_t ms) = 0;
virtual bool init(void) = 0;
virtual bool off(void) = 0;
virtual transfer_err_def transfer(uint32_t request, uint32_t data) = 0;
virtual void swj_sequence(uint32_t count, const uint8_t *data) = 0;
virtual void set_target_reset(uint8_t asserted) = 0;
```

## Contributing

Contributions are welcome! Feel free to submit issues and pull requests.

Happy debugging!
