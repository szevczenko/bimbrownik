idf_component_register(SRCS "ota.c" "api_config.c" "api.c" "app_manager.c" "network_manager.c" "tcp_server.c" "api_temperature_sensor.c"
                            "api_ota.c" "ota_config.c" "mqtt_app.c" "mqtt_config.c" "api_mqtt.c" "device_manager.c"
                    INCLUDE_DIRS "." 
                    REQUIRES config drivers utils efuse esp_http_client esp_https_ota app_update esp-tls mqtt spiffs
                    )
