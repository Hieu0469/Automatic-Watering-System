/**
 * @file sms_v1.h
 * @brief Thư viện đọc cảm biến độ ẩm đất SMS V1 (LM393) cho ESP32 IDF v5.x
 *
 * Nguyên lý hoạt động:
 *   - Hai que đo cắm vào đất tạo thành một điện trở thay đổi theo độ ẩm.
 *   - Đất ẩm  → điện trở thấp  → điện áp A0 thấp  → ADC raw thấp
 *   - Đất khô → điện trở cao   → điện áp A0 cao   → ADC raw cao
 *   - LM393 so sánh điện áp A0 với ngưỡng đặt bởi biến trở:
 *       + Ẩm hơn ngưỡng → D0 = LOW  (0)
 *       + Khô hơn ngưỡng → D0 = HIGH (1)
 *
 * Sơ đồ kết nối:
 *   SMS V1 VCC  ->  ESP32 3.3V (hoặc 5V nếu dùng level-shifter)
 *   SMS V1 GND  ->  ESP32 GND
 *   SMS V1 D0   ->  GPIO bất kỳ (digital input)
 *   SMS V1 A0   ->  GPIO thuộc ADC1 (GPIO32–GPIO39)
 *
 * Gợi ý mặc định:
 *   D0  ->  GPIO5
 *   A0  ->  GPIO34  (ADC1_CH6)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Mặc định                                                           */
/* ------------------------------------------------------------------ */

#define SMS_V1_DEFAULT_D0_GPIO    GPIO_NUM_5
#define SMS_V1_DEFAULT_A0_UNIT    ADC_UNIT_1
#define SMS_V1_DEFAULT_A0_CHANNEL ADC_CHANNEL_6   /**< GPIO34 */

/**
 * @brief Số mẫu ADC lấy trung bình (giảm nhiễu)
 *        Tăng lên nếu môi trường nhiễu nhiều, giảm nếu cần tốc độ.
 */
#define SMS_V1_ADC_SAMPLES        16

/**
 * @brief Ngưỡng mặc định cho phân loại mức ẩm (0–4095)
 *        Hiệu chỉnh lại bằng thực nghiệm với loại đất cụ thể.
 */
#define SMS_V1_THRESHOLD_WET      1200   /**< raw <= giá trị này: ẩm       */
#define SMS_V1_THRESHOLD_DRY      2800   /**< raw >= giá trị này: khô      */
                                         /**< ở giữa: độ ẩm vừa phải      */

/* ------------------------------------------------------------------ */
/*  Enum mức độ ẩm                                                     */
/* ------------------------------------------------------------------ */

/**
 * @brief Phân loại mức độ ẩm đất
 */
typedef enum {
    SMS_V1_MOISTURE_WET    = 0,  /**< Đất ẩm (cần thoát nước)            */
    SMS_V1_MOISTURE_OK     = 1,  /**< Độ ẩm tốt                          */
    SMS_V1_MOISTURE_DRY    = 2,  /**< Đất khô (cần tưới nước)            */
} sms_v1_moisture_level_t;

/* ------------------------------------------------------------------ */
/*  Kiểu dữ liệu                                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief Kết quả đọc từ SMS V1
 */
typedef struct {
    int                     adc_raw;        /**< Giá trị ADC thô (0–4095)  */
    int                     adc_mv;         /**< Điện áp A0 (mV)           */
    uint8_t                 moisture_pct;   /**< Ước tính độ ẩm (0–100%)   */
    bool                    d0_wet;         /**< true = ẩm (D0=LOW)        */
    sms_v1_moisture_level_t level;          /**< Phân loại mức ẩm          */
} sms_v1_data_t;

/**
 * @brief Cấu hình ngưỡng phân loại
 */
typedef struct {
    int threshold_wet;  /**< raw ADC <= giá trị này → WET  (mặc định 1200) */
    int threshold_dry;  /**< raw ADC >= giá trị này → DRY  (mặc định 2800) */
} sms_v1_thresholds_t;

