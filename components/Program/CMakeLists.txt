set(COMPONENT_ADD_INCLUDEDIRS "inc/")
set(COMPONENT_SRCS 
            "src/swd_iface.cpp"
            "src/target_swd.cpp"
            "src/target_flash.cpp"
            "src/flash_accessor.cpp"
			"src/bin_program.cpp"
            "src/hex_parser.c"
			"src/hex_program.cpp"
            "src/algo_extractor.cpp"
            "src/file_programmer.cpp"
            "src/stream_programmer.cpp"
			)
set(COMPONENT_REQUIRES fatfs DAP)
register_component()