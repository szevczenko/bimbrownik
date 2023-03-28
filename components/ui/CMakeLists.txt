idf_component_register(SRCS "ui.c" "screen.c" "ui_helpers.c"
                    INCLUDE_DIRS "." 
                    REQUIRES lvgl lvgl_esp32_drivers)
