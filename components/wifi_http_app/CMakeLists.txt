idf_component_register(SRC_DIRS "src"
                    INCLUDE_DIRS "src"
                    EMBED_FILES src/style.css src/code.js src/index.html
                    PRIV_REQUIRES main mongoose_drv nvs_flash)
