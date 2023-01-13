idf_component_register(SRCS "wifi.c"
                    INCLUDE_DIRS "." 
                    REQUIRES config nvs_flash wpa_supplicant)
