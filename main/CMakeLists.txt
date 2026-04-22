set(usb_srcs "")
if (CONFIG_USB_DEBUG_PROBE)
set(usb_srcs
        "usb/msc_disk.c"
        "usb/usb_cdc_handler.c"
        "usb/usb_desc.c")
endif()

idf_component_register(SRCS "main.cpp"
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
                       INCLUDE_DIRS . usb serial web programmer
                       EMBED_FILES "../html/root.html"
                        "../html/favicon.ico"
                        "../html/program.html"
                        "../html/webserial.html"
                        "../html/upgrade.html"
                        "../html/settings.html")
