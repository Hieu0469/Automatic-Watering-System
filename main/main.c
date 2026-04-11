#include <stdio.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_system.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "esp_adc/adc_oneshot.h"

#include "DHT.h"
#include "pwm_motor.h"
#include "tft_st7735.h"
#include "sms_v1.h"

int mutex = 1;
float water_time = 0.0;

static const char *TAG = "DHT";

#define UART_NUM UART_NUM_0
#define GPIO_OUTPUT_PIN GPIO_NUM_4
#define DHT_GPIO GPIO_NUM_16
#define TH118_GPIO GPIO_NUM_13
#define BUF_SIZE 1024
#define WATER_HIGH 2
#define WATER_LOW 1
#define WATER_NONE 0
#define MAX_WATER_TIME 2000 // Maximum watering time in milliseconds


float defuzzify(float temp, float hum, float soil_moisture);
float low_soil_moisture_membership(float soil_moisture);
float medium_soil_moisture_membership(float soil_moisture);
float high_soil_moisture_membership(float soil_moisture);
float hot_temp_membership(float temp);
float medium_temp_membership(float temp);
float cold_temp_membership(float temp);
float low_hum_membership(float hum);
float medium_hum_membership(float hum);
float high_hum_membership(float hum);
float fmaxf(float a, float b) {
    return (a > b) ? a : b;
}
float fminf(float a, float b) {
    return (a < b) ? a : b;
}   


int rule_matrix_mois_low[3][3] = {
    {2, 2, 2}, 
    {2, 2, 1}, 
    {2, 1, 1}  
};
int rule_matrix_mois_med[3][3] = {
    {2, 1, 1}, 
    {1, 1, 0}, 
    {1, 0, 0}  
};
int rule_matrix_mois_high[3][3] = {
    {1, 0, 0}, 
    {0, 0, 0}, 
    {0, 0, 0}  
};

float defuzzify(float temp, float hum, float soil_moisture) {
    float temp_lvl[3] = {hot_temp_membership(temp), medium_temp_membership(temp), cold_temp_membership(temp)};
    float hum_lvl[3] = {low_hum_membership(hum), medium_hum_membership(hum), high_hum_membership(hum)};
    float soil_moisture_lvl[3] = {low_soil_moisture_membership(soil_moisture), medium_soil_moisture_membership(soil_moisture), high_soil_moisture_membership(soil_moisture)};


    float water_high = 0.0;
    float water_low = 0.0;
    float water_none = 0.0;

    ESP_LOGI("Fuzzy Logic", "Hot: %.2f, Medium: %.2f, Cold: %.2f", temp_lvl[0], temp_lvl[1], temp_lvl[2]);
    ESP_LOGI("Fuzzy Logic", "Low Hum: %.2f, Medium Hum: %.2f, High Hum: %.2f", hum_lvl[0], hum_lvl[1], hum_lvl[2]);
    ESP_LOGI("Fuzzy Logic", "Low Moisture: %.2f, Medium Moisture: %.2f, High Moisture: %.2f", soil_moisture_lvl[0], soil_moisture_lvl[1], soil_moisture_lvl[2]);

    for(int i = 0 ; i < 3 ; i++) {
        for(int j = 0 ; j < 3 ; j++) {
            if(rule_matrix_mois_low[i][j] == WATER_HIGH) {
                water_high = fmaxf(water_high, fminf(fminf(temp_lvl[j], hum_lvl[i]), soil_moisture_lvl[0]));
            } else if(rule_matrix_mois_low[i][j] == WATER_LOW) {
                water_low = fmaxf(water_low, fminf(fminf(temp_lvl[j], hum_lvl[i]), soil_moisture_lvl[0]));
            } else {
                water_none = fmaxf(water_none, fminf(fminf(temp_lvl[j], hum_lvl[i]), soil_moisture_lvl[0]));
            }
        }
    }

    for(int i = 0 ; i < 3 ; i++) {
        for(int j = 0 ; j < 3 ; j++) {
            if(rule_matrix_mois_med[i][j] == WATER_HIGH) {
                water_high = fmaxf(water_high, fminf(fminf(temp_lvl[j], hum_lvl[i]), soil_moisture_lvl[1]));
            } else if(rule_matrix_mois_med[i][j] == WATER_LOW) {
                water_low = fmaxf(water_low, fminf(fminf(temp_lvl[j], hum_lvl[i]), soil_moisture_lvl[1]));
            } else {
                water_none = fmaxf(water_none, fminf(fminf(temp_lvl[j], hum_lvl[i]), soil_moisture_lvl[1]));
            }
        }
    }

    for(int i = 0 ; i < 3 ; i++) {
        for(int j = 0 ; j < 3 ; j++) {
            if(rule_matrix_mois_high[i][j] == WATER_HIGH) {
                water_high = fmaxf(water_high, fminf(fminf(temp_lvl[j], hum_lvl[i]), soil_moisture_lvl[2]));
            } else if(rule_matrix_mois_high[i][j] == WATER_LOW) {
                water_low = fmaxf(water_low, fminf(fminf(temp_lvl[j], hum_lvl[i]), soil_moisture_lvl[2]));
            } else {
                water_none = fmaxf(water_none, fminf(fminf(temp_lvl[j], hum_lvl[i]), soil_moisture_lvl[2]));
            }
        }
    }
    if(water_high + water_low + water_none == 0) {
            return 0; // Avoid division by zero
    }
    ESP_LOGI("Fuzzy Logic", "Water High: %.2f, Water Low: %.2f, Water None: %.2f", water_high, water_low, water_none);
    return (water_high * 100.0 + water_low * 50.0 + water_none * 0.0) / (water_high + water_low + water_none);
}

