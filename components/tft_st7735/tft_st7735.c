/*
 * tft_st7735.c  –  ST7735S 1.77" TFT driver for ESP32-IDF
 *
 * Pin mapping passed via tft_config_t:
 *   SCK  → SPI clock
 *   SDA  → SPI MOSI
 *   RES  → Hardware reset (active-low)
 *   RS   → Data / Command select  (low = command, high = data)
 *   CS   → Chip select (active-low)
 *   LEDA → Backlight LED anode (drive high to enable)
 */

#include "tft_st7735.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "tft_st7735";

/* ─────────────────────────────────────────────
 *  Built-in 5×7 font (ASCII 0x20–0x7E)
 *  Each character = 5 bytes (columns), MSB = top row
 * ───────────────────────────────────────────── */
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* 0x20  */
    {0x00,0x00,0x5F,0x00,0x00}, /* 0x21 ! */
    {0x00,0x07,0x00,0x07,0x00}, /* 0x22 " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* 0x23 # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* 0x24 $ */
    {0x23,0x13,0x08,0x64,0x62}, /* 0x25 % */
    {0x36,0x49,0x55,0x22,0x50}, /* 0x26 & */
    {0x00,0x05,0x03,0x00,0x00}, /* 0x27 ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* 0x28 ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* 0x29 ) */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* 0x2A * */
    {0x08,0x08,0x3E,0x08,0x08}, /* 0x2B + */
    {0x00,0x50,0x30,0x00,0x00}, /* 0x2C , */
    {0x08,0x08,0x08,0x08,0x08}, /* 0x2D - */
    {0x00,0x60,0x60,0x00,0x00}, /* 0x2E . */
    {0x20,0x10,0x08,0x04,0x02}, /* 0x2F / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0x30 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 0x31 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 0x32 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 0x33 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 0x34 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 0x35 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 0x36 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 0x37 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 0x38 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 0x39 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* 0x3A : */
    {0x00,0x56,0x36,0x00,0x00}, /* 0x3B ; */
    {0x00,0x08,0x14,0x22,0x41}, /* 0x3C < */
    {0x14,0x14,0x14,0x14,0x14}, /* 0x3D = */
    {0x41,0x22,0x14,0x08,0x00}, /* 0x3E > */
    {0x02,0x01,0x51,0x09,0x06}, /* 0x3F ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* 0x40 @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 0x41 A */
    {0x7F,0x49,0x49,0x49,0x36}, /* 0x42 B */
    {0x3E,0x41,0x41,0x41,0x22}, /* 0x43 C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 0x44 D */
    {0x7F,0x49,0x49,0x49,0x41}, /* 0x45 E */
    {0x7F,0x09,0x09,0x01,0x01}, /* 0x46 F */
    {0x3E,0x41,0x41,0x51,0x32}, /* 0x47 G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 0x48 H */
    {0x00,0x41,0x7F,0x41,0x00}, /* 0x49 I */
    {0x20,0x40,0x41,0x3F,0x01}, /* 0x4A J */
    {0x7F,0x08,0x14,0x22,0x41}, /* 0x4B K */
    {0x7F,0x40,0x40,0x40,0x40}, /* 0x4C L */
    {0x7F,0x02,0x04,0x02,0x7F}, /* 0x4D M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 0x4E N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 0x4F O */
    {0x7F,0x09,0x09,0x09,0x06}, /* 0x50 P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 0x51 Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* 0x52 R */
    {0x46,0x49,0x49,0x49,0x31}, /* 0x53 S */
    {0x01,0x01,0x7F,0x01,0x01}, /* 0x54 T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 0x55 U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 0x56 V */
    {0x7F,0x20,0x18,0x20,0x7F}, /* 0x57 W */
    {0x63,0x14,0x08,0x14,0x63}, /* 0x58 X */
    {0x03,0x04,0x78,0x04,0x03}, /* 0x59 Y */
    {0x61,0x51,0x49,0x45,0x43}, /* 0x5A Z */
    {0x00,0x00,0x7F,0x41,0x41}, /* 0x5B [ */
    {0x02,0x04,0x08,0x10,0x20}, /* 0x5C \ */
    {0x41,0x41,0x7F,0x00,0x00}, /* 0x5D ] */
    {0x04,0x02,0x01,0x02,0x04}, /* 0x5E ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* 0x5F _ */
    {0x00,0x01,0x02,0x04,0x00}, /* 0x60 ` */
    {0x20,0x54,0x54,0x54,0x78}, /* 0x61 a */
    {0x7F,0x48,0x44,0x44,0x38}, /* 0x62 b */
    {0x38,0x44,0x44,0x44,0x20}, /* 0x63 c */
    {0x38,0x44,0x44,0x48,0x7F}, /* 0x64 d */
    {0x38,0x54,0x54,0x54,0x18}, /* 0x65 e */
    {0x08,0x7E,0x09,0x01,0x02}, /* 0x66 f */
    {0x08,0x14,0x54,0x54,0x3C}, /* 0x67 g */
    {0x7F,0x08,0x04,0x04,0x78}, /* 0x68 h */
    {0x00,0x44,0x7D,0x40,0x00}, /* 0x69 i */
    {0x20,0x40,0x44,0x3D,0x00}, /* 0x6A j */
    {0x00,0x7F,0x10,0x28,0x44}, /* 0x6B k */
    {0x00,0x41,0x7F,0x40,0x00}, /* 0x6C l */
    {0x7C,0x04,0x18,0x04,0x78}, /* 0x6D m */
    {0x7C,0x08,0x04,0x04,0x78}, /* 0x6E n */
    {0x38,0x44,0x44,0x44,0x38}, /* 0x6F o */
    {0x7C,0x14,0x14,0x14,0x08}, /* 0x70 p */
    {0x08,0x14,0x14,0x18,0x7C}, /* 0x71 q */
    {0x7C,0x08,0x04,0x04,0x08}, /* 0x72 r */
    {0x48,0x54,0x54,0x54,0x20}, /* 0x73 s */
    {0x04,0x3F,0x44,0x40,0x20}, /* 0x74 t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 0x75 u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 0x76 v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 0x77 w */
    {0x44,0x28,0x10,0x28,0x44}, /* 0x78 x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 0x79 y */
    {0x44,0x64,0x54,0x4C,0x44}, /* 0x7A z */
    {0x00,0x08,0x36,0x41,0x00}, /* 0x7B { */
    {0x00,0x00,0x7F,0x00,0x00}, /* 0x7C | */
    {0x00,0x41,0x36,0x08,0x00}, /* 0x7D } */
    {0x08,0x08,0x2A,0x1C,0x08}, /* 0x7E ~ */
};

