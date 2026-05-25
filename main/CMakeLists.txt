set(usb_srcs "")
if (CONFIG_USB_DEBUG_PROBE)
set(usb_srcs
        "usb/msc_disk.c"
        "usb/usb_cdc_handler.c"
        "usb/usb_desc.c")
endif()

set(main_requires
        espressif__cjson
        esp_wifi
        esp_http_server
        nvs_flash
        Program
        usbipd
)

# Starting from esp-idf v5.3, UART driver is moved to a separate component.
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
        list(APPEND main_requires esp_driver_uart)
else()
        list(APPEND main_requires driver)
endif()

idf_component_register(SRCS "main.cpp"
                        # Disk
                        "disk/disk.c"
                        # Serial
                        "serial/cdc_uart.c"
                        "serial/serial_manager.c"
                        # USB
                        ${usb_srcs}
                        # Web
                        "web/web_handler.cpp"
                        "web/web_server.c"
                        # Programmer
                        "programmer/prog.cpp"
                        "programmer/programmer.cpp"
                        "programmer/prog_data.cpp"
                        "programmer/prog_idle.cpp"
                        "programmer/prog_online.cpp"
                        "programmer/prog_offline.cpp"
                        # WiFi
                        "wifi.c"
                       REQUIRES ${main_requires}
                       PRIV_REQUIRES app_update spi_flash
                       INCLUDE_DIRS . usb serial web programmer disk
                       EMBED_FILES "../html/root.html"
                        "../html/favicon.ico"
                        "../html/webserial.html"
                        "../html/upgrade.html"
                        "../html/settings.html")
