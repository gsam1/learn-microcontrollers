# Photo Frame — Waveshare ESP32-S3-Touch-LCD-7

Turns the [Waveshare ESP32-S3-Touch-LCD-7](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-7) into a smart photo frame:
it fetches the photo list from `http://192.168.1.18:8080/photos/`, shows one photo every 10 minutes,
and skips to the next one when the screen is touched. Photos are downloaded through imgproxy (port 8081), which
shrinks them to the screen, turns them upright and converts HEIC, so any phone photo can be dropped in `photos/` as is.

Built with ESP-IDF v6.1 in plain C (no Arduino, no LVGL).

## Build and flash

```sh
source ~/.espressif/tools/activate_idf_v6.1.sh
cd esp32/esp-display

idf.py -B build/default -DSDKCONFIG=build/default/sdkconfig menuconfig      # Photo Frame → Wi-Fi SSID/password
idf.py -B build/default -DSDKCONFIG=build/default/sdkconfig -p /dev/ttyACM0 flash monitor   # Ctrl+] exits the monitor
```

Gotchas hit while setting this up:

- **Always pass `-B build/default -DSDKCONFIG=...`.** With the `CMakePresets.json` in this folder, plain `idf.py build`
  creates a folder literally named `${sourceDir}` and fails.
- **Wi-Fi credentials live only in `build/default/sdkconfig`**, which is gitignored. Deleting `build/` means entering them again.
- **Board not showing up in `lsusb`?** Usually a charge-only USB-C cable. Either port works: `USB` shows up as `303a:1001`
  (the S3's built-in USB), `UART` as `1a86:55d3` (CH343 chip). Both appear as `/dev/ttyACM0`.
- **"Permission denied" on the port:** `sudo usermod -aG dialout $USER`, then log out and back in.
- **"Failed to connect" while flashing:** hold BOOT, tap RESET, release BOOT (download mode). Tap RESET afterwards to run.
- **Stuck in download mode after a reset over USB?** (`boot:0x0 (DOWNLOAD(USB/UART0))` in the log.) Resetting through the
  `USB` port's serial lines can do this. Flashing still works; unplug the board and power it up again to run the frame.

## The hardware

The board puts four parts that matter here on one PCB:

| Part | What it does | How the code talks to it |
|---|---|---|
| **ESP32-S3** (N16R8) | The processor, with Wi-Fi, 16 MB flash and 8 MB PSRAM (extra RAM) | — |
| **800×480 LCD** | The screen | 16 parallel data wires plus timing signals ("RGB interface") |
| **GT911** | Touch controller | I2C bus (SDA GPIO 8, SCL GPIO 9), INT on GPIO 4 |
| **CH422G** | IO expander, a chip that adds extra pins | Same I2C bus |

The CH422G is there because the ESP32 runs out of pins: the screen alone uses 20 of them. So the backlight (EXIO2),
the LCD reset (EXIO3) and the touch reset (EXIO1) are wired to the expander instead. The ESP32 switches them by sending
I2C messages to the expander. Oddly, the CH422G has no register map: each I2C *address* is a register
(0x24 = mode, 0x38 = output pin values).

## The files

```
esp-display/
├── CMakeLists.txt          project definition (ESP-IDF boilerplate)
├── sdkconfig.defaults      chip settings: 16 MB flash, PSRAM on, display stability tweaks
└── main/
    ├── Kconfig.projbuild   defines the menuconfig settings (Wi-Fi, URL, interval)
    ├── idf_component.yml   pulls in the JPEG decoder library from Espressif's component registry
    ├── board.c / board.h   the hardware: screen, touch, IO expander
    └── main.c              the frame itself: Wi-Fi, downloading, decoding, the loop
```

The split: `board.c` knows the pins and chips, and `main.c` knows the frame logic. `main.c` only calls four functions
from `board.h`: init, get the back buffer, present, and "was the screen touched?". A different screen would only need
a new `board.c`.

## What happens at boot (`app_main` in main.c)

1. **NVS init.** Wi-Fi keeps calibration data in a small flash storage area (NVS), so it has to start first.
2. **`board_init()`**:
   - Starts the I2C bus.
   - Tells the CH422G to make its pins outputs, then turns on the backlight.
   - Resets the touch chip. The INT pin is held low during reset, which is how the GT911 chooses its I2C address (0x5D).
   - Creates the LCD panel with **two frame buffers** in PSRAM.
   - Checks that the touch chip answers (falls back to address 0x14, otherwise runs without touch).
3. **`wifi_init()`** starts connecting in the background. An event handler sets a flag when it gets an IP address,
   and reconnects if the connection drops. Every HTTP request waits on that flag first.

## The main loop

```
forever:
    if we've shown every photo → re-download the list
    download photo → decode → draw
    wait 10 minutes, or less if the screen is touched
```

A photo that fails to download or decode is skipped after 5 seconds instead of leaving the old one up for 10 minutes.

**Getting the list.** The server (nginx with JSON autoindex) answers `/photos/` with JSON like
`[{"name":"a.jpg","type":"file",...}]`. Rather than pulling in a JSON library, `fetch_photo_list` just searches for
`"name":"` and keeps names ending in `.jpg`/`.jpeg`/`.heic`. That's fine for nginx's predictable output, but it would break on
arbitrary JSON. Re-reading the list each round means photos added or removed on the server get picked up.

**Downloading (`http_get`).** Photos come from imgproxy, not nginx: `url_for` builds
`CONFIG_FRAME_IMAGES_URL` + file name + `@jpg`, e.g.
`http://192.168.1.18:8081/insecure/rs:fit:800:480/plain/20240717_203536.jpg@jpg`. imgproxy's base URL already points at
the photos folder, so only the file name goes after `plain/`. It returns a baseline JPEG of at most 800×480, upright,
usually 20–100 KB instead of several MB.

 This reads the HTTP response into a PSRAM buffer. The server usually says how big the
file is (`Content-Length`), so the buffer is sized right first time. If it doesn't, the buffer doubles as data arrives,
up to a 6 MB cap (about what is left of the 8 MB PSRAM after the frame buffers).

**Decoding (`show_jpeg`).** `esp_new_jpeg` is Espressif's decoder, tuned for the S3's SIMD instructions.
- It reads the header first to get the photo's size.
- Phones save portraits as landscape pixels with an EXIF *Orientation* tag (6 = turn 90° clockwise, 8 = 270°).
  `exif_rotation` reads that tag and the decoder rotates while decoding, so portraits show upright (pillarboxed).
- If the photo is bigger than the screen, it works out a scale that fits while keeping proportions. The decoder
  shrinks the photo *while* decoding, which is much cheaper than decoding full size and shrinking afterwards.
- Output is **RGB565**: 16 bits per pixel, the same format the screen uses, so no conversion is needed.

Decoder limits to remember:
- Baseline JPEGs only; **progressive JPEGs fail**.
- Scaled sizes must be multiples of 8, so fitted photos can be a few pixels narrower than the screen.
- It can shrink at most 8×. Anything over ~6400 px wide is centred and cropped.

**Drawing (double buffering).** The screen is always showing buffer A. The code clears buffer B to black, copies the
photo into the middle, then calls `board_present()`, which tells the LCD driver to switch to B. Next time the roles swap.
You never see a half-drawn photo because the switch happens between frames.

**Touch (`board_touch_pressed`).** Every 30 ms the code reads one GT911 register (0x814E):
- Bit 7 means "new data is ready".
- The low 4 bits are the number of fingers on the screen.
- After reading, the code writes 0 to the register so the chip records the next reading.

The function only returns true on the change from no finger to finger. That way, holding your finger on the screen
advances one photo, not one every 30 ms.

## The non-obvious config (`sdkconfig.defaults`)

The screen is refreshed straight from PSRAM by DMA (the chip copying memory in hardware without the CPU), about 60 times
a second. Wi-Fi and the CPU compete with it for PSRAM access. If the screen loses that race, the picture glitches or
shifts sideways. Three settings protect against that:
- **Bounce buffers** (set in `board.c`) stage lines in fast internal RAM before they go to the screen.
- **64-byte cache lines** make the CPU's PSRAM accesses more efficient.
- **`LCD_RGB_RESTART_IN_VSYNC`** re-syncs the display at the start of each frame if it ever drifts.

The display timing (16 MHz pixel clock, porches of 8, sync pulses of 4) comes from Waveshare's own example code,
not the wiki. If the picture ever flickers or looks shifted, those numbers in `board.c` are the first thing to change.

The settings under **Photo Frame** in `menuconfig` come from `Kconfig.projbuild`. They turn into `CONFIG_FRAME_*`
constants that the code uses (`CONFIG_FRAME_INTERVAL_SEC` for the 10 minutes, `CONFIG_FRAME_PHOTOS_URL` for the list,
`CONFIG_FRAME_IMAGES_URL` for imgproxy).