/* ─────────────────────────────────────────────
 *  Internal device structure
 * ───────────────────────────────────────────── */
struct tft_dev_s {
    spi_device_handle_t spi;
    tft_config_t        cfg;
    tft_rotation_t      rotation;
    uint16_t            width;    /* Current effective width  */
    uint16_t            height;   /* Current effective height */
};

/* ─────────────────────────────────────────────
 *  Low-level SPI helpers
 * ───────────────────────────────────────────── */
static inline void _dc_cmd(tft_handle_t h)  { gpio_set_level(h->cfg.pin_rs, 0); }
static inline void _dc_data(tft_handle_t h) { gpio_set_level(h->cfg.pin_rs, 1); }
static inline void _cs_lo(tft_handle_t h)   { gpio_set_level(h->cfg.pin_cs, 0); }
static inline void _cs_hi(tft_handle_t h)   { gpio_set_level(h->cfg.pin_cs, 1); }

static void _spi_write(tft_handle_t h, const uint8_t *data, size_t len)
{
    if (len == 0) return;

    spi_transaction_t t = {
        .length    = len * 8,
        .tx_buffer = data,
    };
    ESP_ERROR_CHECK(spi_device_transmit(h->spi, &t));
}

void tft_send_command(tft_handle_t h, uint8_t cmd)
{
    _cs_lo(h);
    _dc_cmd(h);
    _spi_write(h, &cmd, 1);
    _cs_hi(h);
}

