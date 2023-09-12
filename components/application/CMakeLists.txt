idf_component_register(SRCS "api_config.c" "api.c" "app_manager.c" "network_manager.c" "tcp_server.c" "api_temperature_sensor.c"
                    INCLUDE_DIRS "." 
                    REQUIRES config drivers utils)
