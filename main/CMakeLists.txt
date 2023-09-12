idf_component_register(SRCS "main.cpp" 
                        "cdc_uart.c"
                        "msc_disk.c"
                        "web_handler.c"
                        "usb_cdc_handler.c"
                        "web_server.c"
                        "programmer.cpp"
                       INCLUDE_DIRS .
                       EMBED_FILES "../html/root.html"
                        "../html/favicon.ico"
                        "../html/program.html"
                        "../html/webserial.html")