void tft_send_data(tft_handle_t h, const uint8_t *data, size_t len)
{
    _cs_lo(h);
    _dc_data(h);
    _spi_write(h, data, len);
    _cs_hi(h);
}

static void _send_data8(tft_handle_t h, uint8_t d)
{
    tft_send_data(h, &d, 1);
}

static void _cmd_data(tft_handle_t h, uint8_t cmd, const uint8_t *data, size_t len)
{
    tft_send_command(h, cmd);
    if (data && len) tft_send_data(h, data, len);
}

/* ─────────────────────────────────────────────
 *  Address window
 * ───────────────────────────────────────────── */
void tft_set_addr_window(tft_handle_t h,
                         uint16_t x0, uint16_t y0,
                         uint16_t x1, uint16_t y1)
{
    x0 += TFT_COL_OFFSET;  x1 += TFT_COL_OFFSET;
    y0 += TFT_ROW_OFFSET;  y1 += TFT_ROW_OFFSET;

    uint8_t caset[4] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF };
    uint8_t raset[4] = { y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF };

    _cmd_data(h, ST7735_CASET, caset, 4);
    _cmd_data(h, ST7735_RASET, raset, 4);
    tft_send_command(h, ST7735_RAMWR);
}

/* Fill a pre-opened address window with a single colour (fast) */
static void _fill_window(tft_handle_t h, uint16_t colour, uint32_t count)
{
    /* Swap to big-endian */
    uint8_t hi = colour >> 8;
    uint8_t lo = colour & 0xFF;

    /* Use a 64-byte chunk buffer to speed up DMA transfers */
    uint8_t buf[64];
    for (int i = 0; i < 64; i += 2) { buf[i] = hi; buf[i+1] = lo; }

    _cs_lo(h);
    _dc_data(h);
    while (count >= 32) {
        _spi_write(h, buf, 64);
        count -= 32;
    }
    while (count--) {
        _spi_write(h, buf, 2);
    }
    _cs_hi(h);
}

/* ─────────────────────────────────────────────
 *  ST7735 initialisation sequence
 * ───────────────────────────────────────────── */
