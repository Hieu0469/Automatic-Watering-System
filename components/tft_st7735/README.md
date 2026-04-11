# tft_st7735 — ESP32-IDF Driver for the 1.77" ST7735 TFT

A compact, zero-dependency ESP32-IDF component for the **ST7735S**-based
1.77-inch TFT display (128 × 160 px, RGB565).

---

## Hardware

| TFT Pin | Signal       | Description                   |
|---------|--------------|-------------------------------|
| SCK     | SPI clock    | SCLK                          |
| SDA     | SPI data     | MOSI                          |
| RES     | Reset        | Active-low hardware reset     |
| RS      | Register Sel | Low = command, High = data    |
| CS      | Chip select  | Active-low                    |
| LEDA    | Backlight    | Drive HIGH to enable          |
| VCC     | Power        | 3.3 V                         |
| GND     | Ground       | GND                           |

**No MISO** line is needed – the driver is write-only.

---

## Directory layout

```
tft_st7735/
├── CMakeLists.txt          ← IDF component manifest
├── include/
│   └── tft_st7735.h        ← Public API header
├── tft_st7735.c            ← Driver implementation
└── example/
    └── main.c              ← Standalone usage example
```

---

## Installation

Copy the `tft_st7735/` folder into your project's `components/` directory:

```
my_project/
├── components/
│   └── tft_st7735/          ← place here
├── main/
│   └── CMakeLists.txt
└── CMakeLists.txt
```

Add `tft_st7735` to your `main/CMakeLists.txt` REQUIRES list:

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES tft_st7735
)
```

---

## Quick start

```c
#include "tft_st7735.h"

tft_config_t cfg = {
    .spi_host     = SPI2_HOST,
    .pin_sck      = GPIO_NUM_18,
    .pin_sda      = GPIO_NUM_23,
    .pin_res      = GPIO_NUM_4,
    .pin_rs       = GPIO_NUM_2,
    .pin_cs       = GPIO_NUM_5,
    .pin_leda     = GPIO_NUM_15,
    .spi_clock_hz = 27000000,      // 27 MHz (ST7735 max)
    .rotation     = TFT_ROTATION_0,
    .bgr_filter   = false,         // true if colours look inverted
};

tft_handle_t tft;
ESP_ERROR_CHECK(tft_init(&cfg, &tft));

tft_fill_screen(tft, TFT_BLACK);
tft_draw_string(tft, 10, 10, "Hello!", TFT_WHITE, TFT_BLACK, 1);
```

---

## API reference

### Lifecycle

| Function | Description |
|---|---|
| `tft_init(cfg, &handle)` | Initialise SPI bus + ST7735. Returns `ESP_OK` on success. |
| `tft_deinit(handle)` | Free resources and de-initialise the SPI bus. |

### Display control

| Function | Description |
|---|---|
| `tft_backlight_on(h)` | Drive LEDA HIGH (backlight on). |
| `tft_backlight_off(h)` | Drive LEDA LOW (backlight off). |
| `tft_display_on(h)` | Send DISPON command. |
| `tft_display_off(h)` | Send DISPOFF command. |
| `tft_sleep(h)` | Enter sleep mode (low power). |
| `tft_wake(h)` | Exit sleep mode. |
| `tft_set_rotation(h, rot)` | `TFT_ROTATION_0/90/180/270` |
| `tft_invert_display(h, bool)` | Invert pixel colours. |
| `tft_get_width(h)` | Effective width after rotation. |
| `tft_get_height(h)` | Effective height after rotation. |

### Drawing

| Function | Description |
|---|---|
| `tft_fill_screen(h, colour)` | Fill entire screen. |
| `tft_draw_pixel(h, x, y, colour)` | Single pixel. |
| `tft_draw_hline(h, x, y, w, colour)` | Horizontal line (fast DMA path). |
| `tft_draw_vline(h, x, y, h, colour)` | Vertical line (fast DMA path). |
| `tft_draw_line(h, x0,y0, x1,y1, colour)` | Arbitrary line (Bresenham). |
| `tft_draw_rect(h, x,y,w,h, colour)` | Rectangle outline. |
| `tft_fill_rect(h, x,y,w,h, colour)` | Filled rectangle. |
| `tft_draw_round_rect(h, x,y,w,h,r, colour)` | Rounded rect outline. |
| `tft_fill_round_rect(h, x,y,w,h,r, colour)` | Filled rounded rect. |
| `tft_draw_circle(h, cx,cy,r, colour)` | Circle outline. |
| `tft_fill_circle(h, cx,cy,r, colour)` | Filled circle. |
| `tft_draw_triangle(h, x0,y0,x1,y1,x2,y2, colour)` | Triangle outline. |
| `tft_fill_triangle(h, x0,y0,x1,y1,x2,y2, colour)` | Filled triangle. |
| `tft_draw_bitmap(h, x,y,w,h, data)` | Raw RGB565 bitmap blit. |

### Text

| Function | Description |
|---|---|
| `tft_draw_char(h, x,y, c, fg,bg, scale)` | Single ASCII char. Built-in 5×7 font. |
| `tft_draw_string(h, x,y, str, fg,bg, scale)` | Null-terminated string; auto-wraps. |
| `tft_printf(h, x,y, fg,bg, scale, fmt, ...)` | Printf-style helper. |

### Colour macros

```c
TFT_BLACK, TFT_WHITE, TFT_RED, TFT_GREEN, TFT_BLUE,
TFT_CYAN, TFT_MAGENTA, TFT_YELLOW, TFT_ORANGE,
TFT_NAVY, TFT_DARKGREEN, TFT_MAROON, TFT_PURPLE,
TFT_OLIVE, TFT_LIGHTGREY, TFT_DARKGREY

// Custom RGB → RGB565
TFT_COLOR(r, g, b)
```

---

## SPI clock

The ST7735S is rated for up to **15 MHz** writes in the datasheet, but most
modules on the market run reliably at **27 MHz**. Start at 27 MHz and reduce
if you see pixel noise.

```c
.spi_clock_hz = 27000000,   // 27 MHz – typical
.spi_clock_hz = 15000000,   // 15 MHz – conservative
```

---

## Colour order (BGR vs RGB)

If reds and blues look swapped on your display, set:

```c
.bgr_filter = true,
```

---

## Bitmap format

`tft_draw_bitmap()` expects **big-endian RGB565** pixel data, row-major.
When converting images use a tool like `ImageMagick`:

```bash
convert input.png -resize 128x160\! -type TrueColor \
        -define bmp:format=bmp3 output.bmp
```

Or generate C arrays with **lcd-image-converter** (export as RGB565 big-endian).

---

## License

MIT – use freely in personal and commercial projects.;