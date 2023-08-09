idf_component_register(SRCS "api.c" "app_manager.c" "network_manager.c" "tcp_server.c"
                    INCLUDE_DIRS "." 
                    REQUIRES config drivers utils)