static void _hw_reset(tft_handle_t h)
{
    gpio_set_level(h->cfg.pin_res, 1);
    vTaskDelay(pdMS_TO_TICKS(5));
    gpio_set_level(h->cfg.pin_res, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(h->cfg.pin_res, 1);
    vTaskDelay(pdMS_TO_TICKS(150));
}

static void _init_sequence(tft_handle_t h)
{
    /* Software reset */
    tft_send_command(h, ST7735_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));

    /* Sleep out */
    tft_send_command(h, ST7735_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Frame-rate control (normal mode) */
    _cmd_data(h, ST7735_FRMCTR1, (uint8_t[]){0x01, 0x2C, 0x2D}, 3);
    /* Frame-rate control (idle mode) */
    _cmd_data(h, ST7735_FRMCTR2, (uint8_t[]){0x01, 0x2C, 0x2D}, 3);
    /* Frame-rate control (partial mode) – dot/line inversion */
    _cmd_data(h, ST7735_FRMCTR3, (uint8_t[]){0x01,0x2C,0x2D,0x01,0x2C,0x2D}, 6);

    /* Display inversion control: no inversion */
    _cmd_data(h, ST7735_INVCTR, (uint8_t[]){0x07}, 1);

    /* Power control */
    _cmd_data(h, ST7735_PWCTR1, (uint8_t[]){0xA2, 0x02, 0x84}, 3);
    _cmd_data(h, ST7735_PWCTR2, (uint8_t[]){0xC5}, 1);
    _cmd_data(h, ST7735_PWCTR3, (uint8_t[]){0x0A, 0x00}, 2);
    _cmd_data(h, ST7735_PWCTR4, (uint8_t[]){0x8A, 0x2A}, 2);
    _cmd_data(h, ST7735_PWCTR5, (uint8_t[]){0x8A, 0xEE}, 2);

    /* VCOM */
    _cmd_data(h, ST7735_VMCTR1, (uint8_t[]){0x0E}, 1);

    /* Display inversion off */
    tft_send_command(h, ST7735_INVOFF);

    /* Memory access (will be overridden by set_rotation) */
    uint8_t madctl = MADCTL_MX | MADCTL_MY | (h->cfg.bgr_filter ? MADCTL_BGR : MADCTL_RGB);
    _cmd_data(h, ST7735_MADCTL, &madctl, 1);

    /* Colour mode: 16-bit */
    _cmd_data(h, ST7735_COLMOD, (uint8_t[]){0x05}, 1);

    /* Gamma positive */
    _cmd_data(h, ST7735_GMCTRP1, (uint8_t[]){
        0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
        0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10
    }, 16);

    /* Gamma negative */
    _cmd_data(h, ST7735_GMCTRN1, (uint8_t[]){
        0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
        0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10
    }, 16);

    /* Normal display */
    tft_send_command(h, ST7735_NORON);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Display ON */
    tft_send_command(h, ST7735_DISPON);
    vTaskDelay(pdMS_TO_TICKS(100));
}

/* Apply MADCTL for a given rotation */
static void _apply_rotation(tft_handle_t h, tft_rotation_t r)
{
    uint8_t bgr = h->cfg.bgr_filter ? MADCTL_BGR : MADCTL_RGB;
    uint8_t madctl;

    switch (r) {
    default:
    case TFT_ROTATION_0:
        madctl = MADCTL_MX | MADCTL_MY | bgr;
        h->width  = TFT_WIDTH;
        h->height = TFT_HEIGHT;
        break;
    case TFT_ROTATION_90:
        madctl = MADCTL_MY | MADCTL_MV | bgr;
        h->width  = TFT_HEIGHT;
        h->height = TFT_WIDTH;
        break;
    case TFT_ROTATION_180:
        madctl = bgr;
        h->width  = TFT_WIDTH;
        h->height = TFT_HEIGHT;
        break;
    case TFT_ROTATION_270:
        madctl = MADCTL_MX | MADCTL_MV | bgr;
        h->width  = TFT_HEIGHT;
        h->height = TFT_WIDTH;
        break;
    }
    h->rotation = r;
    _cmd_data(h, ST7735_MADCTL, &madctl, 1);
}

/* ─────────────────────────────────────────────
 *  Lifecycle
 * ───────────────────────────────────────────── */
esp_err_t tft_init(const tft_config_t *cfg, tft_handle_t *out)
{
    ESP_LOGI(TAG, "Initialising ST7735 1.77\" TFT");

    struct tft_dev_s *h = calloc(1, sizeof(struct tft_dev_s));
    if (!h) return ESP_ERR_NO_MEM;

    h->cfg = *cfg;

    /* Configure GPIO outputs */
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << cfg->pin_res) |
                        (1ULL << cfg->pin_rs)  |
                        (1ULL << cfg->pin_cs)  |
                        (1ULL << cfg->pin_leda),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));

    /* Backlight OFF during init */
    gpio_set_level(cfg->pin_leda, 0);
    _cs_hi(h);
    _dc_data(h);

    /* Initialise SPI bus */
    spi_bus_config_t bus = {
        .mosi_io_num   = cfg->pin_sda,
        .miso_io_num   = -1,        /* not used */
        .sclk_io_num   = cfg->pin_sck,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = TFT_WIDTH * TFT_HEIGHT * 2 + 8,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(cfg->spi_host, &bus, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev = {
        .clock_speed_hz = cfg->spi_clock_hz > 0 ? cfg->spi_clock_hz : 27000000,
        .mode           = 0,           /* CPOL=0, CPHA=0 */
        .spics_io_num   = -1,          /* manual CS via GPIO */
        .queue_size     = 7,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(cfg->spi_host, &dev, &h->spi));

    /* Hardware reset */
    _hw_reset(h);

    /* Send init commands */
    _init_sequence(h);

    /* Set rotation */
    _apply_rotation(h, cfg->rotation);

    /* Clear screen */
    tft_fill_screen(h, TFT_BLACK);

    /* Backlight ON */
    gpio_set_level(cfg->pin_leda, 1);

    *out = h;
    ESP_LOGI(TAG, "ST7735 ready  %dx%d", h->width, h->height);
    return ESP_OK;
}

esp_err_t tft_deinit(tft_handle_t h)
{
    if (!h) return ESP_ERR_INVALID_ARG;
    tft_backlight_off(h);
    tft_display_off(h);
    spi_bus_remove_device(h->spi);
    spi_bus_free(h->cfg.spi_host);
    free(h);
    return ESP_OK;
}

/* ─────────────────────────────────────────────
 *  Backlight / display control
 * ───────────────────────────────────────────── */
void tft_backlight_on(tft_handle_t h)  { gpio_set_level(h->cfg.pin_leda, 1); }
void tft_backlight_off(tft_handle_t h) { gpio_set_level(h->cfg.pin_leda, 0); }

void tft_display_on(tft_handle_t h)
{
    tft_send_command(h, ST7735_DISPON);
}

void tft_display_off(tft_handle_t h)
{
    tft_send_command(h, ST7735_DISPOFF);
}

void tft_sleep(tft_handle_t h)
{
    tft_send_command(h, ST7735_SLPIN);
    vTaskDelay(pdMS_TO_TICKS(5));
}

void tft_wake(tft_handle_t h)
{
    tft_send_command(h, ST7735_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));
}

