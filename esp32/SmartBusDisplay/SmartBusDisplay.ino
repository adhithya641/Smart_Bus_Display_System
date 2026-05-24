/**
 * Smart Bus Stop Display v2.0
 * ESP32 + ILI9488 3.5" TFT (Parallel) + LVGL + XPT2046 Touch
 *
 * Libraries needed (install via Arduino Library Manager):
 *  - LVGL (>= 8.3)
 *  - Arduino_GFX_Library (by Moon On Our Nation)
 *  - ArduinoJson (>= 7.x)
 *  - XPT2046_Touchscreen
 *  - NTPClient
 *  - WiFi, HTTPClient (ESP32 built-in)
 *
 * File structure:
 *   SmartBusDisplay.ino  ← this file (setup/loop)
 *   config.h             ← WiFi, API, pins (EDIT THIS)
 *   ui.h                 ← colors & layout constants
 *   api.h / api.cpp      ← REST client
 *   screens.h / screens.cpp ← LVGL screen builders
 */

#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <XPT2046_Touchscreen.h>

#include "config.h"
#include "ui.h"
#include "api.h"
#include "screens.h"

// ─── Display Driver (ILI9488 Parallel 8-bit) ──────────────────
Arduino_DataBus *bus = new Arduino_SWPAR8(TFT_DC, TFT_CS, TFT_WR, TFT_RD,
    TFT_D0, TFT_D1, TFT_D2, TFT_D3,
    TFT_D4, TFT_D5, TFT_D6, TFT_D7);
Arduino_GFX *gfx = new Arduino_ILI9488(bus, TFT_RST, ROTATION, false);

// ─── Touch ────────────────────────────────────────────────────
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// ─── NTP ──────────────────────────────────────────────────────
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER, NTP_OFFSET, NTP_INTERVAL);

// ─── LVGL display buffer ──────────────────────────────────────
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_WIDTH * 20];
static lv_color_t buf2[SCREEN_WIDTH * 20];

// ─── Timers ───────────────────────────────────────────────────
unsigned long lastPoll     = 0;
unsigned long lastAlert    = 0;
unsigned long lastSchedule = 0;
unsigned long lastClock    = 0;
unsigned long lastActivity = 0;
bool isDimmed = false;

// ─── LVGL Flush Callback ──────────────────────────────────────
void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px_map) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    lv_disp_flush_ready(drv);
}

// ─── LVGL Touch Callback ──────────────────────────────────────
void lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        // Map raw touch coords to screen pixels (calibrate as needed)
        data->point.x = map(p.x, 200, 3800, 0, SCREEN_WIDTH - 1);
        data->point.y = map(p.y, 200, 3800, 0, SCREEN_HEIGHT - 1);
        data->state = LV_INDEV_STATE_PR;
        lastActivity = millis();
        if (isDimmed) {
            analogWrite(TFT_BL, 255);
            isDimmed = false;
        }
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// ─── WiFi Connect ─────────────────────────────────────────────
void connectWiFi() {
    Serial.print("[WiFi] Connecting");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 40) {
        delay(500); Serial.print("."); tries++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Connected: " + WiFi.localIP().toString());
    } else {
        Serial.println("\n[WiFi] Failed — running offline");
    }
}

// ─── SETUP ────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Smart Bus Display v" FW_VERSION " ===");

    // Backlight
    pinMode(TFT_BL, OUTPUT);
    analogWrite(TFT_BL, 255);

    // Buzzer
    if (BUZZER_PIN >= 0) pinMode(BUZZER_PIN, OUTPUT);

    // Init display
    gfx->begin();
    gfx->fillScreen(BLACK);

    // Init LVGL
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, SCREEN_WIDTH * 20);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = SCREEN_WIDTH;
    disp_drv.ver_res  = SCREEN_HEIGHT;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Init touch
    ts.begin();
    ts.setRotation(ROTATION);
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_cb;
    lv_indev_drv_register(&indev_drv);

    // Splash screen (uses raw GFX before LVGL screens load)
    screens_showSplash(gfx);
    delay(SPLASH_DURATION_MS);

    // Connect WiFi
    connectWiFi();

    // NTP
    timeClient.begin();
    timeClient.update();

    // Build LVGL UI
    screens_init(&timeClient);

    // Initial data fetch
    api_healthCheck();
    api_fetchLiveBuses(STOP_ID);
    api_fetchAlerts();
    api_fetchSchedules();
    screens_updateHome();
    screens_updateTicker();
    screens_updateSchedule();

    lastActivity = millis();
    Serial.println("[Main] Ready.");
}

// ─── LOOP ─────────────────────────────────────────────────────
void loop() {
    lv_timer_handler();   // LVGL tasks (call as fast as possible)
    delay(5);

    unsigned long now = millis();

    // WiFi watchdog
    if (WiFi.status() != WL_CONNECTED) {
        if (now % 30000 < 100) connectWiFi();
    }

    // Clock update (every second)
    if (now - lastClock >= CLOCK_INTERVAL_MS) {
        lastClock = now;
        timeClient.update();
        screens_updateClock(&timeClient);
    }

    // Live bus data refresh
    if (now - lastPoll >= POLL_INTERVAL_MS) {
        lastPoll = now;
        if (WiFi.status() == WL_CONNECTED) {
            api_fetchLiveBuses(STOP_ID);
            screens_updateHome();
            screens_setApiStatus(g_apiOnline);
        }
    }

    // Alert refresh
    if (now - lastAlert >= ALERT_INTERVAL_MS) {
        lastAlert = now;
        if (WiFi.status() == WL_CONNECTED) {
            api_fetchAlerts();
            screens_updateTicker();
        }
    }

    // Schedule refresh
    if (now - lastSchedule >= SCHEDULE_INTERVAL_MS) {
        lastSchedule = now;
        if (WiFi.status() == WL_CONNECTED) {
            api_fetchSchedules();
            screens_updateSchedule();
        }
    }

    // Auto-dim backlight after inactivity
    if (!isDimmed && (now - lastActivity > DIM_TIMEOUT_MS)) {
        analogWrite(TFT_BL, 50);
        isDimmed = true;
    }
}
