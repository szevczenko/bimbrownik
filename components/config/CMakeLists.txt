idf_component_register(SRCS "config.c" "dev_config.c"
                    INCLUDE_DIRS "." 
                    REQUIRES ${requires} nvs_flash)
