/*
 * ================================================================
 *  SMART BUS STOP INFORMATION DISPLAY SYSTEM
 *  ESP32 + ILI9488 3.5" TFT (8-Bit Parallel) + Resistive Touch
 * ================================================================
 *
 *  PURPOSE:
 *    Professional public-transport bus stop display. Installed at a
 *    physical bus stop, it shows only buses that serve THIS stop,
 *    with live ETA, route maps, alerts, and touch navigation.
 *
 *  HARDWARE:
 *    ESP32 38-pin · ILI9488 480x320 · 8-bit parallel · Resistive Touch
 *    SD card on VSPI · Touch shares pins with display (time-div mux)
 *
 *  FEATURES:
 *    • 5 touch-navigable pages (Home, Buses, Detail, Map, Settings)
 *    • Live data from backend API with 8-second polling
 *    • SD card caching for offline fallback
 *    • NTP time sync (IST)
 *    • Animated premium dark "smart city" UI
 *    • Scrollable bus arrival list with ETA countdown
 *    • Route progress visualization
 *    • Alert banners
 *    • Auto WiFi reconnect
 *
 *  LIBRARIES (install via Arduino Library Manager):
 *    Arduino_GFX_Library   — Display driver
 *    TouchScreen           — Resistive touch (Adafruit)
 *    ArduinoJson           — JSON parsing (v6 or v7)
 *    WiFi, HTTPClient      — Built-in ESP32
 *    SD, SPI               — Built-in
 *
 *  Author : Smart Bus Tracking Project
 *  Date   : 2026-05-24
 * ================================================================
 */

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <TouchScreen.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>  

/* ================================================================
 *  USER CONFIGURATION — EDIT THESE FOR YOUR DEPLOYMENT
 * ================================================================ */

const char* WIFI_SSID     = "Your_wifi_name";
const char* WIFI_PASSWORD = "Your_wifi_password";

// Backend server IP (your local network or Render URL)
const char* API_HOST      = "Your_backend_server_address/api";

// Stop ID — MongoDB ObjectId of the bus stop this display serves.
// To find your stop ID:
//   1. Open http://10.45.21.213:5000/api/stops in a browser
//   2. Find your stop and copy the "_id" field
//   3. Paste it below
// Leave empty ("") to show ALL buses (demo mode).
const char* STOP_ID       = "6a13011258c8b180b76e115f";

// NTP timezone offset in seconds (IST = UTC+5:30 = 19800)
const long  GMT_OFFSET    = 19800;

// How often to poll the API (milliseconds)
const unsigned long POLL_INTERVAL = 8000;

/* ================================================================
 *  PIN DEFINITIONS (match your physical wiring)
 * ================================================================ */

// TFT Control
#define TFT_RST   32
#define TFT_CS    33
#define TFT_RS    15    // DC
#define TFT_WR    4

// TFT 8-Bit Parallel Data (D0-D7)
#define TFT_D0    21
#define TFT_D1    13
#define TFT_D2    26
#define TFT_D3    25
#define TFT_D4    17
#define TFT_D5    16
#define TFT_D6    27
#define TFT_D7    14

// Hardware Buttons
#define BTN_UP      34
#define BTN_DOWN    35
#define BTN_SELECT  5
#define BTN_BACK    2

#define BTN_DEBOUNCE_MS 200

// Resistive Touch (SHARED with display!)
#define XP        21    // = TFT_D0
#define YP        33    // = TFT_CS
#define XM        15    // = TFT_RS
#define YM        13    // = TFT_D1

/* ================================================================
 *  TOUCH CALIBRATION
 * ================================================================ */
#define TS_MINX           150
#define TS_MAXX           920
#define TS_MINY           120
#define TS_MAXY           940
#define TS_MIN_PRESSURE   200
#define TS_MAX_PRESSURE   1200
#define TOUCH_DEBOUNCE    300
#define TOUCH_POLL_MS     50

// SD Card (VSPI)
#define SD_CS     23
#define SD_MOSI   22
#define SD_MISO   19
#define SD_SCK    18

/* ================================================================
 *  SCREEN LAYOUT
 * ================================================================ */
#define SCR_W             480
#define SCR_H             320
#define STATUS_H          26
#define NAV_H             48
#define CONTENT_Y         STATUS_H
#define CONTENT_H         (SCR_H - STATUS_H - NAV_H)
#define NAV_Y             (SCR_H - NAV_H)

// Bus card dimensions
#define CARD_H            66
#define CARD_GAP          6
#define CARD_STEP         (CARD_H + CARD_GAP)
#define VISIBLE_CARDS     3

// Button sizes
#define BTN_H             40
#define BTN_R             6

/* ================================================================
 *  COLOR PALETTE — Premium Dark "Smart City" Theme (RGB565)
 * ================================================================ */

// Backgrounds
#define C_BG              0x0924    // #0B1220  deep space
#define C_CARD            0x1125    // #151D2B  card panel
#define C_CARD_HI         0x1968    // #1E2D42  card hover/border
#define C_HEADER          0x0903    // #0D1218  header/nav
#define C_DIVIDER         0x18E3    // #1C1C1C  hairline

// Accent
#define C_ACCENT          0x065F    // #00CBFF  electric cyan
#define C_ACCENT_DIM      0x0370    // #006D80  dark cyan
#define C_ACCENT2         0x325F    // #3049FF  blue

// Text
#define C_WHITE           0xFFFF
#define C_LIGHT           0xC638    // #C8C8C0
#define C_DIM             0x7BEF    // #7B7B78
#define C_MUTED           0x4228    // #404040

// Status
#define C_GREEN           0x2EC8    // #2CDA40  approaching
#define C_GREEN_BR        0x07E0    // bright green
#define C_YELLOW          0xFD20    // #FFB400  delayed
#define C_RED             0xF800    // #FF0000  critical
#define C_ORANGE          0xFC60    // #FF8C00
#define C_CYAN            0x07FF

// Bus type colors
#define C_BUS_ORD         0x2EC8    // green  — ordinary
#define C_BUS_EXP         0xFC60    // orange — express
#define C_BUS_DLX         0x325F    // blue   — deluxe
#define C_BUS_AC          0x8C1F    // purple — A/C

// UI elements
#define C_MODAL_BG        0x18C3
#define C_BTN_PRI         0x065F    // cyan
#define C_BTN_SEC         0x4228    // dark grey
#define C_NAV_ACTIVE      0x065F    // cyan for active tab
#define C_PULSE           0x07E0    // live pulse color

/* ================================================================
 *  DATA STRUCTURES
 * ================================================================ */

#define MAX_BUSES         8
#define MAX_ALERTS        3
#define MAX_STOPS_SETUP   10

struct BusData {
    char busId[16];
    char name[32];
    char route[48];
    char routeNum[10];
    char type[12];        // ordinary, express, deluxe, ac
    int  eta;             // minutes
    float distance;       // km
    int  speed;           // km/h
    char status[12];      // on_time, delayed, early
    char confidence[8];   // high, medium, low
};

struct AlertData {
    char type[16];
    char title[48];
    char message[80];
    char severity[12];
};

struct StopEntry {
    char id[28];          // MongoDB ObjectId
    char name[40];
    char district[24];
    char code[12];
};

// Navigation pages
enum Page {
    PAGE_SPLASH,
    PAGE_CONNECTING,
    PAGE_HOME,
    PAGE_BUSLIST,
    PAGE_DETAIL,
    PAGE_MAP,
    PAGE_SETTINGS
};

// Navigation items for focus mapping
enum NavAction {
    ACT_NONE = 0,
    ACT_HOME_BUSES,
    ACT_HOME_NEXT,
    ACT_BUS_SELECT,
    ACT_DETAIL_MAP,
    ACT_DETAIL_BACK,
    ACT_MAP_BACK,
    ACT_SET_WIFI,
    ACT_SET_REFRESH,
    ACT_NAV_HOME,
    ACT_NAV_BUSES,
    ACT_NAV_MAP,
    ACT_NAV_SETTINGS,
    ACT_SCROLL_UP,
    ACT_SCROLL_DOWN
};

struct FocusRegion {
    int16_t x, y, w, h;
    NavAction action;
    int8_t data; // e.g. bus index
};

/* ================================================================
 *  GLOBAL STATE
 * ================================================================ */

// Display hardware
Arduino_DataBus *dataBus = new Arduino_ESP32PAR8Q(
    TFT_RS, TFT_CS, TFT_WR, GFX_NOT_DEFINED,
    TFT_D0, TFT_D1, TFT_D2, TFT_D3,
    TFT_D4, TFT_D5, TFT_D6, TFT_D7
);
Arduino_GFX *gfx = new Arduino_ILI9488(dataBus, TFT_RST, 1, false);
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
SPIClass sdSPI(VSPI);

// Pages
Page    currentPage     = PAGE_SPLASH;
Page    previousPage    = PAGE_HOME;
bool    needsRedraw     = true;
bool    needsStatusBar  = false;

// Bus data
BusData    busList[MAX_BUSES];
uint8_t    busCount       = 0;
AlertData  alertList[MAX_ALERTS];
uint8_t    alertCount     = 0;
char       stopName[40]   = "Bus Stop";
char       stopDistrict[24] = "";
char       stopCode[12]   = "";
char       activeStopId[28] = "";

// Scroll state
int8_t     scrollOffset   = 0;
int8_t     selectedBus    = -1;

// Connectivity
bool       wifiConnected  = false;
bool       dataAvailable  = false;
bool       sdReady        = false;
unsigned long lastPollTime     = 0;
unsigned long lastDataUpdate   = 0;
uint16_t   fetchFailCount     = 0;

// Button state
bool btnUpPressed = false, btnDownPressed = false, btnSelectPressed = false, btnBackPressed = false;
unsigned long lastButtonPress = 0;

