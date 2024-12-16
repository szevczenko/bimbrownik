idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES config
                    PRIV_REQUIRES drivers project_hal utils application ui wifi_http_app spiffs mongoose_drv)
