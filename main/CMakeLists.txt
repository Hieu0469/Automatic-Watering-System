idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES tft_st7735 pwm_motor DHT driver esp_driver_uart nvs_flash esp_event freertos sms_v1 esp_common log esp_timer esp_adc
                   )
