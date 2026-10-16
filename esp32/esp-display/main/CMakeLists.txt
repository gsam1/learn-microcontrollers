idf_component_register(SRCS "main.c" "board.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_lcd esp_driver_i2c esp_driver_gpio esp_wifi esp_netif
                                     esp_event nvs_flash esp_http_client esp_psram esp_timer)
