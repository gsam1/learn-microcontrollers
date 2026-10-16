// Smart photo frame: lists the JPEGs on a web server, shows one every CONFIG_FRAME_INTERVAL_SEC,
// and skips to the next one when the screen is touched.

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "board.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_jpeg_dec.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "frame";

#define MAX_PHOTOS    512
#define MAX_DOWNLOAD  (4 * 1024 * 1024)
#define RETRY_DELAY_MS 5000

static EventGroupHandle_t s_wifi_events;
#define WIFI_CONNECTED_BIT BIT0

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && (id == WIFI_EVENT_STA_START || id == WIFI_EVENT_STA_DISCONNECTED)) {
        xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = data;
        ESP_LOGI(TAG, "got IP " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init(void)
{
    s_wifi_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));

    wifi_config_t cfg = {0};
    strlcpy((char *)cfg.sta.ssid, CONFIG_FRAME_WIFI_SSID, sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, CONFIG_FRAME_WIFI_PASSWORD, sizeof(cfg.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// GET url into a PSRAM buffer. Caller frees *out.
static esp_err_t http_get(const char *url, uint8_t **out, int *out_len)
{
    xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    esp_http_client_config_t cfg = {.url = url, .timeout_ms = 15000};
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        goto done;
    }
    int64_t content_len = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGE(TAG, "%s: HTTP %d", url, status);
        err = ESP_FAIL;
        goto done;
    }

    int cap = content_len > 0 ? content_len : 64 * 1024;
    int len = 0;
    uint8_t *buf = NULL;
    for (;;) {
        if (cap > MAX_DOWNLOAD) {
            ESP_LOGE(TAG, "%s: larger than %d bytes", url, MAX_DOWNLOAD);
            err = ESP_ERR_NO_MEM;
            break;
        }
        // +1 so text responses can be NUL-terminated
        uint8_t *grown = heap_caps_realloc(buf, cap + 1, MALLOC_CAP_SPIRAM);
        if (!grown) {
            err = ESP_ERR_NO_MEM;
            break;
        }
        buf = grown;
        int n = esp_http_client_read(client, (char *)buf + len, cap - len);
        if (n < 0) {
            err = ESP_FAIL;
            break;
        }
        len += n;
        if (n == 0 || (content_len > 0 && len >= content_len)) {
            break;
        }
        if (len == cap) {
            cap *= 2;
        }
    }
    if (err == ESP_OK) {
        buf[len] = '\0';
        *out = buf;
        *out_len = len;
    } else {
        free(buf);
    }

done:
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GET %s failed: %s", url, esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    return err;
}

static bool is_jpeg_name(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot && (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0);
}

// Parse nginx's JSON autoindex: [{ "name":"a.jpg", "type":"file", ... }, ...]
static int fetch_photo_list(char *names[], int max)
{
    uint8_t *body;
    int len;
    if (http_get(CONFIG_FRAME_PHOTOS_URL, &body, &len) != ESP_OK) {
        return 0;
    }
    int count = 0;
    const char *p = (const char *)body;
    while (count < max && (p = strstr(p, "\"name\":\"")) != NULL) {
        p += strlen("\"name\":\"");
        const char *end = strchr(p, '"');
        if (!end) {
            break;
        }
        char *name = strndup(p, end - p);
        if (is_jpeg_name(name)) {
            names[count++] = name;
        } else {
            free(name);
        }
        p = end;
    }
    free(body);
    ESP_LOGI(TAG, "%d photos listed", count);
    return count;
}

static void url_for(const char *name, char *url, size_t size)
{
    size_t n = strlcpy(url, CONFIG_FRAME_PHOTOS_URL, size);
    for (const char *c = name; *c && n + 4 < size; c++) {
        if (isalnum((unsigned char)*c) || strchr("-_.~", *c)) {
            url[n++] = *c;
        } else {
            n += snprintf(url + n, size - n, "%%%02X", (unsigned char)*c);
        }
    }
    url[n] = '\0';
}

// Decoder scale target must be a multiple of 8 and at most 8x smaller than the source.
static int scaled_dim(int src, float factor)
{
    int d = (int)(src * factor) & ~7;
    while (d * 8 < src) {
        d += 8;
    }
    return d < 8 ? 8 : d;
}

// Decode a JPEG, fit it to the screen (letterboxed on black) and show it.
static esp_err_t show_jpeg(uint8_t *data, int len)
{
    jpeg_dec_config_t cfg = DEFAULT_JPEG_DEC_CONFIG();
    cfg.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;

    jpeg_dec_handle_t dec;
    if (jpeg_dec_open(&cfg, &dec) != JPEG_ERR_OK) {
        return ESP_FAIL;
    }
    jpeg_dec_io_t io = {.inbuf = data, .inbuf_len = len};
    jpeg_dec_header_info_t info;
    esp_err_t err = ESP_FAIL;
    uint8_t *pixels = NULL;

    if (jpeg_dec_parse_header(dec, &io, &info) != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "bad JPEG header (progressive JPEGs are not supported)");
        goto done;
    }
    int w = info.width, h = info.height;
    if (w > LCD_H_RES || h > LCD_V_RES) {
        float fx = (float)LCD_H_RES / w, fy = (float)LCD_V_RES / h;
        float f = fx < fy ? fx : fy;
        w = scaled_dim(info.width, f);
        h = scaled_dim(info.height, f);
        // Re-open with the scale setting; the header has to be parsed again after that.
        jpeg_dec_close(dec);
        cfg.scale.width = w;
        cfg.scale.height = h;
        if (jpeg_dec_open(&cfg, &dec) != JPEG_ERR_OK) {
            return ESP_FAIL;
        }
        io = (jpeg_dec_io_t){.inbuf = data, .inbuf_len = len};
        if (jpeg_dec_parse_header(dec, &io, &info) != JPEG_ERR_OK) {
            goto done;
        }
    }

    int out_len;
    if (jpeg_dec_get_outbuf_len(dec, &out_len) != JPEG_ERR_OK) {
        goto done;
    }
    pixels = jpeg_calloc_align(out_len, 16);
    if (!pixels) {
        err = ESP_ERR_NO_MEM;
        goto done;
    }
    io.outbuf = pixels;
    int64_t t0 = esp_timer_get_time();
    jpeg_error_t jerr = jpeg_dec_process(dec, &io);
    if (jerr != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "decode failed: %d", jerr);
        goto done;
    }
    ESP_LOGI(TAG, "decoded %dx%d -> %dx%d in %lld ms", info.width, info.height, w, h,
             (esp_timer_get_time() - t0) / 1000);

    // Center the image; crop if it is still larger than the screen (only for > 8x oversized photos).
    uint16_t *fb = board_back_buffer();
    memset(fb, 0, LCD_H_RES * LCD_V_RES * sizeof(uint16_t));
    const uint16_t *src = (const uint16_t *)pixels;
    int copy_w = w < LCD_H_RES ? w : LCD_H_RES;
    int copy_h = h < LCD_V_RES ? h : LCD_V_RES;
    int dst_x = (LCD_H_RES - copy_w) / 2, dst_y = (LCD_V_RES - copy_h) / 2;
    int src_x = (w - copy_w) / 2, src_y = (h - copy_h) / 2;
    for (int y = 0; y < copy_h; y++) {
        memcpy(fb + (dst_y + y) * LCD_H_RES + dst_x, src + (src_y + y) * w + src_x, copy_w * sizeof(uint16_t));
    }
    board_present();
    err = ESP_OK;