// Touch state
unsigned long lastTouchTime    = 0;
unsigned long lastTouchPoll    = 0;
bool          touchActive      = false;

// Focus system
int8_t focusedItem = 0;
int8_t maxFocusItems = 0;

#define MAX_REGIONS 20
FocusRegion regions[MAX_REGIONS];
uint8_t regionCount = 0;

// Animation
unsigned long lastAnimTime     = 0;
uint8_t       animFrame        = 0;
unsigned long lastClockUpdate  = 0;

/* ================================================================
 *  FUNCTION PROTOTYPES
 * ================================================================ */

// Core
void   restoreDisplayPins();
bool   readTouch(int16_t &tx, int16_t &ty);
void   handleTouch(int16_t tx, int16_t ty);
void   readButtons();
void   executeAction(NavAction action, int8_t data);
void   navigateTo(Page page);

// Focus regions
void   clearFocus();
void   addFocus(int16_t x, int16_t y, int16_t w, int16_t h, NavAction action, int8_t data = 0);
void   drawHighlights();

// Connectivity
void   connectWiFi();
void   syncTime();
void   fetchBusData();
void   fetchAllBuses();
void   parseDisplayResponse(const String &json);
void   parseAllBusesResponse(const String &json);

// SD Card
bool   initSD();
void   cacheToSD(const String &json);
bool   loadFromSD();

// Drawing — pages
void   drawSplash();
void   drawConnecting(int attempt, int maxAttempts);
void   drawHomePage();
void   drawBusListPage();
void   drawDetailPage();
void   drawMapPage();
void   drawSettingsPage();

// Drawing — shared elements
void   drawStatusBar();
void   drawNavBar();
void   drawAlertBanner();

// Drawing — UI primitives
void   drawRoundCard(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t bg, uint16_t border);
void   drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const char* label, uint16_t bg, uint16_t fg);
void   drawBusIconGeo(int16_t x, int16_t y, int16_t sz, uint16_t col);
void   drawStatusDot(int16_t x, int16_t y, const char* status);
void   drawThickH(int16_t x, int16_t y, int16_t w, uint8_t t, uint16_t c);
void   drawThickLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t t, uint16_t c);
void   drawSpinner(int16_t cx, int16_t cy, uint8_t frame);
void   drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, float pct, uint16_t fg);
void   centerText(const char* txt, int16_t y, uint8_t sz, uint16_t col);
uint16_t busTypeColor(const char* type);
const char* statusLabel(const char* status);

// Time
void   getTimeStr(char* buf, size_t len);
void   getDateStr(char* buf, size_t len);

/* ================================================================
 *                     S E T U P
 * ================================================================ */

void setup() {
    Serial.begin(115200);

    // Configure ESP32 ADC to 10-bit to match Adafruit TouchScreen library
    // This fixes the touch pressure calculation (z = 1023 - (z2-z1))
    analogReadResolution(10);
    
    // Initialize buttons
    pinMode(BTN_UP, INPUT);
    pinMode(BTN_DOWN, INPUT);
    pinMode(BTN_SELECT, INPUT_PULLUP);
    pinMode(BTN_BACK, INPUT_PULLUP);

    Serial.println(F("\n╔══════════════════════════════════════╗"));
    Serial.println(F("║  SMART BUS STOP DISPLAY — Booting    ║"));
    Serial.println(F("╚══════════════════════════════════════╝"));

    // Copy configured stop ID
    strncpy(activeStopId, STOP_ID, sizeof(activeStopId));

    // Initialize display
    if (!gfx->begin()) {
        Serial.println(F("FATAL: Display init failed"));
        while (true) delay(100);
    }
    gfx->fillScreen(C_BG);
    gfx->setTextWrap(false);

    // Splash
    drawSplash();
    delay(1800);

    // SD Card
    sdReady = initSD();
    Serial.println(sdReady ? F("[OK] SD card") : F("[--] No SD card"));

    // Load cached stop ID if not configured
    if (strlen(activeStopId) == 0 && sdReady) {
        File f = SD.open("/config/stop_id.txt");
        if (f) {
            int len = f.readBytesUntil('\n', activeStopId, sizeof(activeStopId) - 1);
            activeStopId[len] = '\0';
            f.close();
            Serial.printf("[SD] Loaded stop ID: %s\n", activeStopId);
        }
    }

    // WiFi
    currentPage = PAGE_CONNECTING;
    connectWiFi();

    // NTP time
    if (wifiConnected) {
        syncTime();
    }

    // Load cached data as fallback
    if (sdReady) {
        loadFromSD();
    }

    // First data fetch
    if (wifiConnected) {
        fetchBusData();
    }

    // Go to home
    currentPage = PAGE_HOME;
    needsRedraw = true;
    Serial.println(F("[OK] Boot complete\n"));
}

/* ================================================================
 *                     M A I N   L O O P
 * ================================================================ */

void loop() {
    unsigned long now = millis();

    // ── WiFi watchdog ──
    if (WiFi.status() != WL_CONNECTED) {
        if (wifiConnected) {
            wifiConnected = false;
            needsStatusBar = true;
        }
        // Attempt reconnect every 30s
        static unsigned long lastReconnect = 0;
        if (now - lastReconnect > 30000) {
            lastReconnect = now;
            WiFi.reconnect();
        }
    } else if (!wifiConnected) {
        wifiConnected = true;
        needsStatusBar = true;
        syncTime();
    }

    // ── API polling ──
    if (wifiConnected && (now - lastPollTime >= POLL_INTERVAL)) {
        lastPollTime = now;
        fetchBusData();
    }

    // ── Button read ──
    readButtons();

    // ── Touch read (time-division mux) ──
    int16_t tx = -1, ty = -1;
    bool touched = false;
    if (now - lastTouchPoll >= TOUCH_POLL_MS) {
        lastTouchPoll = now;
        touched = readTouch(tx, ty);
    }
    if (touched) {
        handleTouch(tx, ty);
    }

    // ── Animation tick (every 150ms) ──
    if (now - lastAnimTime >= 150) {
        lastAnimTime = now;
        animFrame++;
        // Redraw status bar live indicator
        if (currentPage >= PAGE_HOME) {
            needsStatusBar = true;
        }
    }

    // ── Clock update (every second) ──
    if (now - lastClockUpdate >= 1000) {
        lastClockUpdate = now;
        if (currentPage >= PAGE_HOME) {
            needsStatusBar = true;
        }
    }

    // ── Redraw ──
    if (needsRedraw) {
        needsRedraw = false;
        needsStatusBar = false;
        switch (currentPage) {
            case PAGE_HOME:     drawHomePage();     break;
            case PAGE_BUSLIST:  drawBusListPage();  break;
            case PAGE_DETAIL:   drawDetailPage();   break;
            case PAGE_MAP:      drawMapPage();      break;
            case PAGE_SETTINGS: drawSettingsPage(); break;
            default: break;
        }
    } else if (needsStatusBar && currentPage >= PAGE_HOME) {
        needsStatusBar = false;
        drawStatusBar();
    }
}

/* ================================================================
 *  PIN MULTIPLEXING & TOUCH INPUT
 * ================================================================ */

void restoreDisplayPins() {
    pinMode(XP, OUTPUT);
    pinMode(YP, OUTPUT);
    pinMode(XM, OUTPUT);
    pinMode(YM, OUTPUT);
}

bool readTouch(int16_t &tx, int16_t &ty) {
    TSPoint p = ts.getPoint();
    restoreDisplayPins();

    if (p.z > 10) {
        Serial.printf("[TOUCH DEBUG] Raw X=%d, Y=%d, Pressure=%d\n", p.x, p.y, p.z);
    }

    if (p.z < TS_MIN_PRESSURE || p.z > TS_MAX_PRESSURE) {
        touchActive = false;
        return false;
    }
    if (touchActive) return false;
    unsigned long now = millis();
    if (now - lastTouchTime < TOUCH_DEBOUNCE) return false;

    tx = map(p.x, TS_MINX, TS_MAXX, 0, SCR_W);
    ty = map(p.y, TS_MINY, TS_MAXY, 0, SCR_H);
    tx = constrain(tx, 0, SCR_W - 1);
    ty = constrain(ty, 0, SCR_H - 1);

    lastTouchTime = now;
    touchActive   = true;
    return true;
}

int8_t hitTest(int16_t tx, int16_t ty) {
    for (uint8_t i = 0; i < regionCount; i++) {
        const FocusRegion &r = regions[i];
        if (tx >= r.x && tx < r.x + r.w && ty >= r.y && ty < r.y + r.h)
            return i;
    }
    return -1;
}

void handleTouch(int16_t tx, int16_t ty) {
    int8_t idx = hitTest(tx, ty);
    if (idx < 0) return;
    
    // Update visual focus to tapped item
    focusedItem = idx;
    needsRedraw = true;
    
    executeAction(regions[idx].action, regions[idx].data);
}

/* ================================================================
 *  BUTTON INPUT & ACTIONS
 * ================================================================ */