float low_soil_moisture_membership(float soil_moisture) {
    if (soil_moisture <= 30.0) {
        return 1.0;
    } else if (soil_moisture >= 50.0) {
        return 0.0;
    } else {
        return (50.0 - soil_moisture) / 20.0; // Linear interpolation between 30 and 50
    }
}
float medium_soil_moisture_membership(float soil_moisture) {
    if (soil_moisture <= 30.0 || soil_moisture >= 70.0) {
        return 0.0;
    }
    else if(soil_moisture > 30 && soil_moisture < 50) {
        return (soil_moisture - 30.0) / 20.0; // Linear interpolation between 30 and 50
    } else {
        return (70.0 - soil_moisture) / 20.0; // Linear interpolation between 50 and 70
    }
}
float high_soil_moisture_membership(float soil_moisture) {
    if (soil_moisture <= 50.0) {
        return 0.0;
    } else if (soil_moisture >= 70.0) {
        return 1.0;
    } else {
        return (soil_moisture - 50.0) / 20.0; // Linear interpolation between 50 and 70
    }
}


float hot_temp_membership(float temp) {
    if (temp <= 25.0) {
        return 0.0;
    } else if (temp >= 30.0) {
        return 1.0;
    } else {
        return (temp - 25.0) / 5.0; // Linear interpolation between 25 and 30
    }
}
float cold_temp_membership(float temp) {
    if (temp <= 20.0) {
        return 1.0;
    } else if (temp >= 25.0) {
        return 0.0;
    } else {
        return (25.0 - temp) / 5.0; // Linear interpolation between 20 and 25
    }
}
float medium_temp_membership(float temp) {
    if (temp <= 20.0 || temp >= 30.0) {
        return 0.0;
    }
    else if(temp > 20 && temp < 25) {
        return (temp - 20.0) / 5.0; // Linear interpolation between 20 and 25
    } else {
        return (30.0 - temp) / 5.0; // Linear interpolation between 25 and 30
    }
}
float low_hum_membership(float hum) {
    if (hum <= 40.0) {
        return 1.0;
    } else if (hum >= 60.0) {
        return 0.0;
    } else {
        return (60.0 - hum) / 20.0; // Linear interpolation between 40 and 60
    }
}
float medium_hum_membership(float hum) {
    if (hum <= 40.0 || hum >= 80.0) {
        return 0.0;
    }
    else if(hum > 40 && hum < 60) {
        return (hum - 40.0) / 20.0; // Linear interpolation between 40 and 60
    } else {
        return (80.0 - hum) / 20.0; // Linear interpolation between 60 and 80
    }
}
float high_hum_membership(float hum) {
    if (hum <= 60.0) {
        return 0.0;
    } else if (hum >= 80.0) {
        return 1.0;
    } else {
        return (hum - 60.0) / 20.0; // Linear interpolation between 60 and 80
    }
}


void Sensor_task(void *pvParameter)
{
    setDHTgpio(GPIO_NUM_15);
    sms_v1_data_t data;
    sms_v1_config_t cfg = {
        .d0_gpio    = GPIO_NUM_5,
        .a0_unit    = ADC_UNIT_1,
        .a0_channel = ADC_CHANNEL_6,   /* GPIO34 */
        /* Ngưỡng mặc định, hiệu chỉnh sau khi đo thực tế:
         *   threshold_wet: giá trị raw khi đất ướt nhất
         *   threshold_dry: giá trị raw khi đất khô hoàn toàn         */
        .thresholds = {
            .threshold_wet = 1200,
            .threshold_dry = 2800,
        },
    };
    sms_v1_handle_t sensor = NULL;
    ESP_ERROR_CHECK(sms_v1_init(&cfg, &sensor));
    while (1)
    {
        int ret = readDHT();
        errorHandler(ret);
        ESP_LOGI("Sensor Task", "--------------------------------------------");
        if(sms_v1_read_all(sensor, &data) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read soil moisture data");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }
        float temp = getTemperature();
        float hum = getHumidity();
        float soil_moisture = data.moisture_pct; // Implement this function to read soil moisture

        water_time = defuzzify(temp, hum, soil_moisture) * MAX_WATER_TIME / 100.0; // Scale to max watering time
        ESP_LOGI("Sensor Task", "Temp: %.1f, Hum: %.1f, Soil Moisture: %.1f, Water Time: %.1f ms", temp, hum, soil_moisture, water_time);
        

        mutex = 0;
        vTaskDelay(10000 / portTICK_PERIOD_MS); // Wait 10 seconds before next watering}
    }
}
void Watering_task(void *pvParameter)
{
    gpio_set_direction(GPIO_OUTPUT_PIN, GPIO_MODE_OUTPUT);
    while(1) {
        if(mutex == 0) {
            ESP_LOGI("Watering", "Starting watering for %.1f ms", water_time);
            gpio_set_level(GPIO_OUTPUT_PIN, 1);
            vTaskDelay((int)(water_time / portTICK_PERIOD_MS)); // Convert ms to ticks
            gpio_set_level(GPIO_OUTPUT_PIN, 0);
            ESP_LOGI("Watering", "Watering done"); 
            mutex = 1; // Set mutex to 1 to indicate watering is done
        }
        vTaskDelay(100 / portTICK_PERIOD_MS); // Check every second
    }
}


void app_main()
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);



    xTaskCreate(Sensor_task, "Sensor Task", 4096, NULL, 1, NULL);
    xTaskCreate(Watering_task, "Watering Task", 4096, NULL, 2, NULL);

}