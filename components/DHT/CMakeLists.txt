idf_component_register(SRCS "DHT.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver log esp_driver_gpio
                   )