void readButtons() {
    unsigned long now = millis();
    if (now - lastButtonPress < BTN_DEBOUNCE_MS) return;

    bool up = (digitalRead(BTN_UP) == LOW);
    bool down = (digitalRead(BTN_DOWN) == LOW);
    bool sel = (digitalRead(BTN_SELECT) == LOW);
    bool back = (digitalRead(BTN_BACK) == LOW);

    if (up && !btnUpPressed) {
        btnUpPressed = true;
        lastButtonPress = now;
        if (focusedItem > 0) {
            focusedItem--;
            needsRedraw = true;
        }
    } else if (!up) {
        btnUpPressed = false;
    }

    if (down && !btnDownPressed) {
        btnDownPressed = true;
        lastButtonPress = now;
        if (focusedItem < maxFocusItems - 1) {
            focusedItem++;
            needsRedraw = true;
        }
    } else if (!down) {
        btnDownPressed = false;
    }

    if (sel && !btnSelectPressed) {
        btnSelectPressed = true;
        lastButtonPress = now;
        if (regionCount > 0 && focusedItem < regionCount) {
            executeAction(regions[focusedItem].action, regions[focusedItem].data);
        }
    } else if (!sel) {
        btnSelectPressed = false;
    }

    if (back && !btnBackPressed) {
        btnBackPressed = true;
        lastButtonPress = now;
        if (currentPage == PAGE_DETAIL || currentPage == PAGE_MAP) {
            navigateTo(PAGE_BUSLIST);
        } else if (currentPage == PAGE_BUSLIST || currentPage == PAGE_SETTINGS) {
            navigateTo(PAGE_HOME);
        }
    } else if (!back) {
        btnBackPressed = false;
    }
}

void executeAction(NavAction action, int8_t data) {
    switch (action) {
        case ACT_NAV_HOME:     navigateTo(PAGE_HOME); break;
        case ACT_NAV_BUSES:    navigateTo(PAGE_BUSLIST); break;
        case ACT_NAV_MAP:      selectedBus = -1; navigateTo(PAGE_MAP); break;
        case ACT_NAV_SETTINGS: navigateTo(PAGE_SETTINGS); break;
        case ACT_HOME_BUSES:
        case ACT_HOME_NEXT:
            navigateTo(PAGE_BUSLIST); break;
        case ACT_SCROLL_UP:
            if (scrollOffset > 0) { scrollOffset--; needsRedraw = true; }
            break;
        case ACT_SCROLL_DOWN:
            if (scrollOffset < busCount - VISIBLE_CARDS && busCount > VISIBLE_CARDS) {
                scrollOffset++; needsRedraw = true;
            }
            break;
        case ACT_BUS_SELECT:
            selectedBus = data;
            if (selectedBus < busCount) navigateTo(PAGE_DETAIL);
            break;
        case ACT_DETAIL_MAP:   navigateTo(PAGE_MAP); break;
        case ACT_DETAIL_BACK:  navigateTo(PAGE_BUSLIST); break;
        case ACT_MAP_BACK:
            if (selectedBus >= 0) navigateTo(PAGE_DETAIL);
            else navigateTo(PAGE_HOME);
            break;
        case ACT_SET_WIFI:
            connectWiFi(); needsRedraw = true; break;
        case ACT_SET_REFRESH:
            fetchBusData(); needsRedraw = true; break;
        default: break;
    }
}

void navigateTo(Page page) {
    previousPage = currentPage;
    currentPage  = page;
    scrollOffset = (page == PAGE_BUSLIST) ? 0 : scrollOffset;
    needsRedraw  = true;
}

/* ================================================================
 *  FOCUS REGION HELPERS
 * ================================================================ */

void clearFocus() { regionCount = 0; }

void addFocus(int16_t x, int16_t y, int16_t w, int16_t h, NavAction action, int8_t data) {
    if (regionCount >= MAX_REGIONS) return;
    regions[regionCount++] = { x, y, w, h, action, data };
}

void drawHighlights() {
    maxFocusItems = regionCount;
    if (focusedItem >= maxFocusItems && maxFocusItems > 0) {
        focusedItem = maxFocusItems - 1;
    }
    
    // Draw thick border for focused item
    if (maxFocusItems > 0) {
        FocusRegion &r = regions[focusedItem];
        gfx->drawRoundRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, 4, C_ACCENT);
        gfx->drawRoundRect(r.x - 3, r.y - 3, r.w + 6, r.h + 6, 5, C_ACCENT);
    }
}

/* ================================================================
 *  WiFi
 * ================================================================ */

void connectWiFi() {
    Serial.printf("Connecting to %s ...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(400);
        if (currentPage == PAGE_CONNECTING) {
            drawConnecting(attempts, 20);
        }
        attempts++;
    }

    wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (wifiConnected) {
        Serial.print(F("WiFi OK — IP: "));
        Serial.println(WiFi.localIP());
    } else {
        Serial.println(F("WiFi FAILED — offline mode"));
    }
}

void syncTime() {
    configTime(GMT_OFFSET, 0, "pool.ntp.org", "time.nist.gov");
    struct tm t;
    if (getLocalTime(&t, 3000)) {
        Serial.printf("[NTP] %02d:%02d:%02d\n", t.tm_hour, t.tm_min, t.tm_sec);
    }
}

/* ================================================================
 *  API DATA FETCH
 * ================================================================ */

void fetchBusData() {
    if (WiFi.status() != WL_CONNECTED) return;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url;

    if (strlen(activeStopId) > 0) {
        url = String(API_HOST) + "/api/esp32/display/" + String(activeStopId);
    } else {
        // Demo mode: fetch all buses
        url = String(API_HOST) + "/api/buses";
    }

    Serial.printf("[API] GET %s\n", url.c_str());
    http.begin(client, url);
    http.setTimeout(5000);
    int code = http.GET();

    if (code == 200) {
        String payload = http.getString();
        fetchFailCount = 0;

        // Cache to SD
        if (sdReady) cacheToSD(payload);

        // Parse based on endpoint used
        if (strlen(activeStopId) > 0) {
            parseDisplayResponse(payload);
        } else {
            parseAllBusesResponse(payload);
        }

        lastDataUpdate = millis();
        dataAvailable  = true;

        // Redraw if on a data page
        if (currentPage == PAGE_HOME || currentPage == PAGE_BUSLIST)
            needsRedraw = true;

    } else {
        fetchFailCount++;
        Serial.printf("[API] Failed: %d (fail #%d)\n", code, fetchFailCount);
    }

    http.end();
}


void parseDisplayResponse(const String &json) {
#if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
#else
    DynamicJsonDocument doc(6144);
#endif
    if (deserializeJson(doc, json)) return;

    // Stop info
    JsonObject stop = doc["stop"];
    strncpy(stopName, stop["name"] | "Unknown Stop", sizeof(stopName));
    strncpy(stopDistrict, stop["district"] | "", sizeof(stopDistrict));
    strncpy(stopCode, stop["code"] | "", sizeof(stopCode));

    // Buses
    JsonArray arr = doc["buses"];
    busCount = 0;
    for (JsonObject obj : arr) {
        if (busCount >= MAX_BUSES) break;
        BusData &b = busList[busCount];
        strncpy(b.busId, obj["busId"] | "", sizeof(b.busId));
        strncpy(b.name, obj["name"] | "", sizeof(b.name));
        strncpy(b.route, obj["route"] | "", sizeof(b.route));
        strncpy(b.routeNum, obj["routeNum"] | "", sizeof(b.routeNum));
        strncpy(b.type, obj["type"] | "ordinary", sizeof(b.type));
        b.eta = obj["eta"] | 0;
        b.distance = obj["distance"] | 0.0f;
        b.speed = obj["speed"] | 0;
        strncpy(b.status, obj["status"] | "on_time", sizeof(b.status));
        strncpy(b.confidence, obj["confidence"] | "low", sizeof(b.confidence));
        busCount++;
    }

    // Alerts
    JsonArray al = doc["alerts"];
    alertCount = 0;
    for (JsonObject obj : al) {
        if (alertCount >= MAX_ALERTS) break;
        AlertData &a = alertList[alertCount];
        strncpy(a.type, obj["type"] | "info", sizeof(a.type));
        strncpy(a.title, obj["title"] | "", sizeof(a.title));
        strncpy(a.message, obj["message"] | "", sizeof(a.message));
        strncpy(a.severity, obj["severity"] | "info", sizeof(a.severity));
        alertCount++;
    }
}

void parseAllBusesResponse(const String &json) {
#if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
#else
    DynamicJsonDocument doc(6144);
#endif
    if (deserializeJson(doc, json)) return;

    JsonArray arr = doc.as<JsonArray>();
    busCount = 0;
    strncpy(stopName, "All Buses (Demo)", sizeof(stopName));
    strncpy(stopDistrict, "Configure Stop ID", sizeof(stopDistrict));

    for (JsonObject obj : arr) {
        if (busCount >= MAX_BUSES) break;
        BusData &b = busList[busCount];
        strncpy(b.busId, obj["busId"] | "", sizeof(b.busId));
        strncpy(b.name, obj["name"] | "", sizeof(b.name));
        strncpy(b.route, obj["routeName"] | "", sizeof(b.route));
        b.routeNum[0] = '\0';
        strncpy(b.type, obj["type"] | "ordinary", sizeof(b.type));
        b.eta = 0;
        b.distance = 0;
        b.speed = obj["currentSpeed"] | 0;
        bool onTrip = obj["isOnTrip"] | false;
        strncpy(b.status, onTrip ? "on_time" : "inactive", sizeof(b.status));
        strncpy(b.confidence, "low", sizeof(b.confidence));
        busCount++;
    }
    alertCount = 0;
}

/* ================================================================
 *  SD CARD
 * ================================================================ */

bool initSD() {
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, sdSPI)) return false;
    if (SD.cardType() == CARD_NONE) return false;

    // Create directories
    if (!SD.exists("/cache")) SD.mkdir("/cache");
    if (!SD.exists("/config")) SD.mkdir("/config");

    Serial.printf("[SD] %llu MB\n", SD.cardSize() / (1024ULL * 1024));
    return true;
}

void cacheToSD(const String &json) {
    File f = SD.open("/cache/buses.json", FILE_WRITE);
    if (f) { f.print(json); f.close(); }
}

