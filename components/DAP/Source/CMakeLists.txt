
set(COMPONENT_SRCS  "DAP.c" 
                    "SW_DP.c"
                    "DAP_vendor.c"
                    "error.c" 
                    "JTAG_DP.c"
                    "swd_host.c"
)
set(COMPONENT_INCLUDEDIRS . ${PROJECT_DIR}/components/CMSIS-DAP/Include
. ${PROJECT_DIR}/components/CMSIS-DAP/cmsis-core)
register_component()