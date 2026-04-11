/**
 * @file sms_v1.c
 * @brief Triển khai thư viện SMS V1 (LM393 Soil Moisture Sensor) – ESP-IDF v5.x
 *
 * Nguyên lý đọc:
 *
 *  [D0] – Digital output của LM393:
 *    LM393 so sánh điện áp que đo (V_soil) với ngưỡng biến trở (V_ref):
 *      V_soil < V_ref  (ẩm)  →  D0 = LOW
 *      V_soil > V_ref  (khô) →  D0 = HIGH
 *    GPIO đọc mức logic trực tiếp, không cần ADC.
 *
 *  [A0] – Analog output trực tiếp từ cầu phân áp que đo:
 *    Đất ẩm  → điện trở thấp  → A0 thấp  → ADC raw thấp
 *    Đất khô → điện trở cao   → A0 cao   → ADC raw cao
 *    Lấy trung bình nhiều mẫu để giảm nhiễu điện hóa từ que đo.
 *
 *  [moisture_pct] – Ước tính phần trăm độ ẩm:
 *    Map ngược ADC raw: raw cao = khô = 0%, raw thấp = ẩm = 100%
 *    Dùng threshold_wet và threshold_dry làm mốc 100% và 0%.
 */

#include "sms_v1.h"

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SMS_V1";

/* ------------------------------------------------------------------ */
/*  Cấu trúc nội bộ                                                    */
/* ------------------------------------------------------------------ */

struct sms_v1_dev_t {
    sms_v1_config_t           config;
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t         cali_handle;
    bool                      cali_enabled;
};

/* ------------------------------------------------------------------ */
/*  Nội bộ: ADC Calibration                                            */
/* ------------------------------------------------------------------ */

static bool _adc_cali_init(adc_unit_t unit,
                            adc_channel_t channel,
                            adc_atten_t atten,
                            adc_cali_handle_t *out_handle)
{
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cfg = {
        .unit_id  = unit,
        .chan     = channel,
        .atten    = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cfg, out_handle) == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration: Curve Fitting");
        calibrated = true;
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        adc_cali_line_fitting_config_t cfg = {
            .unit_id  = unit,
            .atten    = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        if (adc_cali_create_scheme_line_fitting(&cfg, out_handle) == ESP_OK) {
            ESP_LOGI(TAG, "ADC calibration: Line Fitting");
            calibrated = true;
        }
    }
#endif

    if (!calibrated) {
        ESP_LOGW(TAG, "ADC calibration không hỗ trợ, dùng raw value");
    }
    return calibrated;
}

static void _adc_cali_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_delete_scheme_curve_fitting(handle);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_delete_scheme_line_fitting(handle);
#endif
}

/* ------------------------------------------------------------------ */
/*  Nội bộ: Tính moisture_pct và level từ raw ADC                     */
/* ------------------------------------------------------------------ */

/**
 * Map ngược: raw thấp = ẩm = 100%, raw cao = khô = 0%
 * Clamp về [0, 100].
 */
static uint8_t _calc_moisture_pct(int raw,
                                   int threshold_wet,
                                   int threshold_dry)
{
    if (raw <= threshold_wet) return 100;
    if (raw >= threshold_dry) return 0;

    /* Nội suy tuyến tính trong khoảng [wet, dry] */
    int range = threshold_dry - threshold_wet;
    int offset = raw - threshold_wet;
    int pct = 100 - (offset * 100 / range);

    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    return (uint8_t)pct;
}

static sms_v1_moisture_level_t _calc_level(int raw,
                                            int threshold_wet,
                                            int threshold_dry)
{
    if (raw <= threshold_wet) return SMS_V1_MOISTURE_WET;
    if (raw >= threshold_dry) return SMS_V1_MOISTURE_DRY;
    return SMS_V1_MOISTURE_OK;
}

/* ------------------------------------------------------------------ */
/*  API công khai                                                       */
/* ------------------------------------------------------------------ */

