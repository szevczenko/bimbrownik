idf_component_register(SRCS "ota_parser.c" "app_events.c" "app_timers.c" "mdns_service.c" "lwjson/lwjson_debug.c" 
                            "lwjson/lwjson_stream.c" "lwjson/lwjson.c" "ota_parser.c"
                    INCLUDE_DIRS "." "lwjson" 
                    REQUIRES config mdns)
