set(COMPONENT_ADD_INCLUDEDIRS 
    "include"
    "include/hal"
)

set(COMPONENT_SRCS 
    # Server core
    "src/server/usbip_server.c"
    "src/server/usbip_urb.c"
    "src/server/usbip_devmgr.c"
    "src/server/usbip_control.c"
    "src/server/usbip_protocol.c"
    # Device base classes
    "src/device/usbip_hid.c"
    "src/device/usbip_bulk.c"
    # HAL
    "src/hal/usbip_osal.c"
    "src/hal/usbip_mempool.c"
    "src/hal/usbip_transport.c"
    # Platform (ESP-IDF)
    "src/platform/espidf/transport_espidf.c"
    "src/platform/espidf/osal_espidf.c"
    # Device drivers
    "src/device_drivers/hid_dap.c"
    "src/device_drivers/bulk_dap.c"
)

set(COMPONENT_REQUIRES 
    "esp_timer"
    "freertos"
    "log"
    "lwip"
    "newlib"
)

set(COMPONENT_PRIV_REQUIRES
    "debug_probe"
)

register_component()

# Force linker to keep USBIP constructor functions
# These functions are marked with __attribute__((constructor)) but may be stripped by linker
# because they are not explicitly called from anywhere
target_link_options(${COMPONENT_LIB} INTERFACE
    "LINKER:--undefined=hid_dap_driver_register"
    "LINKER:--undefined=bulk_dap_driver_register"
    "LINKER:--undefined=transport_register_espidf"
    "LINKER:--undefined=osal_register_espidf"
)
