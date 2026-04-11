#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────
 *  Screen geometry (ST7735S, 1.77", 128x160)
 * ───────────────────────────────────────────── */
#define TFT_WIDTH       128
#define TFT_HEIGHT      160

/* Column / Row offsets (may vary by panel revision) */
#define TFT_COL_OFFSET  0
#define TFT_ROW_OFFSET  0

/* ─────────────────────────────────────────────
 *  ST7735 Command set
 * ───────────────────────────────────────────── */
#define ST7735_NOP       0x00
#define ST7735_SWRESET   0x01
#define ST7735_RDDID     0x04
#define ST7735_RDDST     0x09
#define ST7735_SLPIN     0x10
#define ST7735_SLPOUT    0x11
#define ST7735_PTLON     0x12
#define ST7735_NORON     0x13
#define ST7735_INVOFF    0x20
#define ST7735_INVON     0x21
#define ST7735_GAMSET    0x26
#define ST7735_DISPOFF   0x28
#define ST7735_DISPON    0x29
#define ST7735_CASET     0x2A
#define ST7735_RASET     0x2B
#define ST7735_RAMWR     0x2C
#define ST7735_RAMRD     0x2E
#define ST7735_PTLAR     0x30
#define ST7735_COLMOD    0x3A
#define ST7735_MADCTL    0x36
#define ST7735_FRMCTR1   0xB1
#define ST7735_FRMCTR2   0xB2
#define ST7735_FRMCTR3   0xB3
#define ST7735_INVCTR    0xB4
#define ST7735_DISSET5   0xB6
#define ST7735_PWCTR1    0xC0
#define ST7735_PWCTR2    0xC1
#define ST7735_PWCTR3    0xC2
#define ST7735_PWCTR4    0xC3
#define ST7735_PWCTR5    0xC4
#define ST7735_VMCTR1    0xC5
#define ST7735_RDID1     0xDA
#define ST7735_RDID2     0xDB
#define ST7735_RDID3     0xDC
#define ST7735_RDID4     0xDD
#define ST7735_GMCTRP1   0xE0
#define ST7735_GMCTRN1   0xE1

/* MADCTL orientation flags */
#define MADCTL_MY   0x80   /* Row address order */
#define MADCTL_MX   0x40   /* Column address order */
#define MADCTL_MV   0x20   /* Row/Column exchange */
#define MADCTL_ML   0x10   /* Vertical refresh order */
#define MADCTL_RGB  0x00   /* RGB colour filter */
#define MADCTL_BGR  0x08   /* BGR colour filter */

/* ─────────────────────────────────────────────
 *  Display rotation
 * ───────────────────────────────────────────── */
typedef enum {
    TFT_ROTATION_0   = 0,   /* Portrait, connector at bottom  */
    TFT_ROTATION_90  = 1,   /* Landscape, connector at right  */
    TFT_ROTATION_180 = 2,   /* Portrait, connector at top     */
    TFT_ROTATION_270 = 3,   /* Landscape, connector at left   */
} tft_rotation_t;

/* ─────────────────────────────────────────────
 *  16-bit RGB565 colour helpers
 * ───────────────────────────────────────────── */
#define TFT_COLOR(r, g, b) \
    ((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3)))

#define TFT_BLACK       0x0000
#define TFT_WHITE       0xFFFF
#define TFT_RED         0xF800
#define TFT_GREEN       0x07E0
#define TFT_BLUE        0x001F
#define TFT_CYAN        0x07FF
#define TFT_MAGENTA     0xF81F
#define TFT_YELLOW      0xFFE0
#define TFT_ORANGE      0xFC00
#define TFT_NAVY        0x000F
#define TFT_DARKGREEN   0x03E0
#define TFT_MAROON      0x7800
#define TFT_PURPLE      0x780F
#define TFT_OLIVE       0x7BE0
#define TFT_LIGHTGREY   0xC618
#define TFT_DARKGREY    0x7BEF

/* ─────────────────────────────────────────────
 *  Configuration structure
 * ───────────────────────────────────────────── */
typedef struct {
    /* SPI host (SPI2_HOST or SPI3_HOST recommended) */
    spi_host_device_t spi_host;

    /* GPIO pin numbers */
    gpio_num_t pin_sck;    /* SCK  – SPI clock           */
    gpio_num_t pin_sda;    /* SDA  – SPI MOSI            */
    gpio_num_t pin_res;    /* RES  – Hardware reset      */
    gpio_num_t pin_rs;     /* RS   – Data / Command (DC) */
    gpio_num_t pin_cs;     /* CS   – Chip select         */
    gpio_num_t pin_leda;   /* LEDA – Backlight anode     */

    /* SPI clock speed (max 27 MHz for ST7735) */
    int        spi_clock_hz;

    /* Initial rotation */
    tft_rotation_t rotation;

    /* Set true if the panel uses BGR pixel order */
    bool bgr_filter;
} tft_config_t;

/* Opaque handle returned by tft_init() */
typedef struct tft_dev_s *tft_handle_t;

/* ─────────────────────────────────────────────
 *  Lifecycle
 * ───────────────────────────────────────────── */

/**
 * @brief  Initialise the SPI bus and the ST7735 controller.
 *
 * @param  cfg     Pointer to a filled tft_config_t.
 * @param  handle  Out: opaque device handle.
 * @return ESP_OK on success.
 */
esp_err_t tft_init(const tft_config_t *cfg, tft_handle_t *handle);

/**
 * @brief  Release all resources and de-initialise the SPI bus.
 */
esp_err_t tft_deinit(tft_handle_t handle);