bool loadFromSD() {
    File f = SD.open("/cache/buses.json");
    if (!f) return false;

    String json = f.readString();
    f.close();

    if (json.length() < 10) return false;

    // Determine format by checking first char
    if (json.charAt(0) == '{') {
        parseDisplayResponse(json);
    } else {
        parseAllBusesResponse(json);
    }
    dataAvailable = true;
    Serial.println(F("[SD] Loaded cached bus data"));
    return true;
}

/* ================================================================
 *  TIME HELPERS
 * ================================================================ */

void getTimeStr(char* buf, size_t len) {
    struct tm t;
    if (getLocalTime(&t, 100)) {
        snprintf(buf, len, "%02d:%02d", t.tm_hour, t.tm_min);
    } else {
        strncpy(buf, "--:--", len);
    }
}

void getDateStr(char* buf, size_t len) {
    struct tm t;
    const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                            "Jul","Aug","Sep","Oct","Nov","Dec"};
    const char* days[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    if (getLocalTime(&t, 100)) {
        snprintf(buf, len, "%s, %02d %s", days[t.tm_wday], t.tm_mday, months[t.tm_mon]);
    } else {
        strncpy(buf, "---", len);
    }
}

/* ================================================================
 *
 *  D R A W I N G   F U N C T I O N S
 *
 * ================================================================ */

/* ---- STATUS BAR (top 26px) ---- */
void drawStatusBar() {
    gfx->fillRect(0, 0, SCR_W, STATUS_H, C_HEADER);
    gfx->drawFastHLine(0, STATUS_H - 1, SCR_W, C_DIVIDER);

    // Time (left)
    char timeBuf[8];
    getTimeStr(timeBuf, sizeof(timeBuf));
    gfx->setTextSize(2);
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(8, 5);
    gfx->print(timeBuf);

    // WiFi indicator
    int16_t wx = 86;
    if (wifiConnected) {
        // Signal bars
        for (int i = 0; i < 4; i++) {
            int bh = 4 + i * 3;
            gfx->fillRect(wx + i * 5, STATUS_H - 4 - bh, 3, bh, C_GREEN);
        }
    } else {
        gfx->setTextSize(1);
        gfx->setTextColor(C_RED);
        gfx->setCursor(wx, 9);
        gfx->print(F("NO WiFi"));
    }

    // LIVE pulse (center)
    int16_t lx = SCR_W / 2 - 20;
    if (dataAvailable && wifiConnected) {
        uint16_t pulseCol = (animFrame % 8 < 4) ? C_GREEN : C_GREEN_BR;
        gfx->fillCircle(lx, STATUS_H / 2, 4, pulseCol);
        gfx->setTextSize(1);
        gfx->setTextColor(pulseCol);
        gfx->setCursor(lx + 8, 9);
        gfx->print(F("LIVE"));
    } else if (dataAvailable) {
        gfx->fillCircle(lx, STATUS_H / 2, 4, C_YELLOW);
        gfx->setTextSize(1);
        gfx->setTextColor(C_YELLOW);
        gfx->setCursor(lx + 8, 9);
        gfx->print(F("CACHED"));
    }

    // Date (right)
    char dateBuf[16];
    getDateStr(dateBuf, sizeof(dateBuf));
    gfx->setTextSize(1);
    gfx->setTextColor(C_LIGHT);
    int16_t dw = strlen(dateBuf) * 6;
    gfx->setCursor(SCR_W - dw - 6, 9);
    gfx->print(dateBuf);
}

/* ---- NAVIGATION BAR (bottom 48px) ---- */
void drawNavBar() {
    int16_t ny = NAV_Y;
    gfx->fillRect(0, ny, SCR_W, NAV_H, C_HEADER);
    gfx->drawFastHLine(0, ny, SCR_W, C_DIVIDER);

    // 4 tabs
    struct { const char* icon; const char* label; NavAction action; Page pg; } tabs[] = {
        { "H", "Home",   ACT_NAV_HOME,     PAGE_HOME },
        { "B", "Buses",  ACT_NAV_BUSES,    PAGE_BUSLIST },
        { "M", "Map",    ACT_NAV_MAP,      PAGE_MAP },
        { "S", "Info",   ACT_NAV_SETTINGS, PAGE_SETTINGS },
    };

    int16_t tw = SCR_W / 4;
    for (int i = 0; i < 4; i++) {
        int16_t tx = i * tw;
        bool active = (currentPage == tabs[i].pg) ||
                      (currentPage == PAGE_DETAIL && tabs[i].pg == PAGE_BUSLIST);

        // Active indicator bar
        if (active) {
            gfx->fillRect(tx + 2, ny + 1, tw - 4, 3, C_NAV_ACTIVE);
        }

        // Icon circle
        int16_t cx = tx + tw / 2;
        int16_t cy = ny + 18;
        uint16_t iconCol = active ? C_NAV_ACTIVE : C_DIM;

        // Draw simple geometric icons
        if (i == 0) { // Home — house
            gfx->fillTriangle(cx - 8, cy, cx, cy - 8, cx + 8, cy, iconCol);
            gfx->fillRect(cx - 5, cy, 10, 8, iconCol);
            gfx->fillRect(cx - 2, cy + 2, 4, 6, C_HEADER);
        } else if (i == 1) { // Buses — bus
            gfx->fillRoundRect(cx - 7, cy - 6, 14, 12, 2, iconCol);
            gfx->fillRect(cx - 5, cy - 4, 4, 4, C_HEADER);
            gfx->fillRect(cx + 1, cy - 4, 4, 4, C_HEADER);
            gfx->fillCircle(cx - 4, cy + 7, 2, iconCol);
            gfx->fillCircle(cx + 4, cy + 7, 2, iconCol);
        } else if (i == 2) { // Map — pin
            gfx->fillCircle(cx, cy - 3, 5, iconCol);
            gfx->fillCircle(cx, cy - 3, 2, C_HEADER);
            gfx->fillTriangle(cx - 4, cy + 1, cx + 4, cy + 1, cx, cy + 8, iconCol);
        } else { // Settings — gear
            gfx->fillCircle(cx, cy, 6, iconCol);
            gfx->fillCircle(cx, cy, 3, C_HEADER);
            for (int a = 0; a < 360; a += 60) {
                float rad = a * 3.14159 / 180.0;
                int16_t dx = cos(rad) * 8;
                int16_t dy = sin(rad) * 8;
                gfx->fillCircle(cx + dx, cy + dy, 2, iconCol);
            }
        }

        // Label
        gfx->setTextSize(1);
        gfx->setTextColor(active ? C_WHITE : C_MUTED);
        int16_t lw = strlen(tabs[i].label) * 6;
        gfx->setCursor(cx - lw / 2, ny + 36);
        gfx->print(tabs[i].label);

        // Focus region
        addFocus(tx, ny, tw, NAV_H, tabs[i].action);
    }
}

/* ---- ALERT BANNER ---- */
void drawAlertBanner() {
    if (alertCount == 0) return;
    AlertData &a = alertList[0];

    uint16_t bg = C_YELLOW;
    if (strcmp(a.severity, "critical") == 0) bg = C_RED;
    else if (strcmp(a.severity, "warning") == 0) bg = C_ORANGE;

    int16_t ay = CONTENT_Y;
    gfx->fillRoundRect(8, ay + 2, SCR_W - 16, 22, 4, bg);
    gfx->setTextSize(1);
    gfx->setTextColor(C_BG);
    gfx->setCursor(16, ay + 8);
    char alertTxt[60];
    snprintf(alertTxt, sizeof(alertTxt), "! %s", a.title);
    gfx->print(alertTxt);
}

/* ================================================================
 *  PAGE: SPLASH
 * ================================================================ */

void drawSplash() {
    gfx->fillScreen(C_BG);

    // Top & bottom accent lines
    for (int i = 0; i < 3; i++) {
        gfx->drawFastHLine(0, i, SCR_W, C_ACCENT);
        gfx->drawFastHLine(0, SCR_H - 1 - i, SCR_W, C_ACCENT);
    }

    // Large bus icon (geometric)
    int16_t bx = SCR_W / 2 - 45;
    int16_t by = SCR_H / 2 - 70;
    gfx->fillRoundRect(bx, by, 90, 56, 10, C_ACCENT);
    gfx->fillRect(bx + 6, by + 4, 78, 3, C_LIGHT);
    gfx->fillRect(bx + 10, by + 12, 18, 18, C_BG);
    gfx->fillRect(bx + 32, by + 12, 18, 18, C_BG);
    gfx->fillRect(bx + 54, by + 12, 18, 18, C_BG);
    gfx->fillRect(bx + 76, by + 20, 8, 30, C_DIM);
    gfx->fillCircle(bx + 22, by + 60, 8, C_DIM);
    gfx->fillCircle(bx + 22, by + 60, 4, C_BG);
    gfx->fillCircle(bx + 66, by + 60, 8, C_DIM);
    gfx->fillCircle(bx + 66, by + 60, 4, C_BG);
    gfx->fillCircle(bx + 5, by + 42, 5, C_YELLOW);

    // Title
    gfx->setTextSize(3);
    gfx->setTextColor(C_WHITE);
    centerText("Smart Bus Stop", SCR_H / 2 + 10, 3, C_WHITE);

    // Subtitle
    gfx->setTextSize(1);
    centerText("Real-time Passenger Information System", SCR_H / 2 + 44, 1, C_DIM);

    // Version
    centerText("v2.0  |  Powered by IoT", SCR_H - 30, 1, C_MUTED);
}

/* ================================================================
 *  PAGE: CONNECTING
 * ================================================================ */

