set(COMPONENT_ADD_INCLUDEDIRS 
    "include"
    "include/hal"
)

set(COMPONENT_SRCS 
    # Server core
    "usbipd/components/usbipd/src/server/usbip_conn.c"
    "usbipd/components/usbipd/src/server/usbip_control.c"
    "usbipd/components/usbipd/src/server/usbip_devmgr.c"
    "usbipd/components/usbipd/src/server/usbip_pack.c"
    "usbipd/components/usbipd/src/server/usbip_server.c"
    "usbipd/components/usbipd/src/server/usbip_urb.c"
    

    # Device base classes
    "usbipd/components/usbipd/src/device/usbip_hid.c"
    # HAL
    "usbipd/components/usbipd/src/hal/usbip_osal.c"
    "usbipd/components/usbipd/src/hal/usbip_mempool.c"
    "usbipd/components/usbipd/src/hal/usbip_transport.c"
    # Platform (ESP-IDF)
    "platform/espidf_transport.c"
    "platform/espidf_os.c"
    # Device drivers
    "usbipd/components/usbipd/src/hid_dap.c"
    "usbipd/components/usbipd/src/bulk_dap.c"
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
    "LINKER:--undefined=dap_lock_init"
)
