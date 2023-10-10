idf_component_register(SRCS "wifi.c" "ow_esp32.c" "tcp_transport.c"
                    INCLUDE_DIRS "." 
                    REQUIRES driver config nvs_flash wpa_supplicant utils esp_event esp_netif esp_wifi)