void drawConnecting(int attempt, int maxAttempts) {
    gfx->fillScreen(C_BG);

    // WiFi icon (geometric)
    int16_t cx = SCR_W / 2;
    int16_t cy = SCR_H / 2 - 40;
    for (int r = 12; r <= 36; r += 12) {
        uint16_t col = (attempt % 3 >= (r / 12 - 1)) ? C_ACCENT : C_MUTED;
        gfx->drawCircle(cx, cy + 10, r, col);
        gfx->drawCircle(cx, cy + 10, r - 1, col);
    }
    gfx->fillCircle(cx, cy + 10, 5, C_ACCENT);

    // Text
    centerText("Connecting to WiFi...", cy + 60, 2, C_WHITE);

    gfx->setTextSize(1);
    gfx->setTextColor(C_LIGHT);
    char ssidBuf[48];
    snprintf(ssidBuf, sizeof(ssidBuf), "SSID: %s", WIFI_SSID);
    centerText(ssidBuf, cy + 85, 1, C_LIGHT);

    // Progress bar
    float pct = (float)(attempt + 1) / maxAttempts;
    drawProgressBar(SCR_W / 2 - 100, cy + 105, 200, 8, pct, C_ACCENT);

    // Attempt counter
    char attemptBuf[20];
    snprintf(attemptBuf, sizeof(attemptBuf), "Attempt %d / %d", attempt + 1, maxAttempts);
    centerText(attemptBuf, cy + 125, 1, C_DIM);
}

/* ================================================================
 *  PAGE: HOME
 * ================================================================ */

void drawHomePage() {
    clearFocus();
    gfx->fillScreen(C_BG);
    drawStatusBar();

    int16_t y = CONTENT_Y + 4;

    // Alert banner (if any)
    int16_t alertOffset = 0;
    if (alertCount > 0) {
        drawAlertBanner();
        alertOffset = 26;
    }
    y += alertOffset;

    // ── Stop Name (large, centered) ──
    gfx->setTextSize(3);
    gfx->setTextColor(C_WHITE);
    int16_t nameW = strlen(stopName) * 18;
    if (nameW > SCR_W - 20) gfx->setTextSize(2);
    centerText(stopName, y + 6, (nameW > SCR_W - 20) ? 2 : 3, C_WHITE);

    // District
    gfx->setTextSize(1);
    gfx->setTextColor(C_DIM);
    centerText(stopDistrict, y + 36, 1, C_DIM);

    y += 52;

    // ── Summary Cards Row ──
    int16_t cardW = 140;
    int16_t cardH2 = 70;
    int16_t gap = 12;
    int16_t startX = (SCR_W - 3 * cardW - 2 * gap) / 2;

    // Card 1: Upcoming Buses
    drawRoundCard(startX, y, cardW, cardH2, C_CARD, C_CARD_HI);
    drawBusIconGeo(startX + cardW / 2, y + 18, 10, C_ACCENT);
    gfx->setTextSize(3);
    gfx->setTextColor(C_WHITE);
    char countBuf[4];
    snprintf(countBuf, sizeof(countBuf), "%d", busCount);
    int16_t cw = strlen(countBuf) * 18;
    gfx->setCursor(startX + (cardW - cw) / 2, y + 30);
    gfx->print(countBuf);
    gfx->setTextSize(1);
    gfx->setTextColor(C_DIM);
    centerText("Buses", y + cardH2 - 14, 1, C_DIM);
    // Adjust centerText call for this card
    gfx->setCursor(startX + (cardW - 30) / 2, y + cardH2 - 14);
    gfx->print(F("Buses"));
    addFocus(startX, y, cardW, cardH2, ACT_HOME_BUSES);

    // Card 2: Next ETA
    int16_t c2x = startX + cardW + gap;
    drawRoundCard(c2x, y, cardW, cardH2, C_CARD, C_CARD_HI);
    gfx->setTextSize(1);
    gfx->setTextColor(C_ACCENT);
    gfx->setCursor(c2x + cardW / 2 - 6, y + 10);
    gfx->print(F("ETA"));
    if (busCount > 0 && busList[0].eta > 0) {
        char etaBuf[10];
        snprintf(etaBuf, sizeof(etaBuf), "%d", busList[0].eta);
        gfx->setTextSize(3);
        gfx->setTextColor(C_GREEN);
        int16_t ew = strlen(etaBuf) * 18;
        gfx->setCursor(c2x + (cardW - ew) / 2, y + 26);
        gfx->print(etaBuf);
        gfx->setTextSize(1);
        gfx->setTextColor(C_DIM);
        gfx->setCursor(c2x + (cardW - 18) / 2, y + cardH2 - 14);
        gfx->print(F("min"));
    } else {
        gfx->setTextSize(2);
        gfx->setTextColor(C_MUTED);
        gfx->setCursor(c2x + (cardW - 24) / 2, y + 30);
        gfx->print(F("--"));
        gfx->setTextSize(1);
        gfx->setTextColor(C_DIM);
        gfx->setCursor(c2x + (cardW - 18) / 2, y + cardH2 - 14);
        gfx->print(F("min"));
    }

    // Card 3: Alerts
    int16_t c3x = c2x + cardW + gap;
    drawRoundCard(c3x, y, cardW, cardH2, C_CARD, C_CARD_HI);
    gfx->setTextSize(1);
    gfx->setTextColor(alertCount > 0 ? C_YELLOW : C_DIM);
    gfx->setCursor(c3x + cardW / 2 - 18, y + 10);
    gfx->print(F("Alerts"));
    char aBuf[4];
    snprintf(aBuf, sizeof(aBuf), "%d", alertCount);
    gfx->setTextSize(3);
    gfx->setTextColor(alertCount > 0 ? C_YELLOW : C_MUTED);
    gfx->setCursor(c3x + (cardW - strlen(aBuf) * 18) / 2, y + 26);
    gfx->print(aBuf);
    gfx->setTextSize(1);
    gfx->setTextColor(C_DIM);
    gfx->setCursor(c3x + (cardW - 30) / 2, y + cardH2 - 14);
    gfx->print(F("active"));
    addFocus(c3x, y, cardW, cardH2, ACT_NONE);

    y += cardH2 + 12;

    // ── Next Bus Preview Card ──
    if (busCount > 0) {
        BusData &nb = busList[0];
        int16_t pvW = SCR_W - 24;
        int16_t pvH = 54;
        int16_t pvX = 12;

        drawRoundCard(pvX, y, pvW, pvH, C_CARD, C_ACCENT_DIM);

        // Status dot
        drawStatusDot(pvX + 14, y + pvH / 2, nb.status);

        // Bus ID
        gfx->setTextSize(2);
        gfx->setTextColor(C_WHITE);
        gfx->setCursor(pvX + 28, y + 8);
        gfx->print(nb.busId);

        // Route
        gfx->setTextSize(1);
        gfx->setTextColor(C_LIGHT);
        gfx->setCursor(pvX + 28, y + 30);
        gfx->print(nb.route);

        // ETA (right side)
        if (nb.eta > 0) {
            char etaStr[12];
            snprintf(etaStr, sizeof(etaStr), "%d min", nb.eta);
            gfx->setTextSize(2);
            gfx->setTextColor(C_GREEN);
            int16_t ew = strlen(etaStr) * 12;
            gfx->setCursor(pvX + pvW - ew - 12, y + 10);
            gfx->print(etaStr);
        }

        // Type badge
        uint16_t tc = busTypeColor(nb.type);
        gfx->setTextSize(1);
        gfx->setTextColor(tc);
        gfx->setCursor(pvX + pvW - 60, y + 36);
        gfx->print(nb.type);

        addFocus(pvX, y, pvW, pvH, ACT_HOME_NEXT);

        // Tap hint
        gfx->setTextSize(1);
        gfx->setTextColor(C_MUTED);
        centerText("Tap for details  |  Swipe to Buses tab", y + pvH + 6, 1, C_MUTED);
    } else {
        gfx->setTextSize(2);
        gfx->setTextColor(C_MUTED);
        centerText("No buses approaching", y + 20, 2, C_MUTED);
        gfx->setTextSize(1);
        centerText(wifiConnected ? "Waiting for live data..." : "Connect WiFi in Settings", y + 48, 1, C_DIM);
    }

    drawNavBar();
    drawHighlights();
}

/* ================================================================
 *  PAGE: BUS LIST
 * ================================================================ */

