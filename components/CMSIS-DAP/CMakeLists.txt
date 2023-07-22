set(COMPONENT_ADD_INCLUDEDIRS "Include/")
set(COMPONENT_SRCS 
			"Source/DAP.c "
			"Source/DAP_vendor.c"
			"Source/JTAG_DP.c "
			"Source/SW_DP.c "
			"Source/SWD_flash.c "
			"Source/SWD_host.c "
			"Source/SWD_opt.c "
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