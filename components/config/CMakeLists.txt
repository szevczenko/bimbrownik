idf_component_register(SRCS "config.c"
                    INCLUDE_DIRS "." 
                    REQUIRES ${requires} nvs_flash)
