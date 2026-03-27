idf_component_register(SRCS "main.cpp"
                        # Serial
                        "serial/cdc_uart.c"
                        "serial/serial_manager.c"
                        # USB
                        "usb/msc_disk.c"
                        "usb/usb_cdc_handler.c"
                        "usb/usb_desc.c"
                        # Web
                        "web/web_handler.cpp"
                        "web/web_server.c"
                        # USBIP
                        "usbip/usbip_server.c"
                        "usbip/usbip_context.c"
                        "usbip/usbip_service.c"
                        "usbip/usbip_protocol.c"
                        "usbip/usbip_endpoint_handler.c"
                        "usbip/dap_handler.c"
                        "usbip/swo_handler.c"
                        "usbip/transport/transport.c"
                        "usbip/transport/transport_tcp.c"
                        "usbip/transport/transport_netconn.c"
                        "usbip/transport/transport_kcp.c"
                        # Programmer
                        "programmer/prog.cpp"
                        "programmer/programmer.cpp"
                        "programmer/prog_data.cpp"
                        "programmer/prog_idle.cpp"
                        "programmer/prog_online.cpp"
                        "programmer/prog_offline.cpp"
                       INCLUDE_DIRS . usbip usbip/transport usb serial web programmer
                       EMBED_FILES "../html/root.html"
                        "../html/favicon.ico"
                        "../html/program.html"
                        "../html/webserial.html"
                        "../html/upgrade.html")