void tft_set_rotation(tft_handle_t h, tft_rotation_t r)
{
    _apply_rotation(h, r);
}

void tft_invert_display(tft_handle_t h, bool invert)
{
    tft_send_command(h, invert ? ST7735_INVON : ST7735_INVOFF);
}

uint16_t tft_get_width(tft_handle_t h)  { return h->width; }
uint16_t tft_get_height(tft_handle_t h) { return h->height; }

/* ─────────────────────────────────────────────
 *  Drawing – screen / pixel
 * ───────────────────────────────────────────── */
void tft_fill_screen(tft_handle_t h, uint16_t colour)
{
    tft_set_addr_window(h, 0, 0, h->width - 1, h->height - 1);
    _fill_window(h, colour, (uint32_t)h->width * h->height);
}

void tft_draw_pixel(tft_handle_t h, int16_t x, int16_t y, uint16_t colour)
{
    if (x < 0 || y < 0 || x >= h->width || y >= h->height) return;

    tft_set_addr_window(h, x, y, x, y);
    uint8_t d[2] = { colour >> 8, colour & 0xFF };
    tft_send_data(h, d, 2);
}

/* ─────────────────────────────────────────────
 *  Drawing – lines
 * ───────────────────────────────────────────── */
void tft_draw_hline(tft_handle_t h, int16_t x, int16_t y, int16_t w, uint16_t colour)
{
    if (y < 0 || y >= h->height || x >= h->width || w <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > h->width) w = h->width - x;

    tft_set_addr_window(h, x, y, x + w - 1, y);
    _fill_window(h, colour, w);
}

void tft_draw_vline(tft_handle_t h, int16_t x, int16_t y, int16_t ht, uint16_t colour)
{
    if (x < 0 || x >= h->width || y >= h->height || ht <= 0) return;
    if (y < 0) { ht += y; y = 0; }
    if (y + ht > h->height) ht = h->height - y;

    tft_set_addr_window(h, x, y, x, y + ht - 1);
    _fill_window(h, colour, ht);
}

