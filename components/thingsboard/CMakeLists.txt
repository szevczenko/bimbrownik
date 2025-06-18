idf_component_register(SRCS "telemetry.c"
        "mqtt_app.c"
        "mqtt_config.c"
        "attributes.c"
        "shared_attribute_callback.c"
        "shared_attribute_update.c"
        "rpc_callback.c"
        "rpc_request_callback.c"
        "server_side_rpc.c"
        "constants.c"
        "thingsboard.c"
        "thingsboard_test.c"
    
    INCLUDE_DIRS "."
    
    REQUIRES utils
        
    PRIV_REQUIRES nvs_flash
        spiffs
        mongoose_drv
        drv
)