esp_err_t sms_v1_init(const sms_v1_config_t *config,
                      sms_v1_handle_t       *out_handle)
{
    if (!out_handle) return ESP_ERR_INVALID_ARG;

    struct sms_v1_dev_t *dev = calloc(1, sizeof(struct sms_v1_dev_t));
    if (!dev) return ESP_ERR_NO_MEM;

    /* Áp dụng config hoặc mặc định */
    if (config) {
        dev->config = *config;
    } else {
        dev->config.d0_gpio    = SMS_V1_DEFAULT_D0_GPIO;
        dev->config.a0_unit    = SMS_V1_DEFAULT_A0_UNIT;
        dev->config.a0_channel = SMS_V1_DEFAULT_A0_CHANNEL;
    }

    /* Điền ngưỡng mặc định nếu chưa set */
    if (dev->config.thresholds.threshold_wet == 0 &&
        dev->config.thresholds.threshold_dry == 0) {
        dev->config.thresholds.threshold_wet = SMS_V1_THRESHOLD_WET;
        dev->config.thresholds.threshold_dry = SMS_V1_THRESHOLD_DRY;
    }

    /* --- Cấu hình GPIO D0: input với pull-up nội ------------------- */
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << dev->config.d0_gpio),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,   /* LM393 open-collector */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi cấu hình GPIO D0: %s", esp_err_to_name(ret));
        free(dev);
        return ret;
    }

    /* --- Khởi tạo ADC oneshot unit --------------------------------- */
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id  = dev->config.a0_unit,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ret = adc_oneshot_new_unit(&unit_cfg, &dev->adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi khởi tạo ADC unit: %s", esp_err_to_name(ret));
        free(dev);
        return ret;
    }

    /* --- Cấu hình channel ADC -------------------------------------- */
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_ATTEN_DB_12,      /* 0 ~ 3.9V */
        .bitwidth = ADC_BITWIDTH_DEFAULT, /* 12-bit    */
    };
    ret = adc_oneshot_config_channel(dev->adc_handle,
                                     dev->config.a0_channel,
                                     &chan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi cấu hình ADC channel: %s", esp_err_to_name(ret));
        adc_oneshot_del_unit(dev->adc_handle);
        free(dev);
        return ret;
    }

    /* --- Calibration ADC (không bắt buộc) -------------------------- */
    dev->cali_enabled = _adc_cali_init(dev->config.a0_unit,
                                        dev->config.a0_channel,
                                        ADC_ATTEN_DB_12,
                                        &dev->cali_handle);

    *out_handle = dev;

    ESP_LOGI(TAG, "Khởi tạo OK");
    ESP_LOGI(TAG, "  D0  = GPIO%d", dev->config.d0_gpio);
    ESP_LOGI(TAG, "  A0  = ADC%d CH%d",
             dev->config.a0_unit + 1, dev->config.a0_channel);
    ESP_LOGI(TAG, "  Ngưỡng WET=%d  DRY=%d",
             dev->config.thresholds.threshold_wet,
             dev->config.thresholds.threshold_dry);
    ESP_LOGI(TAG, "  Calibration: %s", dev->cali_enabled ? "có" : "không");

    return ESP_OK;
}

/* ------------------------------------------------------------------ */

esp_err_t sms_v1_deinit(sms_v1_handle_t handle)
{
    if (!handle) return ESP_ERR_INVALID_ARG;
    if (handle->cali_enabled) {
        _adc_cali_deinit(handle->cali_handle);
    }
    adc_oneshot_del_unit(handle->adc_handle);
    free(handle);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */

esp_err_t sms_v1_read_digital(sms_v1_handle_t handle, sms_v1_data_t *data)
{
    if (!handle || !data) return ESP_ERR_INVALID_ARG;

    /*
     * LM393 có ngõ ra open-collector:
     *   D0 = LOW  → transistor dẫn → đất ẩm hơn ngưỡng
     *   D0 = HIGH → transistor ngắt → đất khô hơn ngưỡng
     */
    int level = gpio_get_level(handle->config.d0_gpio);
    data->d0_wet = (level == 0);

    ESP_LOGD(TAG, "D0 level=%d (%s)", level, data->d0_wet ? "ẨM" : "KHÔ");
    return ESP_OK;
}

/* ------------------------------------------------------------------ */

esp_err_t sms_v1_read_analog(sms_v1_handle_t handle, sms_v1_data_t *data)
{
    if (!handle || !data) return ESP_ERR_INVALID_ARG;

    /* Lấy trung bình SMS_V1_ADC_SAMPLES mẫu */
    int64_t sum = 0;
    for (int i = 0; i < SMS_V1_ADC_SAMPLES; i++) {
        int sample = 0;
        esp_err_t ret = adc_oneshot_read(handle->adc_handle,
                                         handle->config.a0_channel,
                                         &sample);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Lỗi đọc ADC mẫu %d: %s", i, esp_err_to_name(ret));
            return ret;
        }
        sum += sample;
        esp_rom_delay_us(500); /* Chờ giữa các mẫu, giảm tự nhiễu điện hóa */
    }
    data->adc_raw = (int)(sum / SMS_V1_ADC_SAMPLES);

    /* Chuyển sang mV */
    if (handle->cali_enabled) {
        esp_err_t ret = adc_cali_raw_to_voltage(handle->cali_handle,
                                                 data->adc_raw,
                                                 &data->adc_mv);
        if (ret != ESP_OK) {
            /* Fallback tính thô: ATTEN_DB_12 → Vmax ~3900mV */
            data->adc_mv = (int)((int64_t)data->adc_raw * 3900 / 4095);
        }
    } else {
        data->adc_mv = (int)((int64_t)data->adc_raw * 3900 / 4095);
    }

    /* Tính moisture_pct và level */
    data->moisture_pct = _calc_moisture_pct(data->adc_raw,
                                             handle->config.thresholds.threshold_wet,
                                             handle->config.thresholds.threshold_dry);
    data->level = _calc_level(data->adc_raw,
                               handle->config.thresholds.threshold_wet,
                               handle->config.thresholds.threshold_dry);

    ESP_LOGD(TAG, "A0 raw=%d mv=%d pct=%d%% level=%s",
             data->adc_raw, data->adc_mv, data->moisture_pct,
             sms_v1_level_str(data->level));

    return ESP_OK;
}