void tft_draw_line(tft_handle_t h,
                   int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   uint16_t colour)
{
    /* Fast paths */
    if (y0 == y1) { tft_draw_hline(h, x0, y0, x1 - x0 + 1, colour); return; }
    if (x0 == x1) { tft_draw_vline(h, x0, y0, y1 - y0 + 1, colour); return; }

    /* Bresenham */
    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) { int16_t t; t=x0;x0=y0;y0=t; t=x1;x1=y1;y1=t; }
    if (x0 > x1) { int16_t t; t=x0;x0=x1;x1=t; t=y0;y0=y1;y1=t; }

    int16_t dx = x1 - x0, dy = abs(y1 - y0);
    int16_t err = dx / 2, ystep = (y0 < y1) ? 1 : -1;

    for (; x0 <= x1; x0++) {
        if (steep) tft_draw_pixel(h, y0, x0, colour);
        else        tft_draw_pixel(h, x0, y0, colour);
        err -= dy;
        if (err < 0) { y0 += ystep; err += dx; }
    }
}

/* ─────────────────────────────────────────────
 *  Drawing – rectangles
 * ───────────────────────────────────────────── */
void tft_draw_rect(tft_handle_t h, int16_t x, int16_t y,
                   int16_t w, int16_t ht, uint16_t colour)
{
    tft_draw_hline(h, x, y,        w,  colour);
    tft_draw_hline(h, x, y+ht-1,   w,  colour);
    tft_draw_vline(h, x,   y,       ht, colour);
    tft_draw_vline(h, x+w-1, y,     ht, colour);
}