/* ─────────────────────────────────────────────
 *  Backlight
 * ───────────────────────────────────────────── */
void tft_backlight_on(tft_handle_t handle);
void tft_backlight_off(tft_handle_t handle);

/* ─────────────────────────────────────────────
 *  Display control
 * ───────────────────────────────────────────── */
void tft_display_on(tft_handle_t handle);
void tft_display_off(tft_handle_t handle);
void tft_sleep(tft_handle_t handle);
void tft_wake(tft_handle_t handle);
void tft_set_rotation(tft_handle_t handle, tft_rotation_t rotation);
void tft_invert_display(tft_handle_t handle, bool invert);

/**
 * @brief  Return the active display width (accounts for rotation).
 */
uint16_t tft_get_width(tft_handle_t handle);

/**
 * @brief  Return the active display height (accounts for rotation).
 */
uint16_t tft_get_height(tft_handle_t handle);

/* ─────────────────────────────────────────────
 *  Drawing primitives
 * ───────────────────────────────────────────── */

/** Fill the entire screen with one colour. */
void tft_fill_screen(tft_handle_t handle, uint16_t colour);

/** Draw a single pixel. */
void tft_draw_pixel(tft_handle_t handle, int16_t x, int16_t y, uint16_t colour);

/** Draw a horizontal line (fast path). */
void tft_draw_hline(tft_handle_t handle, int16_t x, int16_t y, int16_t w, uint16_t colour);

/** Draw a vertical line (fast path). */
void tft_draw_vline(tft_handle_t handle, int16_t x, int16_t y, int16_t h, uint16_t colour);

/** Draw an arbitrary line (Bresenham). */
void tft_draw_line(tft_handle_t handle, int16_t x0, int16_t y0,
                   int16_t x1, int16_t y1, uint16_t colour);

/** Draw a rectangle outline. */
void tft_draw_rect(tft_handle_t handle, int16_t x, int16_t y,
                   int16_t w, int16_t h, uint16_t colour);

/** Draw a filled rectangle. */
void tft_fill_rect(tft_handle_t handle, int16_t x, int16_t y,
                   int16_t w, int16_t h, uint16_t colour);

/** Draw a rectangle with rounded corners (outline). */
void tft_draw_round_rect(tft_handle_t handle, int16_t x, int16_t y,
                         int16_t w, int16_t h, int16_t r, uint16_t colour);

/** Draw a filled rectangle with rounded corners. */
void tft_fill_round_rect(tft_handle_t handle, int16_t x, int16_t y,
                         int16_t w, int16_t h, int16_t r, uint16_t colour);

/** Draw a circle outline. */
void tft_draw_circle(tft_handle_t handle, int16_t cx, int16_t cy,
                     int16_t r, uint16_t colour);

/** Draw a filled circle. */
void tft_fill_circle(tft_handle_t handle, int16_t cx, int16_t cy,
                     int16_t r, uint16_t colour);

/** Draw a triangle outline. */
void tft_draw_triangle(tft_handle_t handle,
                        int16_t x0, int16_t y0,
                        int16_t x1, int16_t y1,
                        int16_t x2, int16_t y2,
                        uint16_t colour);

/** Draw a filled triangle. */
void tft_fill_triangle(tft_handle_t handle,
                        int16_t x0, int16_t y0,
                        int16_t x1, int16_t y1,
                        int16_t x2, int16_t y2,
                        uint16_t colour);

/* ─────────────────────────────────────────────
 *  Bitmap / image rendering
 * ───────────────────────────────────────────── */

/**
 * @brief  Blit a raw RGB565 bitmap into a screen region.
 *
 * @param  x, y   Top-left corner.
 * @param  w, h   Width and height in pixels.
 * @param  data   Pointer to pixel data (big-endian RGB565, row-major).
 */
void tft_draw_bitmap(tft_handle_t handle, int16_t x, int16_t y,
                     int16_t w, int16_t h, const uint16_t *data);

/* ─────────────────────────────────────────────
 *  Text rendering  (built-in 5x7 font)
 * ───────────────────────────────────────────── */

/**
 * @brief  Draw a single ASCII character.
 *
 * @param  x, y       Top-left of the character cell.
 * @param  c          ASCII character (0x20–0x7E).
 * @param  fg         Foreground colour.
 * @param  bg         Background colour (set equal to fg for transparent-ish).
 * @param  scale      Integer scale factor (1 = 5×7 px, 2 = 10×14 px …).
 */
void tft_draw_char(tft_handle_t handle, int16_t x, int16_t y, char c,
                   uint16_t fg, uint16_t bg, uint8_t scale);

/**
 * @brief  Draw a null-terminated ASCII string.
 *
 * Automatically wraps at the right edge and advances to the next line.
 */
void tft_draw_string(tft_handle_t handle, int16_t x, int16_t y,
                     const char *str, uint16_t fg, uint16_t bg, uint8_t scale);

/**
 * @brief  Printf-style text helper (max 128 chars per call).
 */
void tft_printf(tft_handle_t handle, int16_t x, int16_t y,
                uint16_t fg, uint16_t bg, uint8_t scale,
                const char *fmt, ...) __attribute__((format(printf, 7, 8)));

/* ─────────────────────────────────────────────
 *  Low-level access (advanced use)
 * ───────────────────────────────────────────── */
void tft_send_command(tft_handle_t handle, uint8_t cmd);
void tft_send_data(tft_handle_t handle, const uint8_t *data, size_t len);
void tft_set_addr_window(tft_handle_t handle,
                         uint16_t x0, uint16_t y0,
                         uint16_t x1, uint16_t y1);

#ifdef __cplusplus
}
#endif