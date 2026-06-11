idf_component_register(SRCS "src/wifi_manager.c"
                       INCLUDE_DIRS "inc"
                       REQUIRES esp_wifi esp_event esp_netif nvs_flash)