void tft_fill_rect(tft_handle_t h, int16_t x, int16_t y,
                   int16_t w, int16_t ht, uint16_t colour)
{
    if (x >= h->width || y >= h->height || w <= 0 || ht <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { ht += y; y = 0; }
    if (x + w > h->width)  w  = h->width  - x;
    if (y + ht > h->height) ht = h->height - y;

    tft_set_addr_window(h, x, y, x + w - 1, y + ht - 1);
    _fill_window(h, colour, (uint32_t)w * ht);
}

/* Helper: draw circle quadrant segments */
static void _circle_helper(tft_handle_t h, int16_t cx, int16_t cy,
                            int16_t r, uint8_t cornermask, uint16_t colour)
{
    int16_t f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r;
    while (x < y) {
        if (f >= 0) { y--; ddy += 2; f += ddy; }
        x++; ddx += 2; f += ddx;
        if (cornermask & 0x4) { tft_draw_pixel(h, cx+x, cy+y, colour); tft_draw_pixel(h, cx+y, cy+x, colour); }
        if (cornermask & 0x2) { tft_draw_pixel(h, cx+x, cy-y, colour); tft_draw_pixel(h, cx+y, cy-x, colour); }
        if (cornermask & 0x8) { tft_draw_pixel(h, cx-y, cy+x, colour); tft_draw_pixel(h, cx-x, cy+y, colour); }
        if (cornermask & 0x1) { tft_draw_pixel(h, cx-y, cy-x, colour); tft_draw_pixel(h, cx-x, cy-y, colour); }
    }
}

static void _fill_circle_helper(tft_handle_t h, int16_t cx, int16_t cy,
                                 int16_t r, uint8_t sides, int16_t delta,
                                 uint16_t colour)
{
    int16_t f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r;
    while (x < y) {
        if (f >= 0) { y--; ddy += 2; f += ddy; }
        x++; ddx += 2; f += ddx;
        if (sides & 0x1) {
            tft_draw_vline(h, cx+x, cy-y, 2*y+1+delta, colour);
            tft_draw_vline(h, cx+y, cy-x, 2*x+1+delta, colour);
        }
        if (sides & 0x2) {
            tft_draw_vline(h, cx-x, cy-y, 2*y+1+delta, colour);
            tft_draw_vline(h, cx-y, cy-x, 2*x+1+delta, colour);
        }
    }
}

void tft_draw_round_rect(tft_handle_t h, int16_t x, int16_t y,
                          int16_t w, int16_t ht, int16_t r, uint16_t colour)
{
    tft_draw_hline(h, x+r, y,        w-2*r, colour);
    tft_draw_hline(h, x+r, y+ht-1,   w-2*r, colour);
    tft_draw_vline(h, x,   y+r,       ht-2*r, colour);
    tft_draw_vline(h, x+w-1, y+r,     ht-2*r, colour);
    _circle_helper(h, x+r,     y+r,     r, 0x1, colour);
    _circle_helper(h, x+w-r-1, y+r,     r, 0x2, colour);
    _circle_helper(h, x+w-r-1, y+ht-r-1, r, 0x4, colour);
    _circle_helper(h, x+r,     y+ht-r-1, r, 0x8, colour);
}

void tft_fill_round_rect(tft_handle_t h, int16_t x, int16_t y,
                          int16_t w, int16_t ht, int16_t r, uint16_t colour)
{
    tft_fill_rect(h, x+r, y, w-2*r, ht, colour);
    _fill_circle_helper(h, x+r,     y+ht/2, r, 0x2, ht-2*r-1, colour);
    _fill_circle_helper(h, x+w-r-1, y+ht/2, r, 0x1, ht-2*r-1, colour);
}

/* ─────────────────────────────────────────────
 *  Drawing – circles
 * ───────────────────────────────────────────── */
void tft_draw_circle(tft_handle_t h, int16_t cx, int16_t cy,
                     int16_t r, uint16_t colour)
{
    int16_t f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r;
    tft_draw_pixel(h, cx,   cy+r, colour);
    tft_draw_pixel(h, cx,   cy-r, colour);
    tft_draw_pixel(h, cx+r, cy,   colour);
    tft_draw_pixel(h, cx-r, cy,   colour);
    while (x < y) {
        if (f >= 0) { y--; ddy += 2; f += ddy; }
        x++; ddx += 2; f += ddx;
        tft_draw_pixel(h, cx+x, cy+y, colour);
        tft_draw_pixel(h, cx-x, cy+y, colour);
        tft_draw_pixel(h, cx+x, cy-y, colour);
        tft_draw_pixel(h, cx-x, cy-y, colour);
        tft_draw_pixel(h, cx+y, cy+x, colour);
        tft_draw_pixel(h, cx-y, cy+x, colour);
        tft_draw_pixel(h, cx+y, cy-x, colour);
        tft_draw_pixel(h, cx-y, cy-x, colour);
    }
}

void tft_fill_circle(tft_handle_t h, int16_t cx, int16_t cy,
                     int16_t r, uint16_t colour)
{
    tft_draw_vline(h, cx, cy - r, 2 * r + 1, colour);
    _fill_circle_helper(h, cx, cy, r, 0x3, 0, colour);
}

/* ─────────────────────────────────────────────
 *  Drawing – triangles
 * ───────────────────────────────────────────── */
void tft_draw_triangle(tft_handle_t h,
                        int16_t x0, int16_t y0,
                        int16_t x1, int16_t y1,
                        int16_t x2, int16_t y2,
                        uint16_t colour)
{
    tft_draw_line(h, x0, y0, x1, y1, colour);
    tft_draw_line(h, x1, y1, x2, y2, colour);
    tft_draw_line(h, x2, y2, x0, y0, colour);
}

void tft_fill_triangle(tft_handle_t h,
                        int16_t x0, int16_t y0,
                        int16_t x1, int16_t y1,
                        int16_t x2, int16_t y2,
                        uint16_t colour)
{
    /* Sort vertices by Y ascending */
    int16_t a, b, last;
    if (y0 > y1) { int16_t t; t=y0;y0=y1;y1=t; t=x0;x0=x1;x1=t; }
    if (y1 > y2) { int16_t t; t=y1;y1=y2;y2=t; t=x1;x1=x2;x2=t; }
    if (y0 > y1) { int16_t t; t=y0;y0=y1;y1=t; t=x0;x0=x1;x1=t; }

    if (y0 == y2) {
        a = b = x0;
        if (x1 < a) a = x1; 
        if (x2 < a) a = x2;
        if (x1 > b) b = x1; 
        if (x2 > b) b = x2;
        tft_draw_hline(h, a, y0, b - a + 1, colour);
        return;
    }

    int16_t dx01 = x1 - x0, dy01 = y1 - y0;
    int16_t dx02 = x2 - x0, dy02 = y2 - y0;
    int16_t dx12 = x2 - x1, dy12 = y2 - y1;
    int32_t sa = 0, sb = 0;

    last = (y1 == y2) ? y1 : y1 - 1;
    for (int16_t y = y0; y <= last; y++) {
        a = x0 + sa / dy01;
        b = x0 + sb / dy02;
        sa += dx01; sb += dx02;
        if (a > b) { int16_t t = a; a = b; b = t; }
        tft_draw_hline(h, a, y, b - a + 1, colour);
    }

    sa = (int32_t)dx12 * (y1 - y1);
    sb = (int32_t)dx02 * (y1 - y0);
    for (int16_t y = y1; y <= y2; y++) {
        a = x1 + sa / dy12;
        b = x0 + sb / dy02;
        sa += dx12; sb += dx02;
        if (a > b) { int16_t t = a; a = b; b = t; }
        tft_draw_hline(h, a, y, b - a + 1, colour);
    }
}

/* ─────────────────────────────────────────────
 *  Bitmap
 * ───────────────────────────────────────────── */
void tft_draw_bitmap(tft_handle_t h, int16_t x, int16_t y,
                     int16_t w, int16_t ht, const uint16_t *data)
{
    if (!data || w <= 0 || ht <= 0) return;
    tft_set_addr_window(h, x, y, x + w - 1, y + ht - 1);

    /* data is big-endian RGB565 ready to send */
    tft_send_data(h, (const uint8_t *)data, (size_t)w * ht * 2);
}

/* ─────────────────────────────────────────────
 *  Text
 * ───────────────────────────────────────────── */
#define FONT_W  5
#define FONT_H  7

void tft_draw_char(tft_handle_t h, int16_t x, int16_t y, char c,
                   uint16_t fg, uint16_t bg, uint8_t scale)
{
    if (c < 0x20 || c > 0x7E) c = '?';
    const uint8_t *glyph = font5x7[c - 0x20];

    uint8_t hi_fg = fg >> 8, lo_fg = fg & 0xFF;
    uint8_t hi_bg = bg >> 8, lo_bg = bg & 0xFF;

    for (uint8_t col = 0; col < FONT_W; col++) {
        for (uint8_t row = 0; row < FONT_H; row++) {
            bool on = (glyph[col] >> row) & 1;
            uint16_t colour = on ? fg : bg;
            (void)hi_fg; (void)lo_fg; (void)hi_bg; (void)lo_bg;
            for (uint8_t sy = 0; sy < scale; sy++) {
                for (uint8_t sx = 0; sx < scale; sx++) {
                    tft_draw_pixel(h,
                        x + col * scale + sx,
                        y + row * scale + sy,
                        colour);
                }
            }
        }
    }
}

void tft_draw_string(tft_handle_t h, int16_t x, int16_t y,
                     const char *str, uint16_t fg, uint16_t bg, uint8_t scale)
{
    if (!str) return;
    int16_t cx = x, cy = y;
    uint8_t cw = (FONT_W + 1) * scale;
    uint8_t ch = (FONT_H + 2) * scale;

    while (*str) {
        if (*str == '\n') {
            cx = x;
            cy += ch;
        } else {
            if (cx + cw > h->width) { cx = x; cy += ch; }
            tft_draw_char(h, cx, cy, *str, fg, bg, scale);
            cx += cw;
        }
        str++;
    }
}

void tft_printf(tft_handle_t h, int16_t x, int16_t y,
                uint16_t fg, uint16_t bg, uint8_t scale,
                const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    tft_draw_string(h, x, y, buf, fg, bg, scale);
}