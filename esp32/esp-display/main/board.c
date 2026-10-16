// Hardware for the Waveshare ESP32-S3-Touch-LCD-7 (800x480 RGB panel, GT911 touch, CH422G IO expander).
// Pinout: https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-7

#include "board.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "board";

#define I2C_SDA     8
#define I2C_SCL     9
#define TOUCH_INT   4

// The CH422G has no register map: each I2C "address" is a register.
#define CH422G_MODE_ADDR   0x24
#define CH422G_OUTPUT_ADDR 0x38
#define EXIO_TP_RST  (1 << 1)
#define EXIO_DISP    (1 << 2)  // backlight
#define EXIO_LCD_RST (1 << 3)
#define EXIO_USB_SEL (1 << 5)

#define GT911_ADDR       0x5D
#define GT911_ADDR_ALT   0x14
#define GT911_REG_STATUS 0x814E

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_exio_mode, s_exio_out, s_touch;
static esp_lcd_panel_handle_t s_panel;
static uint16_t *s_fb[2];
static int s_back;  // index of the buffer not currently shown
static bool s_touch_down;

static i2c_master_dev_handle_t add_dev(uint16_t addr)
{
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    i2c_master_dev_handle_t dev;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(s_bus, &cfg, &dev));
    return dev;
}

static void exio_write(uint8_t value)
{
    ESP_ERROR_CHECK(i2c_master_transmit(s_exio_out, &value, 1, 100));
}

static void exio_init_and_reset_touch(void)
{
    s_exio_mode = add_dev(CH422G_MODE_ADDR);
    s_exio_out = add_dev(CH422G_OUTPUT_ADDR);
    uint8_t mode = 0x01;  // IO0-7 as outputs
    ESP_ERROR_CHECK(i2c_master_transmit(s_exio_mode, &mode, 1, 100));

    // GT911 reset with INT held low latches I2C address 0x5D (same sequence as the Waveshare demo).
    exio_write(EXIO_DISP | EXIO_LCD_RST | EXIO_USB_SEL);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_direction(TOUCH_INT, GPIO_MODE_OUTPUT);
    gpio_set_level(TOUCH_INT, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    exio_write(EXIO_TP_RST | EXIO_DISP | EXIO_LCD_RST | EXIO_USB_SEL);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_direction(TOUCH_INT, GPIO_MODE_INPUT);
}

static void touch_init(void)
{
    uint16_t addr = GT911_ADDR;
    if (i2c_master_probe(s_bus, addr, 100) != ESP_OK) {
        addr = GT911_ADDR_ALT;
        if (i2c_master_probe(s_bus, addr, 100) != ESP_OK) {
            ESP_LOGE(TAG, "GT911 not found, touch disabled");
            return;
        }
    }
    ESP_LOGI(TAG, "GT911 at 0x%02X", addr);
    s_touch = add_dev(addr);
}

static void lcd_init(void)
{
    esp_lcd_rgb_panel_config_t cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_width = 16,
        .num_fbs = 2,
        .bounce_buffer_size_px = 10 * LCD_H_RES,
        .dma_burst_size = 64,
        .hsync_gpio_num = 46,
        .vsync_gpio_num = 3,
        .de_gpio_num = 5,
        .pclk_gpio_num = 7,
        .disp_gpio_num = -1,
        .data_gpio_nums = {
            14, 38, 18, 17, 10,     // B3-B7
            39, 0, 45, 48, 47, 21,  // G2-G7
            1, 2, 42, 41, 40,       // R3-R7
        },
        .timings = {
            .pclk_hz = 16 * 1000 * 1000,
            .h_res = LCD_H_RES,
            .v_res = LCD_V_RES,
            .hsync_pulse_width = 4,
            .hsync_back_porch = 8,
            .hsync_front_porch = 8,
            .vsync_pulse_width = 4,
            .vsync_back_porch = 8,
            .vsync_front_porch = 8,
            .flags.pclk_active_neg = true,
        },
        .flags.fb_in_psram = true,
    };
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&cfg, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(s_panel, 2, (void **)&s_fb[0], (void **)&s_fb[1]));
    s_back = 1;  // the driver starts out showing fb 0
}

void board_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &s_bus));

    exio_init_and_reset_touch();
    lcd_init();
    touch_init();
}

uint16_t *board_back_buffer(void)
{
    return s_fb[s_back];
}

void board_present(void)
{
    // Drawing with a pointer to one of the driver's own frame buffers just switches the scan-out to it.
    esp_lcd_panel_draw_bitmap(s_panel, 0, 0, LCD_H_RES, LCD_V_RES, s_fb[s_back]);
    s_back ^= 1;
}

bool board_touch_pressed(void)
{
    if (!s_touch) {
        return false;
    }
    uint8_t reg[2] = {GT911_REG_STATUS >> 8, GT911_REG_STATUS & 0xFF};
    uint8_t status;
    if (i2c_master_transmit_receive(s_touch, reg, 2, &status, 1, 50) != ESP_OK) {
        return false;
    }
    if (!(status & 0x80)) {
        return false;  // no new data since the last poll; finger state unchanged
    }
    uint8_t clear[3] = {reg[0], reg[1], 0};
    i2c_master_transmit(s_touch, clear, 3, 50);

    bool down = (status & 0x0F) > 0;
    bool pressed = down && !s_touch_down;
    s_touch_down = down;
    return pressed;
}
