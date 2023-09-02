set(COMPONENT_ADD_INCLUDEDIRS "Include/" "cmsis-core")
set(COMPONENT_SRCS 
			"Source/DAP.c "
			"Source/DAP_vendor.c"
			"Source/JTAG_DP.c"
			"Source/SW_DP.c"
			"Source/swd_host.c"
			"Source/error.c"
			)
set(COMPONENT_REQUIRES driver)
register_component()