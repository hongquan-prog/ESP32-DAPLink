set(COMPONENT_ADD_INCLUDEDIRS "Include/" "cmsis-core")
set(COMPONENT_SRCS 
			"Source/DAP.c "
			"Source/DAP_vendor.c"
			"Source/JTAG_DP.c "
			"Source/SW_DP.c "
			"Source/swd_flash.c "
			"Source/swd_host.c "
			"Source/swd_opt.c "
			"Source/error.c "
			"algo/STM32_ALGO.c "
			"algo/STM32F0xx_OPT.c "
			"algo/STM32F10x_OPT.c "
			"algo/STM32F3xx_OPT.c "
			"algo/STM32F4xx_OPT.c "
			"algo/STM32F7xx_OPT.c "
			"algo/STM32H7xx.c "

			)
set(COMPONENT_REQUIRES driver)
register_component()