void drawBusListPage() {
    clearFocus();
    gfx->fillScreen(C_BG);
    drawStatusBar();

    // Header
    int16_t hy = CONTENT_Y + 2;
    gfx->setTextSize(2);
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(14, hy + 4);
    gfx->print(F("Upcoming Buses"));

    // Bus count badge
    char cntBuf[8];
    snprintf(cntBuf, sizeof(cntBuf), "%d", busCount);
    int16_t bw = strlen(cntBuf) * 12 + 16;
    gfx->fillRoundRect(SCR_W - bw - 12, hy + 2, bw, 22, 11, C_ACCENT);
    gfx->setTextSize(2);
    gfx->setTextColor(C_BG);
    gfx->setCursor(SCR_W - bw - 4, hy + 5);
    gfx->print(cntBuf);

    int16_t listY = hy + 28;
    int16_t listH = NAV_Y - listY - 4;

    // Scroll arrows
    if (busCount > VISIBLE_CARDS) {
        // Up arrow
        if (scrollOffset > 0) {
            gfx->fillTriangle(SCR_W - 20, listY, SCR_W - 28, listY + 10, SCR_W - 12, listY + 10, C_ACCENT);
            addFocus(SCR_W - 34, listY - 4, 28, 18, ACT_SCROLL_UP);
        }
        // Down arrow
        int16_t dY = NAV_Y - 16;
        if (scrollOffset < busCount - VISIBLE_CARDS) {
            gfx->fillTriangle(SCR_W - 28, dY, SCR_W - 12, dY, SCR_W - 20, dY + 10, C_ACCENT);
            addFocus(SCR_W - 34, dY - 4, 28, 18, ACT_SCROLL_DOWN);
        }
    }

    // Bus cards
    if (busCount == 0) {
        gfx->setTextSize(2);
        centerText("No buses found", listY + 40, 2, C_MUTED);
        gfx->setTextSize(1);
        centerText("Waiting for live data...", listY + 68, 1, C_DIM);
    } else {
        for (int i = 0; i < VISIBLE_CARDS && (scrollOffset + i) < busCount; i++) {
            int idx = scrollOffset + i;
            BusData &b = busList[idx];
            int16_t cy = listY + i * CARD_STEP;
            int16_t cw = SCR_W - 24;
            int16_t cx = 12;

            // Card background
            drawRoundCard(cx, cy, cw, CARD_H, C_CARD, C_CARD_HI);

            // Status color bar (left edge)
            uint16_t sc = C_GREEN;
            if (strcmp(b.status, "delayed") == 0) sc = C_YELLOW;
            else if (strcmp(b.status, "inactive") == 0) sc = C_MUTED;
            else if (strcmp(b.status, "early") == 0) sc = C_CYAN;
            gfx->fillRoundRect(cx, cy, 5, CARD_H, 3, sc);

            // Bus ID (bold)
            gfx->setTextSize(2);
            gfx->setTextColor(C_WHITE);
            gfx->setCursor(cx + 14, cy + 6);
            gfx->print(b.busId);

            // Type badge
            uint16_t tc = busTypeColor(b.type);
            int16_t typeX = cx + 14 + strlen(b.busId) * 12 + 8;
            gfx->fillRoundRect(typeX, cy + 6, strlen(b.type) * 6 + 8, 14, 3, tc);
            gfx->setTextSize(1);
            gfx->setTextColor(C_BG);
            gfx->setCursor(typeX + 4, cy + 9);
            gfx->print(b.type);

            // Route
            gfx->setTextSize(1);
            gfx->setTextColor(C_LIGHT);
            gfx->setCursor(cx + 14, cy + 26);
            // Truncate route to fit
            char routeBuf[42];
            strncpy(routeBuf, b.route, 40);
            routeBuf[40] = '\0';
            gfx->print(routeBuf);

            // Bottom row: distance & speed
            gfx->setTextColor(C_DIM);
            gfx->setCursor(cx + 14, cy + 42);
            if (b.distance > 0) {
                char distBuf[20];
                snprintf(distBuf, sizeof(distBuf), "%.1f km", b.distance);
                gfx->print(distBuf);
            }
            if (b.speed > 0) {
                gfx->setCursor(cx + 14 + 70, cy + 42);
                char spdBuf[16];
                snprintf(spdBuf, sizeof(spdBuf), "%d km/h", b.speed);
                gfx->print(spdBuf);
            }

            // Confidence dot
            gfx->setCursor(cx + 14 + 140, cy + 42);
            if (strcmp(b.confidence, "high") == 0) {
                gfx->setTextColor(C_GREEN);
                gfx->print(F("High"));
            } else if (strcmp(b.confidence, "medium") == 0) {
                gfx->setTextColor(C_YELLOW);
                gfx->print(F("Med"));
            } else {
                gfx->setTextColor(C_MUTED);
                gfx->print(F("Low"));
            }

            // ETA (right side, large)
            if (b.eta > 0) {
                char etaBuf[8];
                snprintf(etaBuf, sizeof(etaBuf), "%d", b.eta);
                gfx->setTextSize(3);
                gfx->setTextColor(sc);
                int16_t ew = strlen(etaBuf) * 18;
                gfx->setCursor(cx + cw - ew - 42, cy + 8);
                gfx->print(etaBuf);

                gfx->setTextSize(1);
                gfx->setTextColor(C_DIM);
                gfx->setCursor(cx + cw - 38, cy + 14);
                gfx->print(F("min"));
            } else {
                // Show status text instead of ETA
                gfx->setTextSize(1);
                gfx->setTextColor(sc);
                gfx->setCursor(cx + cw - 60, cy + 14);
                gfx->print(statusLabel(b.status));
            }

            // Status label
            gfx->setTextSize(1);
            gfx->setTextColor(sc);
            gfx->setCursor(cx + cw - 70, cy + CARD_H - 16);
            gfx->print(statusLabel(b.status));

            addFocus(cx, cy, cw, CARD_H, ACT_BUS_SELECT, idx);
        }
    }

    // Scroll position indicator
    if (busCount > VISIBLE_CARDS) {
        int16_t indH = listH - 20;
        int16_t indY = listY + 10;
        float ratio = (float)scrollOffset / (busCount - VISIBLE_CARDS);
        int16_t thumbH = max(10, indH / busCount * VISIBLE_CARDS);
        int16_t thumbY = indY + ratio * (indH - thumbH);
        gfx->fillRect(SCR_W - 6, indY, 3, indH, C_MUTED);
        gfx->fillRoundRect(SCR_W - 7, thumbY, 5, thumbH, 2, C_ACCENT);
    }

    // Auto-scroll adjustment based on focus
    if (busCount > VISIBLE_CARDS) {
        // Find if the currently focused bus index is outside visible range
        if (focusedItem < busCount) { // It's a bus, not a nav bar tab
            if (focusedItem < scrollOffset) {
                scrollOffset = focusedItem;
                needsRedraw = true; // Needs full redraw immediately
            } else if (focusedItem >= scrollOffset + VISIBLE_CARDS) {
                scrollOffset = focusedItem - VISIBLE_CARDS + 1;
                needsRedraw = true; // Needs full redraw immediately
            }
        }
    }

    drawNavBar();
    drawHighlights();
}

/* ================================================================
 *  PAGE: BUS DETAIL
 * ================================================================ */

void drawDetailPage() {
    clearFocus();
    if (selectedBus < 0 || selectedBus >= busCount) {
        navigateTo(PAGE_BUSLIST);
        return;
    }
    BusData &b = busList[selectedBus];

    gfx->fillScreen(C_BG);
    drawStatusBar();

    int16_t y = CONTENT_Y + 2;

    // ── Header bar ──
    gfx->fillRect(0, y, SCR_W, 36, C_CARD);
    gfx->drawFastHLine(0, y + 35, SCR_W, C_DIVIDER);

    // Back button
    drawButton(6, y + 2, 70, 32, "< Back", C_BTN_SEC, C_LIGHT);
    addFocus(6, y + 2, 70, 32, ACT_DETAIL_BACK);

    // Bus ID title
    gfx->setTextSize(2);
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(86, y + 10);
    gfx->print(F("Bus "));
    gfx->print(b.busId);

    // Type badge
    uint16_t tc = busTypeColor(b.type);
    int16_t badgeX = 86 + (4 + strlen(b.busId)) * 12 + 10;
    gfx->fillRoundRect(badgeX, y + 10, strlen(b.type) * 6 + 10, 16, 4, tc);
    gfx->setTextSize(1);
    gfx->setTextColor(C_BG);
    gfx->setCursor(badgeX + 5, y + 13);
    gfx->print(b.type);

    // Map button (right)
    drawButton(SCR_W - 90, y + 2, 82, 32, "Map >", C_BTN_PRI, C_WHITE);
    addFocus(SCR_W - 90, y + 2, 82, 32, ACT_DETAIL_MAP);

    y += 42;

    // ── Info Grid ──
    // Row 1: Route
    gfx->setTextSize(1);
    gfx->setTextColor(C_DIM);
    gfx->setCursor(16, y);
    gfx->print(F("ROUTE"));
    gfx->setTextSize(2);
    gfx->setTextColor(C_LIGHT);
    gfx->setCursor(16, y + 12);
    // Truncate if too long
    char routeTrunc[38];
    strncpy(routeTrunc, b.route, 36);
    routeTrunc[36] = '\0';
    gfx->print(routeTrunc);

    y += 36;

    // Row 2: ETA | Distance | Speed
    int16_t col1 = 16, col2 = 170, col3 = 320;

    // ETA
    gfx->setTextSize(1);
    gfx->setTextColor(C_DIM);
    gfx->setCursor(col1, y);
    gfx->print(F("ETA"));
    gfx->setTextSize(3);
    uint16_t etaCol = C_GREEN;
    if (strcmp(b.status, "delayed") == 0) etaCol = C_YELLOW;
    gfx->setTextColor(etaCol);
    gfx->setCursor(col1, y + 12);
    if (b.eta > 0) {
        char etaBuf[8];
        snprintf(etaBuf, sizeof(etaBuf), "%d", b.eta);
        gfx->print(etaBuf);
        gfx->setTextSize(1);
        gfx->setTextColor(C_DIM);
        gfx->setCursor(col1 + strlen(etaBuf) * 18 + 4, y + 20);
        gfx->print(F("min"));
    } else {
        gfx->setTextSize(2);
        gfx->print(F("--"));
    }

    // Distance
    gfx->setTextSize(1);
    gfx->setTextColor(C_DIM);
    gfx->setCursor(col2, y);
    gfx->print(F("DISTANCE"));
    gfx->setTextSize(2);
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(col2, y + 14);
    if (b.distance > 0) {
        char distBuf[12];
        snprintf(distBuf, sizeof(distBuf), "%.1f km", b.distance);
        gfx->print(distBuf);
    } else {
        gfx->print(F("-- km"));
    }

    // Speed
    gfx->setTextSize(1);
    gfx->setTextColor(C_DIM);
    gfx->setCursor(col3, y);
    gfx->print(F("SPEED"));
    gfx->setTextSize(2);
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(col3, y + 14);
    char spdBuf[12];
    snprintf(spdBuf, sizeof(spdBuf), "%d km/h", b.speed);
    gfx->print(spdBuf);

    y += 44;

    // Row 3: Status | Confidence
    gfx->setTextSize(1);
    gfx->setTextColor(C_DIM);
    gfx->setCursor(col1, y);
    gfx->print(F("STATUS"));
    drawStatusDot(col1 + 2, y + 18, b.status);
    gfx->setTextSize(2);
    gfx->setTextColor(etaCol);
    gfx->setCursor(col1 + 16, y + 12);
    gfx->print(statusLabel(b.status));

    gfx->setTextSize(1);
    gfx->setTextColor(C_DIM);
    gfx->setCursor(col2, y);
    gfx->print(F("CONFIDENCE"));
    gfx->setTextSize(2);
    uint16_t confCol = C_MUTED;
    if (strcmp(b.confidence, "high") == 0) confCol = C_GREEN;
    else if (strcmp(b.confidence, "medium") == 0) confCol = C_YELLOW;
    gfx->setTextColor(confCol);
    gfx->setCursor(col2, y + 12);
    gfx->print(b.confidence);

    // Name
    gfx->setTextSize(1);
    gfx->setTextColor(C_DIM);
    gfx->setCursor(col3, y);
    gfx->print(F("NAME"));
    gfx->setTextSize(1);
    gfx->setTextColor(C_LIGHT);
    gfx->setCursor(col3, y + 14);
    gfx->print(b.name);

    y += 38;

    // ── Route Progress Visualization ──
    gfx->drawFastHLine(8, y, SCR_W - 16, C_DIVIDER);
    y += 6;

    gfx->setTextSize(1);
    gfx->setTextColor(C_DIM);
    gfx->setCursor(16, y);
    gfx->print(F("ROUTE PROGRESS"));
    y += 14;

    int16_t rx0 = 30;
    int16_t rx1 = SCR_W - 30;
    int16_t routeY = y + 14;
    int numStops = 5;
    int16_t spacing = (rx1 - rx0) / (numStops - 1);

    // Calculate bus position (simulate from distance/eta)
    float busProgress = 0.5;
    if (b.distance > 0 && b.eta > 0) {
        // Rough estimate: bus is at some fraction of the route
        busProgress = max(0.1f, min(0.9f, 1.0f - (b.distance / 20.0f)));
    }

    // Route backbone
    drawThickH(rx0, routeY, rx1 - rx0, 4, C_MUTED);

    // Traversed portion (cyan)
    int16_t busX = rx0 + (int16_t)(busProgress * (rx1 - rx0));
    drawThickH(rx0, routeY, busX - rx0, 4, C_ACCENT);

    // Stop circles
    const char* stopLabels[] = { "Start", "Stop 2", "Stop 3", "THIS", "End" };
    for (int i = 0; i < numStops; i++) {
        int16_t sx = rx0 + i * spacing;
        bool isThisStop = (i == 3); // "THIS" stop
        int16_t r = isThisStop ? 8 : 5;
        uint16_t fill = isThisStop ? C_ACCENT : (sx <= busX ? C_WHITE : C_MUTED);

        gfx->fillCircle(sx, routeY, r, fill);
        if (isThisStop) {
            gfx->drawCircle(sx, routeY, r + 2, C_ACCENT);
        }

        gfx->setTextSize(1);
        gfx->setTextColor(isThisStop ? C_ACCENT : C_DIM);
        int16_t lw = strlen(stopLabels[i]) * 6;
        gfx->setCursor(sx - lw / 2, routeY + 14);
        gfx->print(stopLabels[i]);
    }

    // Bus marker (animated pulse)
    uint16_t busCol = (animFrame % 4 < 2) ? C_GREEN_BR : C_GREEN;
    gfx->fillCircle(busX, routeY, 6, busCol);
    gfx->fillCircle(busX, routeY, 3, C_BG);
    gfx->drawCircle(busX, routeY, 9, C_GREEN);

    // Bus label above marker
    gfx->setTextSize(1);
    gfx->setTextColor(C_WHITE);
    int16_t blw = strlen(b.busId) * 6;
    gfx->fillRoundRect(busX - blw / 2 - 4, routeY - 24, blw + 8, 14, 3, C_GREEN);
    gfx->setTextColor(C_BG);
    gfx->setCursor(busX - blw / 2, routeY - 21);
    gfx->print(b.busId);

    drawNavBar();
    drawHighlights();
}

