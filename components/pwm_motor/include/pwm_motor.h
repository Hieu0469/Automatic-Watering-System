#ifndef PWM_MOTOR_H
#define PWM_MOTOR_H

#include <stdint.h>

/**
 * @brief Initialize PWM motor controller
 * @param gpio_pin GPIO pin number for PWM output
 * @param frequency PWM frequency in Hz (typical: 1000-5000 Hz)
 * @param max_duty Maximum duty cycle value (for 13-bit: 8191)
 */
void pwm_motor_init(int gpio_pin, int frequency, int max_duty);

/**
 * @brief Set motor speed via PWM duty cycle
 * @param duty_cycle Duty cycle value (0 to max_duty)
 *                   0 = 0% speed (stopped)
 *                   max_duty = 100% speed (full)
 */
void pwm_motor_set_speed(uint32_t duty_cycle);

/**
 * @brief Stop motor (set duty cycle to 0)
 */
void pwm_motor_stop(void);

/**
 * @brief Set motor speed as percentage (0-100%)
 * @param percentage Speed as percentage (0-100)
 */
void pwm_motor_set_speed_percent(uint8_t percentage);

#endif
