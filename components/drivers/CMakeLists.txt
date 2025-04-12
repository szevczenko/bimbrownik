idf_component_register(SRCS "onewire_uart/src/devices/ow_device_ds18x20.c" 
                            "onewire_uart/src/ow/ow.c" "temperature.c" "json_parser.c" "analog_in.c"
                            "digital_in_out.c" "mqtt_json_parser.c"
                    INCLUDE_DIRS "." "onewire_uart/src/include"
                    REQUIRES drv utils mongoose_drv
                    PRIV_REQUIRES application config project_hal hal backend )
