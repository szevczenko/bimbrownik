idf_component_register(SRCS "wifidrv.c" "onewire_uart/src/devices/ow_device_ds18x20.c" "onewire_uart/src/ow/ow.c" "temperature.c"
                    INCLUDE_DIRS "." "onewire_uart/src/include"
                    REQUIRES application config project_hal utils hal)