/* ------------------------------------------------------------------ */

esp_err_t sms_v1_read_all(sms_v1_handle_t handle, sms_v1_data_t *data)
{
    if (!handle || !data) return ESP_ERR_INVALID_ARG;
    memset(data, 0, sizeof(*data));

    esp_err_t ret_d = sms_v1_read_digital(handle, data);
    esp_err_t ret_a = sms_v1_read_analog(handle, data);

    if (ret_d != ESP_OK) ESP_LOGW(TAG, "D0 thất bại: %s", esp_err_to_name(ret_d));
    if (ret_a != ESP_OK) ESP_LOGW(TAG, "A0 thất bại: %s", esp_err_to_name(ret_a));

    return (ret_d == ESP_OK && ret_a == ESP_OK) ? ESP_OK : ESP_FAIL;
}

/* ------------------------------------------------------------------ */

esp_err_t sms_v1_set_thresholds(sms_v1_handle_t            handle,
                                const sms_v1_thresholds_t *thresholds)
{
    if (!handle || !thresholds) return ESP_ERR_INVALID_ARG;
    if (thresholds->threshold_wet >= thresholds->threshold_dry) {
        ESP_LOGE(TAG, "threshold_wet phải nhỏ hơn threshold_dry");
        return ESP_ERR_INVALID_ARG;
    }
    handle->config.thresholds = *thresholds;
    ESP_LOGI(TAG, "Cập nhật ngưỡng: WET=%d  DRY=%d",
             thresholds->threshold_wet, thresholds->threshold_dry);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */

esp_err_t sms_v1_get_thresholds(sms_v1_handle_t      handle,
                                sms_v1_thresholds_t *thresholds)
{
    if (!handle || !thresholds) return ESP_ERR_INVALID_ARG;
    *thresholds = handle->config.thresholds;
    return ESP_OK;
}

/* ------------------------------------------------------------------ */

const char *sms_v1_level_str(sms_v1_moisture_level_t level)
{
    switch (level) {
        case SMS_V1_MOISTURE_WET: return "ẨM (cần thoát nước)";
        case SMS_V1_MOISTURE_OK:  return "VỪA ĐỦ";
        case SMS_V1_MOISTURE_DRY: return "KHÔ (cần tưới)";
        default:                  return "KHÔNG XÁC ĐỊNH";
    }
}

/* ------------------------------------------------------------------ */

void sms_v1_print(const sms_v1_data_t *data)
{
    if (!data) return;
    ESP_LOGI(TAG, "+------------------------------------+");
    ESP_LOGI(TAG, "|  ADC raw     : %5d              |", data->adc_raw);
    ESP_LOGI(TAG, "|  ADC voltage : %5d mV           |", data->adc_mv);
    ESP_LOGI(TAG, "|  Do am       : %5d %%            |", data->moisture_pct);
    ESP_LOGI(TAG, "|  D0 wet      : %s               |", data->d0_wet ? "CO " : "KHONG");
    ESP_LOGI(TAG, "|  Muc do      : %s", sms_v1_level_str(data->level));
    ESP_LOGI(TAG, "+------------------------------------+");
}