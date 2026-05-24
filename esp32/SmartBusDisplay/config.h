/**
 * ============================================================
 *  Smart Bus Stop Display — User Configuration
 *  ESP32 + ILI9488 3.5" TFT (Parallel 8-bit) + XPT2046 Touch
 * ============================================================
 *  Edit ONLY this file to match your hardware setup.
 */

#ifndef CONFIG_H
#define CONFIG_H

// ─── WiFi ────────────────────────────────────────────────────
#define WIFI_SSID         "YOUR_WIFI_SSID"
#define WIFI_PASSWORD     "YOUR_WIFI_PASSWORD"

// ─── Backend API ─────────────────────────────────────────────
// Your Node.js server IP + port (no trailing slash)
#define API_BASE_URL      "http://192.168.1.100:5000/api"

// MongoDB ObjectId of THIS bus stop (from /api/stops)
// Leave empty to use "nearest stop" auto-detection
#define STOP_ID           "YOUR_STOP_OBJECT_ID"
#define STOP_NAME         "Gandhipuram Bus Stand"
#define STOP_DISTRICT     "Coimbatore"

// ─── Timing ──────────────────────────────────────────────────
#define POLL_INTERVAL_MS      15000   // Live bus refresh (15 s)
#define CLOCK_INTERVAL_MS     1000    // Clock tick (1 s)
#define ALERT_INTERVAL_MS     30000   // Alert refresh (30 s)
#define SCHEDULE_INTERVAL_MS  60000   // Schedule refresh (1 min)
#define SCROLL_SPEED_MS       40      // Ticker scroll pixel delay (ms)
#define DIM_TIMEOUT_MS        180000  // Backlight dim after 3 min
#define SPLASH_DURATION_MS    3000    // Splash screen duration

// ─── Display ─────────────────────────────────────────────────
#define SCREEN_WIDTH   480
#define SCREEN_HEIGHT  320
#define ROTATION       1              // 1 = Landscape

// ILI9488 Parallel 8-bit Pin Map (change for your board)
#define TFT_CS   5
#define TFT_DC   27
#define TFT_WR   4
#define TFT_RD   2
#define TFT_RST  12
#define TFT_BL   13   // Backlight PWM

// Data bus GPIO pins (D0-D7)
#define TFT_D0  15
#define TFT_D1  17
#define TFT_D2  18
#define TFT_D3  19
#define TFT_D4  21
#define TFT_D5  22
#define TFT_D6  23
#define TFT_D7  26

// ─── Touch (XPT2046) ─────────────────────────────────────────
#define TOUCH_CS    33
#define TOUCH_IRQ   36    // Optional — connect to any input pin
#define TOUCH_THRESHOLD  500

// ─── Buzzer / Speaker ────────────────────────────────────────
#define BUZZER_PIN  25    // Optional — -1 to disable
#define BUZZER_TONE_HZ 1000

// ─── NTP ─────────────────────────────────────────────────────
// IST = UTC + 5:30 = 19800 seconds offset
#define NTP_SERVER    "pool.ntp.org"
#define NTP_OFFSET    19800
#define NTP_INTERVAL  3600000   // Re-sync every hour

// ─── Display Limits ──────────────────────────────────────────
#define MAX_BUSES       6     // Max bus rows on home screen
#define MAX_ALERTS      5     // Alert ticker buffer
#define MAX_ROUTES      8     // Search result limit
#define MAX_STOPS_ROUTE 20    // Stops shown in route detail

// ─── Version ─────────────────────────────────────────────────
#define FW_VERSION  "2.0.0"
#define FW_DATE     "2026-05-15"
#define DISPLAY_NAME "TN Smart Bus Display"

#endif // CONFIG_H
