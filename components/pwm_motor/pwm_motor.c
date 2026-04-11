#include <stdio.h>
#include "pwm_motor.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "PWM_MOTOR";

// LEDC configuration
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT  // 13-bit resolution (0-8191)

static uint32_t max_duty_value = 8191;  // Store max duty for percentage conversion

void pwm_motor_init(int gpio_pin, int frequency, int max_duty)
{
    max_duty_value = max_duty;

    // Configure LEDC timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = frequency,
        .clk_cfg = LEDC_AUTO_CLK
    };
    
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
        return;
    }

    // Configure LEDC channel
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = gpio_pin,
        .duty = 0,  // Start at 0% duty cycle
        .hpoint = 0
    };
    
    ret = ledc_channel_config(&ledc_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "PWM Motor initialized on GPIO %d at %d Hz", gpio_pin, frequency);
}

void pwm_motor_set_speed(uint32_t duty_cycle)
{
    // Clamp duty cycle to max value
    if (duty_cycle > max_duty_value) {
        duty_cycle = max_duty_value;
    }

    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty_cycle);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

void pwm_motor_stop(void)
{
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    ESP_LOGI(TAG, "Motor stopped");
}

void pwm_motor_set_speed_percent(uint8_t percentage)
{
    // Clamp percentage to 0-100
    if (percentage > 100) {
        percentage = 100;
    }

    // Convert percentage to duty cycle value
    uint32_t duty_cycle = (percentage * max_duty_value) / 100;
    
    pwm_motor_set_speed(duty_cycle);
    ESP_LOGI(TAG, "Motor speed set to %d%%", percentage);
}