/* ================================================================
 *  PAGE: MAP VIEW
 * ================================================================ */

void drawMapPage() {
    clearFocus();
    gfx->fillScreen(C_BG);
    drawStatusBar();

    int16_t y = CONTENT_Y + 2;

    // Header
    gfx->fillRect(0, y, SCR_W, 32, C_CARD);
    gfx->drawFastHLine(0, y + 31, SCR_W, C_DIVIDER);

    drawButton(6, y + 2, 70, 28, "< Back", C_BTN_SEC, C_LIGHT);
    addFocus(6, y + 2, 70, 28, ACT_MAP_BACK);

    gfx->setTextSize(2);
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(86, y + 8);
    gfx->print(F("Route Map"));

    y += 38;

    // Map area
    int16_t mapX = 10;
    int16_t mapY = y;
    int16_t mapW = SCR_W - 20;
    int16_t mapH = NAV_Y - y - 50;

    drawRoundCard(mapX, mapY, mapW, mapH, 0x0000, C_CARD_HI);

    // Draw a curved route across the map
    int numPts = 7;
    int16_t routePts[][2] = {
        { mapX + 30,       mapY + mapH - 30 },
        { mapX + 80,       mapY + mapH / 2 + 20 },
        { mapX + 140,      mapY + 40 },
        { mapX + mapW / 2, mapY + mapH / 2 - 10 },
        { mapX + mapW - 140, mapY + mapH - 50 },
        { mapX + mapW - 80,  mapY + 50 },
        { mapX + mapW - 30,  mapY + mapH / 2 },
    };

    // Draw route lines
    for (int i = 0; i < numPts - 1; i++) {
        drawThickLine(routePts[i][0], routePts[i][1],
                      routePts[i + 1][0], routePts[i + 1][1], 3, C_MUTED);
    }

    // Draw stop markers
    const char* mapStopNames[] = { "Origin", "S2", "S3", stopName, "S5", "S6", "Dest" };
    int thisStopIdx = 3;

    for (int i = 0; i < numPts; i++) {
        bool isThis = (i == thisStopIdx);
        int16_t r = isThis ? 10 : 5;
        uint16_t fill = isThis ? C_ACCENT : C_WHITE;

        gfx->fillCircle(routePts[i][0], routePts[i][1], r, fill);
        if (isThis) {
            gfx->drawCircle(routePts[i][0], routePts[i][1], r + 3, C_ACCENT);
            gfx->drawCircle(routePts[i][0], routePts[i][1], r + 5, C_ACCENT_DIM);
        }

        // Label
        gfx->setTextSize(1);
        gfx->setTextColor(isThis ? C_ACCENT : C_DIM);
        int16_t lw = strlen(mapStopNames[i]) * 6;
        int16_t labelY = (i % 2 == 0) ? routePts[i][1] - 16 : routePts[i][1] + 10;
        gfx->setCursor(routePts[i][0] - lw / 2, labelY);
        gfx->print(mapStopNames[i]);
    }

    // Draw bus position if selected
    if (selectedBus >= 0 && selectedBus < busCount) {
        BusData &b = busList[selectedBus];
        float prog = 0.5;
        if (b.distance > 0) prog = max(0.1f, min(0.9f, 1.0f - (b.distance / 20.0f)));

        // Interpolate position along route
        float totalIdx = prog * (numPts - 1);
        int segIdx = (int)totalIdx;
        segIdx = constrain(segIdx, 0, numPts - 2);
        float segFrac = totalIdx - segIdx;

        int16_t bx = routePts[segIdx][0] + segFrac * (routePts[segIdx + 1][0] - routePts[segIdx][0]);
        int16_t by = routePts[segIdx][1] + segFrac * (routePts[segIdx + 1][1] - routePts[segIdx][1]);

        // Highlight route up to bus
        for (int i = 0; i < segIdx; i++) {
            drawThickLine(routePts[i][0], routePts[i][1],
                          routePts[i + 1][0], routePts[i + 1][1], 3, C_GREEN);
        }
        if (segIdx < numPts - 1) {
            drawThickLine(routePts[segIdx][0], routePts[segIdx][1], bx, by, 3, C_GREEN);
        }

        // Bus marker (animated)
        uint16_t busPulse = (animFrame % 6 < 3) ? C_GREEN_BR : C_GREEN;
        gfx->fillCircle(bx, by, 8, busPulse);
        drawBusIconGeo(bx, by, 6, C_BG);
        gfx->drawCircle(bx, by, 11, C_GREEN);

        // Bus label
        gfx->fillRoundRect(bx - 20, by - 22, 40, 12, 3, C_GREEN);
        gfx->setTextSize(1);
        gfx->setTextColor(C_BG);
        int16_t idw = strlen(b.busId) * 6;
        gfx->setCursor(bx - idw / 2, by - 19);
        gfx->print(b.busId);
    }

    // Info bar at bottom of map
    int16_t infoY = mapY + mapH + 4;
    gfx->setTextSize(1);
    gfx->setTextColor(C_DIM);
    gfx->setCursor(mapX + 4, infoY);
    if (selectedBus >= 0 && selectedBus < busCount) {
        char infoBuf[60];
        snprintf(infoBuf, sizeof(infoBuf), "Tracking: %s  |  ETA: %d min  |  %.1f km away",
                 busList[selectedBus].busId, busList[selectedBus].eta, busList[selectedBus].distance);
        gfx->print(infoBuf);
    } else {
        gfx->print(F("Select a bus from the Buses tab to track on map"));
    }

    drawNavBar();
    drawHighlights();
}

/* ================================================================
 *  PAGE: SETTINGS
 * ================================================================ */

