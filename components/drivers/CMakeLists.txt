idf_component_register(SRCS "error_code.c" "wifidrv.c" "onewire_uart/src/devices/ow_device_ds18x20.c" 
                            "onewire_uart/src/ow/ow.c" "temperature.c" "json_parser.c" "analog_in.c"
                            "digital_in_out.c" "mqtt_json_parser.c" "water_flow_sensor.c"
                    INCLUDE_DIRS "." "onewire_uart/src/include"
                    REQUIRES application config project_hal utils hal esp32-wifi-manager)
