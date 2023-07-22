set(COMPONENT_ADD_INCLUDEDIRS "config include $ENV{IDF_PATH}/components/soc/" "$ENV{IDF_PATH}/components/driver/spi/include")
set(COMPONENT_SRCS "./source/DAP.c ./source/DAP_vendor.c ./source/JTAG_DP.c ./source/SW_DP.c ./source/SWO.c ./source/dap_utility.c ./source/spi_switch.c ./source/spi_op.c")
register_component()