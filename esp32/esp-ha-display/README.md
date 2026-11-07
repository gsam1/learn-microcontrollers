# Home Assistant Remote — 4" ESP32-32E display

A 3×2 grid of touch tiles that toggle Home Assistant entities. A tile is amber when its entity is on, grey when off,
and dimmed when the entity is unavailable. Built with [ESPHome](https://esphome.io) + LVGL, so it talks to Home Assistant
through the native ESPHome integration (no access token in the firmware).

| | | |
|---|---|---|
| Living Room Lights `switch.relay_controler_switch` | Kitchen Lights `switch.kitchen_lights_switch` | Lamp `switch.living_room_lamp_plug` |
| TV LED `light.living_room_living_room_tv_led` | Air Purifier `fan.xiaomi_cpa4_d0ea_air_purifier` | AC `climate.hallway_ac` |

To change a tile, edit both its `text_sensor` entry (which shows the state) and its button's `homeassistant.action`
(which toggles it) in `ha-remote.yaml`.

## Build and flash

```sh
cd esp32/esp-ha-display
python3 -m venv .venv && .venv/bin/pip install esphome          # once

.venv/bin/esphome run ha-remote.yaml --device /dev/ttyUSB0       # over USB
.venv/bin/esphome run ha-remote.yaml --device ha-remote.local    # over Wi-Fi (OTA) once it's running
```

`secrets.yaml` (gitignored) holds `wifi_ssid`, `wifi_password` and `api_key` (32 random bytes, base64:
`head -c32 /dev/urandom | base64`). OTA uploads use the same key.

In Home Assistant the device is added under **Settings → Devices & services → ESPHome** (host `ha-remote.local`, the
`api_key` as encryption key). **"Allow the device to perform Home Assistant actions"** must be enabled in that
integration's options, or taps do nothing.

## The hardware

ESP32-WROOM-32E (no PSRAM, 4 MB flash) with a CH340 USB-serial chip (`1a86:7523`, `/dev/ttyUSB0`). Nothing printed
on the board identified it, so the pins below were found by testing:

| Part | Pins |
|---|---|
| ST7796 480×320 screen (SPI) | SCLK 14, MOSI 13, MISO 12, CS 15, DC 2 |
| Backlight | GPIO27 (PWM) |
| XPT2046 touch, **same SPI bus** as the screen | CS 33, IRQ 36 |

Gotchas hit while setting this up:

- **It looks like the 2.8" "Cheap Yellow Display" but isn't.** The CYD config (ILI9341, backlight 21, touch on its own
  SPI bus 25/32/39) gives a blank screen and a touch chip that reads all zeros.
- **Calibrate touch in the panel's native portrait orientation**, not landscape. LVGL's `rotation: 90` rotates touch
  together with the picture, so a transform that is correct in landscape gets rotated a second time (tapping a tile
  toggled the one 90° around the centre). With LVGL rotation the right transform is just `mirror_y: true`.
  Calibration values came from tapping the four corners and reading `raw_x`/`raw_y` from the `touch x=… raw_x=…` log lines.
- **LVGL draw buffer:** a full-screen buffer (300 KB) doesn't fit without PSRAM, so `buffer_size: 12%`.
- `gpio_pullup_en ... GPIO number error` at boot is harmless: the touch driver asks for a pull-up on GPIO36, which
  is input-only and has an external one anyway.
