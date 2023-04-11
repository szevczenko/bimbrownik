idf_component_register(SRCS "wifi.c" "ow_esp32.c" "tcp_transport.c"
                    INCLUDE_DIRS "." 
                    REQUIRES config nvs_flash wpa_supplicant)
