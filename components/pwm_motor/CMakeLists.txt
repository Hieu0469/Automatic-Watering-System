idf_component_register(SRCS "pwm_motor.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver log esp_driver_ledc)
