idf_component_register(SRCS "wifidrv.c"
                    INCLUDE_DIRS "." 
                    REQUIRES application config project_hal utils hal)
