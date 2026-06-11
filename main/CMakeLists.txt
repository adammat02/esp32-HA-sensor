idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES wifi_manager mqtt_manager sht31_driver nvs_flash)
