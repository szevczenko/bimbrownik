idf_component_register(SRCS "app_manager.c" "network_manager.c"
                    INCLUDE_DIRS "." 
                    REQUIRES config drivers utils)