/**
 * @brief Cấu hình driver SMS V1
 */
typedef struct {
    gpio_num_t          d0_gpio;    /**< GPIO chân D0                      */
    adc_unit_t          a0_unit;    /**< ADC unit (khuyến nghị ADC_UNIT_1) */
    adc_channel_t       a0_channel; /**< ADC channel tương ứng GPIO A0     */
    sms_v1_thresholds_t thresholds; /**< Ngưỡng phân loại, {0,0} = mặc định*/
} sms_v1_config_t;

/** Handle (opaque) */
typedef struct sms_v1_dev_t *sms_v1_handle_t;

/* ------------------------------------------------------------------ */
/*  API                                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief Khởi tạo driver SMS V1
 *
 * @param config     Cấu hình, truyền NULL để dùng mặc định hoàn toàn
 * @param out_handle Nhận handle
 * @return ESP_OK nếu thành công
 */
esp_err_t sms_v1_init(const sms_v1_config_t *config,
                      sms_v1_handle_t       *out_handle);

/**
 * @brief Giải phóng tài nguyên driver
 *
 * @param handle Handle đã khởi tạo
 * @return ESP_OK nếu thành công
 */
esp_err_t sms_v1_deinit(sms_v1_handle_t handle);

/**
 * @brief Đọc trạng thái digital từ chân D0
 *
 * D0 = LOW  → đất ẩm hơn ngưỡng biến trở  → d0_wet = true
 * D0 = HIGH → đất khô hơn ngưỡng biến trở → d0_wet = false
 *
 * @param handle  Handle đã khởi tạo
 * @param data    Con trỏ nhận dữ liệu (chỉ cập nhật trường d0_wet)
 * @return ESP_OK nếu thành công
 */
esp_err_t sms_v1_read_digital(sms_v1_handle_t handle, sms_v1_data_t *data);

/**
 * @brief Đọc giá trị analog từ chân A0
 *
 * Trung bình SMS_V1_ADC_SAMPLES mẫu, có ADC calibration.
 * Tính toán moisture_pct và level từ ngưỡng cấu hình.
 *
 * @param handle  Handle đã khởi tạo
 * @param data    Con trỏ nhận dữ liệu
 * @return ESP_OK nếu thành công
 */
esp_err_t sms_v1_read_analog(sms_v1_handle_t handle, sms_v1_data_t *data);

/**
 * @brief Đọc cả D0 và A0 cùng lúc
 *
 * @param handle  Handle đã khởi tạo
 * @param data    Con trỏ nhận dữ liệu đầy đủ
 * @return ESP_OK nếu cả hai đều thành công
 */
esp_err_t sms_v1_read_all(sms_v1_handle_t handle, sms_v1_data_t *data);

/**
 * @brief Cập nhật ngưỡng phân loại sau khi khởi tạo (hiệu chỉnh thực tế)
 *
 * @param handle      Handle đã khởi tạo
 * @param thresholds  Ngưỡng mới
 * @return ESP_OK nếu thành công
 */
esp_err_t sms_v1_set_thresholds(sms_v1_handle_t            handle,
                                const sms_v1_thresholds_t *thresholds);

/**
 * @brief Lấy ngưỡng hiện tại
 *
 * @param handle      Handle đã khởi tạo
 * @param thresholds  Nhận ngưỡng hiện tại
 * @return ESP_OK nếu thành công
 */
esp_err_t sms_v1_get_thresholds(sms_v1_handle_t      handle,
                                sms_v1_thresholds_t *thresholds);

/**
 * @brief Lấy tên chuỗi mức độ ẩm
 *
 * @param level  Mức độ ẩm
 * @return Chuỗi mô tả (không được free)
 */
const char *sms_v1_level_str(sms_v1_moisture_level_t level);

/**
 * @brief In kết quả ra console (ESP_LOGI)
 *
 * @param data  Con trỏ đến dữ liệu đã đọc
 */
void sms_v1_print(const sms_v1_data_t *data);

#ifdef __cplusplus
}
#endif