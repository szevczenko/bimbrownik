idf_component_register(SRCS "hawkbit_parser.c" "app_events.c" "app_timers.c" "mdns_service.c" "lwjson/lwjson_debug.c" 
                            "lwjson/lwjson_stream.c" "lwjson/lwjson.c"
                    INCLUDE_DIRS "." "lwjson" 
                    REQUIRES config mdns)

set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY LINK_INTERFACE_MULTIPLICITY 6)
