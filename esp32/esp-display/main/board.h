#pragma once

#include <stdbool.h>
#include <stdint.h>

#define LCD_H_RES 800
#define LCD_V_RES 480

// Bring up I2C, the CH422G IO expander, the RGB panel (backlight on) and the GT911 touch controller.
void board_init(void);

// Back buffer to draw the next frame into (RGB565, LCD_H_RES * LCD_V_RES pixels).
uint16_t *board_back_buffer(void);

// Show the back buffer and swap it with the front one.
void board_present(void);

// True once per new finger-down on the touch panel.
bool board_touch_pressed(void);
