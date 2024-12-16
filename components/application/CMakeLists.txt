idf_component_register(SRCS "network_manager.c" "api_temperature_sensor.c" "hawkbit_process.c"
                            "api_hawkbit.c" "hawkbit_config.c" "mqtt_app.c" "mqtt_config.c"
                            "api_mqtt.c" "device_manager.c" "api_config.c" "api.c" "app_manager.c" 
                    INCLUDE_DIRS "."
                    REQUIRES utils
                    PRIV_REQUIRES spiffs drivers efuse esp_http_client app_update esp-tls nvs_flash backend mqtt wifi_http_app)

