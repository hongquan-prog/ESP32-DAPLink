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
                        # Programmer
                        "programmer/prog.cpp"
                        "programmer/programmer.cpp"
                        "programmer/prog_data.cpp"
                        "programmer/prog_idle.cpp"
                        "programmer/prog_online.cpp"
                        "programmer/prog_offline.cpp"
                       INCLUDE_DIRS . usb serial web programmer
                       EMBED_FILES "../html/root.html"
                        "../html/favicon.ico"
                        "../html/program.html"
                        "../html/webserial.html"
                        "../html/upgrade.html")