void drawSettingsPage() {
    clearFocus();
    gfx->fillScreen(C_BG);
    drawStatusBar();

    int16_t y = CONTENT_Y + 4;

    // Header
    gfx->setTextSize(2);
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(14, y);
    gfx->print(F("System Information"));
    y += 28;

    gfx->drawFastHLine(8, y, SCR_W - 16, C_DIVIDER);
    y += 8;

    int16_t labelX = 20;
    int16_t valX   = 160;

    // WiFi Status
    gfx->setTextSize(1);
    gfx->setTextColor(C_DIM);
    gfx->setCursor(labelX, y);
    gfx->print(F("WiFi Status"));
    gfx->setTextColor(wifiConnected ? C_GREEN : C_RED);
    gfx->setCursor(valX, y);
    gfx->print(wifiConnected ? F("Connected") : F("Disconnected"));
    y += 16;

    // SSID
    gfx->setTextColor(C_DIM);
    gfx->setCursor(labelX, y);
    gfx->print(F("SSID"));
    gfx->setTextColor(C_LIGHT);
    gfx->setCursor(valX, y);
    gfx->print(WIFI_SSID);
    y += 16;

    // IP Address
    gfx->setTextColor(C_DIM);
    gfx->setCursor(labelX, y);
    gfx->print(F("IP Address"));
    gfx->setTextColor(C_LIGHT);
    gfx->setCursor(valX, y);
    if (wifiConnected) gfx->print(WiFi.localIP());
    else gfx->print(F("N/A"));
    y += 16;

    // Signal
    gfx->setTextColor(C_DIM);
    gfx->setCursor(labelX, y);
    gfx->print(F("Signal Strength"));
    gfx->setTextColor(C_LIGHT);
    gfx->setCursor(valX, y);
    if (wifiConnected) {
        int rssi = WiFi.RSSI();
        const char* sig = rssi > -50 ? "Excellent" : rssi > -65 ? "Good" : rssi > -80 ? "Fair" : "Weak";
        gfx->print(sig);
        char rssiBuf[12];
        snprintf(rssiBuf, sizeof(rssiBuf), " (%ddBm)", rssi);
        gfx->print(rssiBuf);
    } else {
        gfx->print(F("N/A"));
    }
    y += 22;

    gfx->drawFastHLine(8, y, SCR_W - 16, C_DIVIDER);
    y += 8;

    // Stop Info
    gfx->setTextColor(C_DIM);
    gfx->setCursor(labelX, y);
    gfx->print(F("Bus Stop"));
    gfx->setTextColor(C_ACCENT);
    gfx->setCursor(valX, y);
    gfx->print(stopName);
    y += 16;

    gfx->setTextColor(C_DIM);
    gfx->setCursor(labelX, y);
    gfx->print(F("District"));
    gfx->setTextColor(C_LIGHT);
    gfx->setCursor(valX, y);
    gfx->print(strlen(stopDistrict) > 0 ? stopDistrict : "N/A");
    y += 16;

    gfx->setTextColor(C_DIM);
    gfx->setCursor(labelX, y);
    gfx->print(F("Stop ID"));
    gfx->setTextColor(C_MUTED);
    gfx->setCursor(valX, y);
    gfx->print(strlen(activeStopId) > 0 ? activeStopId : "(demo mode)");
    y += 22;

    gfx->drawFastHLine(8, y, SCR_W - 16, C_DIVIDER);
    y += 8;

    // API / Data
    gfx->setTextColor(C_DIM);
    gfx->setCursor(labelX, y);
    gfx->print(F("API Server"));
    gfx->setTextColor(C_LIGHT);
    gfx->setCursor(valX, y);
    gfx->print(API_HOST);
    y += 16;

    gfx->setTextColor(C_DIM);
    gfx->setCursor(labelX, y);
    gfx->print(F("Active Buses"));
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(valX, y);
    char busCnt[8];
    snprintf(busCnt, sizeof(busCnt), "%d", busCount);
    gfx->print(busCnt);
    y += 16;

    gfx->setTextColor(C_DIM);
    gfx->setCursor(labelX, y);
    gfx->print(F("Fetch Errors"));
    gfx->setTextColor(fetchFailCount > 0 ? C_YELLOW : C_GREEN);
    gfx->setCursor(valX, y);
    char failBuf[8];
    snprintf(failBuf, sizeof(failBuf), "%d", fetchFailCount);
    gfx->print(failBuf);
    y += 16;

    gfx->setTextColor(C_DIM);
    gfx->setCursor(labelX, y);
    gfx->print(F("SD Card"));
    gfx->setTextColor(sdReady ? C_GREEN : C_RED);
    gfx->setCursor(valX, y);
    gfx->print(sdReady ? F("Mounted") : F("Not Found"));
    y += 16;

    gfx->setTextColor(C_DIM);
    gfx->setCursor(labelX, y);
    gfx->print(F("Free Heap"));
    gfx->setTextColor(C_LIGHT);
    gfx->setCursor(valX, y);
    char heapBuf[16];
    snprintf(heapBuf, sizeof(heapBuf), "%d KB", ESP.getFreeHeap() / 1024);
    gfx->print(heapBuf);
    y += 22;

    // Action buttons
    drawButton(16, y, 140, BTN_H, "Restart WiFi", C_BTN_SEC, C_LIGHT);
    addFocus(16, y, 140, BTN_H, ACT_SET_WIFI);

    drawButton(170, y, 140, BTN_H, "Refresh Data", C_BTN_PRI, C_WHITE);
    addFocus(170, y, 140, BTN_H, ACT_SET_REFRESH);

    drawNavBar();
    drawHighlights();
}

/* ================================================================
 *  UI PRIMITIVES
 * ================================================================ */

void drawRoundCard(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t bg, uint16_t border) {
    gfx->fillRoundRect(x, y, w, h, 6, bg);
    gfx->drawRoundRect(x, y, w, h, 6, border);
}

void drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const char* label, uint16_t bg, uint16_t fg) {
    gfx->fillRoundRect(x, y, w, h, BTN_R, bg);
    gfx->drawRoundRect(x, y, w, h, BTN_R, fg);

    // Subtle top highlight
    gfx->drawFastHLine(x + BTN_R, y + 1, w - 2 * BTN_R, bg + 0x1082);

    gfx->setTextSize(1);
    gfx->setTextColor(fg);
    int16_t tw = strlen(label) * 6;
    gfx->setCursor(x + (w - tw) / 2, y + (h - 8) / 2);
    gfx->print(label);
}

void drawBusIconGeo(int16_t cx, int16_t cy, int16_t sz, uint16_t col) {
    // Simple bus shape centered at (cx, cy)
    gfx->fillRoundRect(cx - sz, cy - sz / 2, sz * 2, sz, 2, col);
    gfx->fillRect(cx - sz + 2, cy - sz / 2 + 2, sz / 2, sz / 2, C_BG);
    gfx->fillRect(cx + 1, cy - sz / 2 + 2, sz / 2, sz / 2, C_BG);
}

void drawStatusDot(int16_t x, int16_t y, const char* status) {
    uint16_t col = C_GREEN;
    if (strcmp(status, "delayed") == 0) col = C_YELLOW;
    else if (strcmp(status, "early") == 0) col = C_CYAN;
    else if (strcmp(status, "inactive") == 0) col = C_MUTED;
    gfx->fillCircle(x, y, 4, col);
}

void drawThickH(int16_t x, int16_t y, int16_t w, uint8_t t, uint16_t c) {
    for (int i = 0; i < t; i++) {
        gfx->drawFastHLine(x, y - t / 2 + i, w, c);
    }
}

void drawThickLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t t, uint16_t c) {
    int16_t half = t / 2;
    if (abs(x1 - x0) >= abs(y1 - y0)) {
        for (int16_t off = -half; off <= half; off++)
            gfx->drawLine(x0, y0 + off, x1, y1 + off, c);
    } else {
        for (int16_t off = -half; off <= half; off++)
            gfx->drawLine(x0 + off, y0, x1 + off, y1, c);
    }
}

void drawSpinner(int16_t cx, int16_t cy, uint8_t frame) {
    for (int i = 0; i < 8; i++) {
        float angle = i * 3.14159 / 4.0;
        int16_t dx = cos(angle) * 12;
        int16_t dy = sin(angle) * 12;
        uint16_t col = (i == (frame % 8)) ? C_ACCENT : C_MUTED;
        gfx->fillCircle(cx + dx, cy + dy, 3, col);
    }
}

void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, float pct, uint16_t fg) {
    gfx->fillRoundRect(x, y, w, h, h / 2, C_MUTED);
    int16_t filled = (int16_t)(pct * w);
    if (filled > 0) {
        gfx->fillRoundRect(x, y, filled, h, h / 2, fg);
    }
}

void centerText(const char* txt, int16_t y, uint8_t sz, uint16_t col) {
    gfx->setTextSize(sz);
    gfx->setTextColor(col);
    int16_t tw = strlen(txt) * 6 * sz;
    gfx->setCursor((SCR_W - tw) / 2, y);
    gfx->print(txt);
}

uint16_t busTypeColor(const char* type) {
    if (strcmp(type, "express") == 0) return C_BUS_EXP;
    if (strcmp(type, "deluxe") == 0)  return C_BUS_DLX;
    if (strcmp(type, "ac") == 0)      return C_BUS_AC;
    return C_BUS_ORD;
}

const char* statusLabel(const char* status) {
    if (strcmp(status, "on_time") == 0)  return "On Time";
    if (strcmp(status, "delayed") == 0)  return "Delayed";
    if (strcmp(status, "early") == 0)    return "Early";
    if (strcmp(status, "inactive") == 0) return "Off Trip";
    return "Active";
}

/* ================================================================
 *  END OF FILE
 * ================================================================ */
