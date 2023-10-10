idf_component_register(SRCS "ota.c" "api_config.c" "api.c" "app_manager.c" "network_manager.c" "tcp_server.c" "api_temperature_sensor.c"
                    INCLUDE_DIRS "." 
                    REQUIRES config drivers utils efuse esp_http_client esp_https_ota app_update
                    EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem)