done:
    jpeg_free_align(pixels);
    jpeg_dec_close(dec);
    return err;
}

static esp_err_t show_photo(const char *name)
{
    char url[512];
    url_for(name, url, sizeof(url));
    ESP_LOGI(TAG, "showing %s", url);

    uint8_t *data;
    int len;
    esp_err_t err = http_get(url, &data, &len);
    if (err == ESP_OK) {
        err = show_jpeg(data, len);
        free(data);
    }
    return err;
}

// Sleep up to ms, returning early on a touch.
static void wait_or_touch(int ms)
{
    int64_t until = esp_timer_get_time() + (int64_t)ms * 1000;
    while (esp_timer_get_time() < until) {
        if (board_touch_pressed()) {
            ESP_LOGI(TAG, "touch -> next photo");
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    board_init();
    wifi_init();

    static char *names[MAX_PHOTOS];
    int count = 0, index = 0;
    for (;;) {
        // Re-read the listing at the start of every round so added/removed photos are picked up.
        if (index >= count) {
            for (int i = 0; i < count; i++) {
                free(names[i]);
            }
            count = fetch_photo_list(names, MAX_PHOTOS);
            index = 0;
            if (count == 0) {
                vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
                continue;
            }
        }
        bool ok = show_photo(names[index++]) == ESP_OK;
        // A failed photo is skipped after a short pause instead of leaving the old one up for the full interval.
        wait_or_touch(ok ? CONFIG_FRAME_INTERVAL_SEC * 1000 : RETRY_DELAY_MS);
    }
}
