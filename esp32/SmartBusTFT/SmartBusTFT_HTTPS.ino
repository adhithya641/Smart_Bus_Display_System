/*
 * ================================================================
 *  SMART BUS TRACKING UI (WIFI & API INTEGRATED)
 *  ESP32 + ILI9488 3.5" TFT (8-Bit Parallel) + Resistive Touch
 * ================================================================
 *
 *  Features:
 *    - Connects to Wi-Fi.
 *    - Periodically calls the local backend API (http://10.45.21.213:5000/api/buses).
 *    - Parses live buses dynamically using ArduinoJson.
 *    - Displays live buses on a beautiful touchscreen map.
 *    - Incorporates pin multiplexing for shared touch & parallel TFT lines.
 *
 *  Libraries Required:
 *    - Arduino_GFX_Library
 *    - TouchScreen (Adafruit/Seeed)
 *    - ArduinoJson
 *    - WiFi (built-in)
 *    - HTTPClient (built-in)
 */

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <TouchScreen.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ===== Wi-Fi Configuration =====
const char* WIFI_SSID     = "YOUR_WIFI_SSID";     // Change to your Wi-Fi name
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"; // Change to your Wi-Fi password
const char* API_URL       = "http://10.45.21.213:5000/api/buses"; // Updated to current local IP!

// --- TFT Display Control Pins ---
#define TFT_RST   32
#define TFT_CS    33
#define TFT_RS    15      // a.k.a. DC (Data/Command)
#define TFT_WR    4

// --- TFT 8-Bit Parallel Data Pins (D0–D7) ---
#define TFT_D0    21
#define TFT_D1    13
#define TFT_D2    26
#define TFT_D3    25
#define TFT_D4    17
#define TFT_D5    16
#define TFT_D6    27
#define TFT_D7    14

// --- Resistive Touch Pins (SHARED with display lines!) ---
#define XP        21      // Shared with TFT_D0
#define YP        13      // Shared with TFT_D1
#define XM        15      // Shared with TFT_RS (DC)
#define YM        33      // Shared with TFT_CS

// --- SD Card Pins (VSPI bus) ---
#define SD_CS     23
#define SD_MOSI   22
#define SD_MISO   19
#define SD_SCK    18

// Touch Calibration
#define TS_MINX               150
#define TS_MAXX               920
#define TS_MINY               120
#define TS_MAXY               940
#define TS_MIN_PRESSURE       200
#define TS_MAX_PRESSURE       1200
#define TOUCH_DEBOUNCE_MS     300
#define TOUCH_READ_INTERVAL   50

#define SCREEN_WIDTH          480
#define SCREEN_HEIGHT         320

#define HEADER_H              44
#define FOOTER_H              32
#define CONTENT_Y             (HEADER_H + 1)
#define CONTENT_H             (SCREEN_HEIGHT - HEADER_H - FOOTER_H)

#define BUS_ICON_W            62
#define BUS_ICON_H            46
#define BUS_ICON_R            8

#define BTN_H                 42
#define BTN_R                 6

#define MODAL_W               360
#define MODAL_H               210
#define MODAL_X               ((SCREEN_WIDTH  - MODAL_W) / 2)
#define MODAL_Y               ((SCREEN_HEIGHT - MODAL_H) / 2)
#define MODAL_R               10

// Colors (RGB-565)
#define CLR_BG                0x0841    // Very dark navy
#define CLR_HEADER_BG         0x1126    // Deep blue
#define CLR_HEADER_ACCENT     0x2B5F    // Bright blue
#define CLR_FOOTER_BG         0x0841
#define CLR_WHITE             0xFFFF
#define CLR_LIGHT             0xC638    // Light grey
#define CLR_DIM               0x7BEF    // Medium grey
#define CLR_YELLOW            0xFFE0
#define CLR_ROUTE             0x4A69
#define CLR_ROUTE_HI          0x07FF    // Cyan
#define CLR_STOP_FILL         0xFFFF
#define CLR_STOP_RING         0x4A69
#define CLR_STOP_DOT          0x2B5F
#define CLR_BUS_1             0x2E6A    // Green
#define CLR_BUS_2             0xFC80    // Orange
#define CLR_BUS_3             0x34BF    // Blue
#define CLR_BUS_4             0xE8C4    // Coral red
#define CLR_MODAL_BG          0x18C3
#define CLR_MODAL_BORDER      0x3186
#define CLR_BTN_PRIMARY       0x2E6A
#define CLR_BTN_SECONDARY     0x4208
#define CLR_GREEN_BRIGHT      0x07E0

enum ScreenState {
    SCREEN_SPLASH,
    SCREEN_MAIN,
    SCREEN_BUS_DETAIL,
    SCREEN_ROUTE_DETAIL
};

struct BusStop {
    char        name[32];
    int16_t     x;
    int16_t     y;
};

struct BusInfo {
    uint8_t     id;
    char        busNumber[16];
    char        destination[48];
    char        arrivalTime[16];
    uint8_t     stopIndex;
    uint16_t    color;
    int16_t     iconX;
    int16_t     iconY;
};

struct TouchRegion {
    int16_t  x, y, w, h;
    uint8_t  id;
};

#define NUM_STOPS   5
BusStop stops[NUM_STOPS] = {
    { "Central Stn",  55, 175 },
    { "Park Ave",    160, 132 },
    { "Market St",   265, 175 },
    { "Tech Hub",    370, 132 },
    { "Airport",     445, 175 }
};

#define MAX_BUSES   5
BusInfo buses[MAX_BUSES];
uint8_t activeBusCount = 0;

// Display Bus
Arduino_DataBus *dataBus = new Arduino_ESP32PAR8Q(
    TFT_RS, TFT_CS, TFT_WR, GFX_NOT_DEFINED,
    TFT_D0, TFT_D1, TFT_D2, TFT_D3,
    TFT_D4, TFT_D5, TFT_D6, TFT_D7
);

Arduino_GFX *gfx = new Arduino_ILI9488(dataBus, TFT_RST, 1, false);
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

ScreenState currentScreen    = SCREEN_SPLASH;
int8_t      selectedBusIdx   = -1;
bool        needsRedraw      = true;
unsigned long lastTouchTime      = 0;
unsigned long lastTouchPollTime  = 0;
bool          touchWasActive     = false;

#define MAX_REGIONS  12
TouchRegion  regions[MAX_REGIONS];
uint8_t      regionCount = 0;

unsigned long lastApiPollTime = 0;
const unsigned long API_POLL_INTERVAL = 8000; // Poll API every 8 seconds

void restoreDisplayPins();
bool readTouch(int16_t &tx, int16_t &ty);
void handleTouch(int16_t tx, int16_t ty);
void clearRegions();
void addRegion(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t id);
int8_t hitTest(int16_t tx, int16_t ty);
void drawSplashScreen();
void drawMainScreen();
void drawBusDetailModal(int idx);
void drawRouteDetailScreen(int idx);
void drawHeader(const char* title);
void drawFooter(const char* msg);
void drawRouteMap();
void drawBusIcon(int idx);
void drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const char* label, uint16_t bg, uint16_t fg);
void drawThickLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t t, uint16_t c);
void connectWiFi();
void fetchLiveBuses();

void setup() {
    Serial.begin(115200);
    Serial.println(F("\n========================================"));
    Serial.println(F("  Smart Bus Tracker (API Mode) — Booting"));
    Serial.println(F("========================================"));

    if (!gfx->begin()) {
        Serial.println(F("FATAL: gfx->begin() failed. Halting."));
        while (true) delay(100);
    }
    gfx->fillScreen(CLR_BG);
    gfx->setTextWrap(false);

    drawSplashScreen();
    
    // Connect to local WiFi network
    connectWiFi();

    // Initial fetch of live bus data
    fetchLiveBuses();

    delay(1000);
    currentScreen = SCREEN_MAIN;
    needsRedraw   = true;
}

void loop() {
    unsigned long now = millis();
    int16_t tx = -1, ty = -1;
    bool    touched = false;

    // ---- WiFi Maintain & API Poll ----
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
    }

    if (now - lastApiPollTime >= API_POLL_INTERVAL) {
        lastApiPollTime = now;
        fetchLiveBuses();
    }

    // ---- Resistive Touch (time-division multiplex) ----
    if (now - lastTouchPollTime >= TOUCH_READ_INTERVAL) {
        lastTouchPollTime = now;
        touched = readTouch(tx, ty);
    }

    if (touched) {
        handleTouch(tx, ty);
    }

    // ---- Redraw screen when dirty ----
    if (needsRedraw) {
        needsRedraw = false;
        switch (currentScreen) {
            case SCREEN_MAIN:
                drawMainScreen();
                break;
            case SCREEN_BUS_DETAIL:
                if (selectedBusIdx >= 0)
                    drawBusDetailModal(selectedBusIdx);
                break;
            case SCREEN_ROUTE_DETAIL:
                if (selectedBusIdx >= 0)
                    drawRouteDetailScreen(selectedBusIdx);
                break;
            default:
                break;
        }
    }
}

void restoreDisplayPins() {
    pinMode(XP, OUTPUT);
    pinMode(YP, OUTPUT);
    pinMode(XM, OUTPUT);
    pinMode(YM, OUTPUT);
}

bool readTouch(int16_t &tx, int16_t &ty) {
    TSPoint p = ts.getPoint();
    restoreDisplayPins();

    if (p.z < TS_MIN_PRESSURE || p.z > TS_MAX_PRESSURE) {
        touchWasActive = false;
        return false;
    }

    unsigned long now = millis();
    if (touchWasActive) return false;
    if (now - lastTouchTime < TOUCH_DEBOUNCE_MS) return false;

    tx = map(p.x, TS_MINX, TS_MAXX, 0, SCREEN_WIDTH);
    ty = map(p.y, TS_MINY, TS_MAXY, 0, SCREEN_HEIGHT);

    tx = constrain(tx, 0, SCREEN_WIDTH  - 1);
    ty = constrain(ty, 0, SCREEN_HEIGHT - 1);

    lastTouchTime  = now;
    touchWasActive = true;
    return true;
}

void handleTouch(int16_t tx, int16_t ty) {
    int8_t rid = hitTest(tx, ty);
    if (rid < 0) return;

    switch (currentScreen) {
        case SCREEN_MAIN:
            if (rid < activeBusCount) {
                selectedBusIdx = rid;
                currentScreen  = SCREEN_BUS_DETAIL;
                needsRedraw    = true;
            }
            break;
        case SCREEN_BUS_DETAIL:
            if (rid == 100) {
                currentScreen = SCREEN_ROUTE_DETAIL;
                needsRedraw   = true;
            } else if (rid == 101) {
                selectedBusIdx = -1;
                currentScreen  = SCREEN_MAIN;
                needsRedraw    = true;
            }
            break;
        case SCREEN_ROUTE_DETAIL:
            if (rid == 200) {
                selectedBusIdx = -1;
                currentScreen  = SCREEN_MAIN;
                needsRedraw    = true;
            }
            break;
        default: break;
    }
}

void clearRegions() {
    regionCount = 0;
}

void addRegion(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t id) {
    if (regionCount >= MAX_REGIONS) return;
    regions[regionCount++] = { x, y, w, h, id };
}

int8_t hitTest(int16_t tx, int16_t ty) {
    for (uint8_t i = 0; i < regionCount; i++) {
        const TouchRegion &r = regions[i];
        if (tx >= r.x && tx < r.x + r.w && ty >= r.y && ty < r.y + r.h) {
            return (int8_t)r.id;
        }
    }
    return -1;
}

void drawSplashScreen() {
    gfx->fillScreen(CLR_BG);
    for (int i = 0; i < 4; i++)
        gfx->drawFastHLine(0, i, SCREEN_WIDTH, CLR_HEADER_ACCENT);

    int16_t bx = SCREEN_WIDTH  / 2 - 40;
    int16_t by = SCREEN_HEIGHT / 2 - 60;

    gfx->fillRoundRect(bx, by, 80, 50, 8, CLR_BUS_1);
    gfx->fillRect(bx + 4, by + 4, 72, 2, CLR_LIGHT);
    gfx->fillRect(bx + 8,  by + 10, 16, 16, CLR_BG);
    gfx->fillRect(bx + 28, by + 10, 16, 16, CLR_BG);
    gfx->fillRect(bx + 48, by + 10, 16, 16, CLR_BG);
    gfx->fillRect(bx + 68, by + 18, 8, 28, CLR_DIM);
    gfx->drawRect(bx + 68, by + 18, 8, 28, CLR_LIGHT);
    gfx->fillCircle(bx + 18, by + 53, 7, CLR_DIM);
    gfx->fillCircle(bx + 18, by + 53, 3, CLR_BG);
    gfx->fillCircle(bx + 60, by + 53, 7, CLR_DIM);
    gfx->fillCircle(bx + 60, by + 53, 3, CLR_BG);
    gfx->fillCircle(bx + 3, by + 38, 4, CLR_YELLOW);

    gfx->setTextSize(3);
    gfx->setTextColor(CLR_WHITE);
    gfx->setCursor(SCREEN_WIDTH / 2 - 142, SCREEN_HEIGHT / 2 + 14);
    gfx->print(F("Smart Bus Tracker"));

    gfx->setTextSize(1);
    gfx->setTextColor(CLR_DIM);
    gfx->setCursor(SCREEN_WIDTH / 2 - 60, SCREEN_HEIGHT / 2 + 48);
    gfx->print(F("Connecting to Wi-Fi..."));

    for (int i = 0; i < 4; i++)
        gfx->drawFastHLine(0, SCREEN_HEIGHT - 1 - i, SCREEN_WIDTH, CLR_HEADER_ACCENT);
}

void drawHeader(const char* title) {
    gfx->fillRect(0, 0, SCREEN_WIDTH, HEADER_H, CLR_HEADER_BG);
    gfx->drawFastHLine(0, HEADER_H - 2, SCREEN_WIDTH, CLR_HEADER_ACCENT);
    gfx->drawFastHLine(0, HEADER_H - 1, SCREEN_WIDTH, CLR_HEADER_ACCENT);

    gfx->fillRoundRect(10, 8, 26, 18, 3, CLR_BUS_1);
    gfx->fillRect(13, 11, 7, 7, CLR_HEADER_BG);
    gfx->fillRect(22, 11, 7, 7, CLR_HEADER_BG);
    gfx->fillCircle(16, 28, 3, CLR_DIM);
    gfx->fillCircle(31, 28, 3, CLR_DIM);

    gfx->setTextSize(2);
    gfx->setTextColor(CLR_WHITE);
    gfx->setCursor(44, 14);
    gfx->print(title);

    gfx->fillCircle(SCREEN_WIDTH - 72, 22, 4, CLR_GREEN_BRIGHT);
    gfx->setTextSize(1);
    gfx->setTextColor(CLR_GREEN_BRIGHT);
    gfx->setCursor(SCREEN_WIDTH - 64, 18);
    gfx->print(F("LIVE"));

    gfx->setTextColor(CLR_LIGHT);
    gfx->setCursor(SCREEN_WIDTH - 50, 8);
    gfx->print(F("API"));
}

void drawFooter(const char* msg) {
    int16_t fy = SCREEN_HEIGHT - FOOTER_H;
    gfx->fillRect(0, fy, SCREEN_WIDTH, FOOTER_H, CLR_FOOTER_BG);
    gfx->drawFastHLine(0, fy, SCREEN_WIDTH, CLR_MODAL_BORDER);

    gfx->setTextSize(1);
    gfx->setTextColor(CLR_DIM);
    gfx->setCursor(10, fy + 11);
    gfx->print(msg);
}

void drawMainScreen() {
    clearRegions();
    gfx->fillScreen(CLR_BG);

    drawHeader("Smart Bus Tracker");
    drawRouteMap();

    for (int i = 0; i < activeBusCount; i++) {
        drawBusIcon(i);
    }

    char footer[64];
    snprintf(footer, sizeof(footer), "Tap a bus for details     %d live buses active", activeBusCount);
    drawFooter(footer);
}

void drawRouteMap() {
    for (int i = 0; i < NUM_STOPS - 1; i++) {
        drawThickLine(stops[i].x, stops[i].y, stops[i + 1].x, stops[i + 1].y, 3, CLR_ROUTE);
    }

    for (int i = 0; i < NUM_STOPS - 1; i++) {
        int16_t mx = (stops[i].x + stops[i + 1].x) / 2;
        int16_t my = (stops[i].y + stops[i + 1].y) / 2;
        gfx->fillTriangle(mx - 5, my - 4, mx + 5, my, mx - 5, my + 4, CLR_DIM);
    }

    for (int i = 0; i < NUM_STOPS; i++) {
        gfx->fillCircle(stops[i].x, stops[i].y, 11, CLR_STOP_FILL);
        gfx->drawCircle(stops[i].x, stops[i].y, 11, CLR_STOP_RING);
        gfx->fillCircle(stops[i].x, stops[i].y, 5, CLR_STOP_DOT);

        gfx->setTextSize(1);
        gfx->setTextColor(CLR_LIGHT);
        int16_t tw = strlen(stops[i].name) * 6;
        gfx->setCursor(stops[i].x - tw / 2, stops[i].y + 17);
        gfx->print(stops[i].name);
    }
}

void drawBusIcon(int idx) {
    BusInfo  &b = buses[idx];
    BusStop  &s = stops[b.stopIndex];

    b.iconX = s.x - BUS_ICON_W / 2;
    b.iconY = s.y - BUS_ICON_H - 22;

    gfx->fillRoundRect(b.iconX, b.iconY, BUS_ICON_W, BUS_ICON_H, BUS_ICON_R, b.color);

    gfx->fillRoundRect(b.iconX + 2, b.iconY + 2, BUS_ICON_W - 4, 13, BUS_ICON_R - 2,
                        b.color > 0x2000 ? b.color - 0x1082 : 0x0000);

    gfx->drawRoundRect(b.iconX, b.iconY, BUS_ICON_W, BUS_ICON_H, BUS_ICON_R, CLR_WHITE);

    gfx->setTextSize(2);
    gfx->setTextColor(CLR_WHITE);
    int16_t nw = strlen(b.busNumber) * 12;
    gfx->setCursor(b.iconX + (BUS_ICON_W - nw) / 2, b.iconY + 17);
    gfx->print(b.busNumber);

    gfx->fillCircle(b.iconX + 14,             b.iconY + BUS_ICON_H + 1, 4, CLR_DIM);
    gfx->fillCircle(b.iconX + BUS_ICON_W - 14, b.iconY + BUS_ICON_H + 1, 4, CLR_DIM);
    gfx->fillCircle(b.iconX + 14,             b.iconY + BUS_ICON_H + 1, 2, CLR_BG);
    gfx->fillCircle(b.iconX + BUS_ICON_W - 14, b.iconY + BUS_ICON_H + 1, 2, CLR_BG);

    for (int16_t dy = b.iconY + BUS_ICON_H + 5; dy < s.y - 12; dy += 6)
        gfx->drawFastVLine(s.x, dy, 3, CLR_DIM);

    int16_t bx = b.iconX + BUS_ICON_W + 4;
    int16_t by = b.iconY + 4;
    int16_t bw = strlen(b.arrivalTime) * 6 + 10;
    gfx->fillRoundRect(bx, by, bw, 16, 4, 0x2104);
    gfx->drawRoundRect(bx, by, bw, 16, 4, CLR_DIM);
    gfx->setTextSize(1);
    gfx->setTextColor(CLR_YELLOW);
    gfx->setCursor(bx + 5, by + 4);
    gfx->print(b.arrivalTime);

    addRegion(b.iconX - 6, b.iconY - 6, BUS_ICON_W + bw + 16, BUS_ICON_H + 18, (uint8_t)idx);
}

void drawBusDetailModal(int idx) {
    clearRegions();
    BusInfo &b = buses[idx];

    gfx->fillRect(0, 0, SCREEN_WIDTH, MODAL_Y, 0x0000);
    gfx->fillRect(0, MODAL_Y, MODAL_X, MODAL_H, 0x0000);
    gfx->fillRect(MODAL_X + MODAL_W, MODAL_Y, SCREEN_WIDTH - MODAL_X - MODAL_W, MODAL_H, 0x0000);
    gfx->fillRect(0, MODAL_Y + MODAL_H, SCREEN_WIDTH, SCREEN_HEIGHT - MODAL_Y - MODAL_H, 0x0000);

    gfx->fillRoundRect(MODAL_X, MODAL_Y, MODAL_W, MODAL_H, MODAL_R, CLR_MODAL_BG);
    gfx->drawRoundRect(MODAL_X, MODAL_Y, MODAL_W, MODAL_H, MODAL_R, CLR_MODAL_BORDER);
    gfx->drawRoundRect(MODAL_X + 1, MODAL_Y + 1, MODAL_W - 2, MODAL_H - 2, MODAL_R, CLR_MODAL_BORDER);

    gfx->fillRoundRect(MODAL_X + 3, MODAL_Y + 3, MODAL_W - 6, 36, MODAL_R - 2, b.color);

    gfx->setTextSize(2);
    gfx->setTextColor(CLR_WHITE);
    gfx->setCursor(MODAL_X + 14, MODAL_Y + 12);
    gfx->print(F("Bus "));
    gfx->print(b.busNumber);
    gfx->print(F("  Details"));

    gfx->drawFastHLine(MODAL_X + 8, MODAL_Y + 44, MODAL_W - 16, CLR_MODAL_BORDER);

    int16_t col1 = MODAL_X + 18;
    int16_t col2 = MODAL_X + 195;
    int16_t rowY = MODAL_Y + 54;

    gfx->setTextSize(1);
    gfx->setTextColor(CLR_DIM);
    gfx->setCursor(col1, rowY);
    gfx->print(F("STATUS"));
    gfx->setTextSize(2);
    gfx->setTextColor(CLR_GREEN_BRIGHT);
    gfx->setCursor(col1, rowY + 14);
    gfx->print(b.arrivalTime);

    gfx->setTextSize(1);
    gfx->setTextColor(CLR_DIM);
    gfx->setCursor(col2, rowY);
    gfx->print(F("CURRENT STOP"));
    gfx->setTextSize(2);
    gfx->setTextColor(CLR_LIGHT);
    gfx->setCursor(col2, rowY + 14);
    gfx->print(stops[b.stopIndex].name);

    rowY += 46;
    gfx->setTextSize(1);
    gfx->setTextColor(CLR_DIM);
    gfx->setCursor(col1, rowY);
    gfx->print(F("ROUTE"));
    gfx->setTextSize(2);
    gfx->setTextColor(CLR_WHITE);
    gfx->setCursor(col1, rowY + 14);
    gfx->print(b.destination);

    gfx->setTextSize(1);
    gfx->setTextColor(CLR_DIM);
    gfx->setCursor(col2, rowY);
    gfx->print(F("STOPS LEFT"));
    gfx->setTextSize(2);
    gfx->setTextColor(CLR_LIGHT);
    gfx->setCursor(col2, rowY + 14);
    int remaining = (NUM_STOPS - 1) - b.stopIndex;
    gfx->print(remaining);

    int16_t btnY = MODAL_Y + MODAL_H - BTN_H - 14;
    int16_t vrBtnX = MODAL_X + 16;
    int16_t clBtnX = MODAL_X + MODAL_W - 116;

    drawButton(vrBtnX, btnY, 140, BTN_H, "View Route", CLR_BTN_PRIMARY, CLR_WHITE);
    addRegion(vrBtnX, btnY, 140, BTN_H, 100);

    drawButton(clBtnX, btnY, 100, BTN_H, "Close", CLR_BTN_SECONDARY, CLR_LIGHT);
    addRegion(clBtnX, btnY, 100, BTN_H, 101);
}

void drawRouteDetailScreen(int idx) {
    clearRegions();
    BusInfo &b = buses[idx];

    gfx->fillScreen(CLR_BG);

    char hdr[40];
    snprintf(hdr, sizeof(hdr), "Route: Bus %s", b.busNumber);
    drawHeader(hdr);

    int16_t backX = SCREEN_WIDTH - 100;
    int16_t backY = 3;
    drawButton(backX, backY, 92, 40, "< Back", CLR_BTN_SECONDARY, CLR_WHITE);
    addRegion(backX, backY, 92, 40, 200);

    int16_t rx0      = 45;
    int16_t rx1      = SCREEN_WIDTH - 45;
    int16_t routeY   = SCREEN_HEIGHT / 2 - 10;
    int16_t spacing  = (rx1 - rx0) / (NUM_STOPS - 1);

    drawThickLine(rx0, routeY, rx1, routeY, 5, CLR_ROUTE);

    if (b.stopIndex > 0) {
        int16_t hlEnd = rx0 + b.stopIndex * spacing;
        drawThickLine(rx0, routeY, hlEnd, routeY, 5, CLR_ROUTE_HI);
    }

    for (int i = 0; i < NUM_STOPS; i++) {
        int16_t sx       = rx0 + i * spacing;
        bool    busHere  = (i == b.stopIndex);
        int16_t radius   = busHere ? 16 : 9;
        uint16_t fill    = busHere ? b.color : CLR_STOP_FILL;

        gfx->fillCircle(sx, routeY, radius, fill);
        gfx->drawCircle(sx, routeY, radius, CLR_WHITE);

        if (busHere) {
            gfx->setTextSize(1);
            gfx->setTextColor(CLR_WHITE);
            int16_t nw = strlen(b.busNumber) * 6;
            gfx->setCursor(sx - nw / 2, routeY - 3);
            gfx->print(b.busNumber);

            gfx->drawCircle(sx, routeY, radius + 4, b.color);
            gfx->drawCircle(sx, routeY, radius + 8, CLR_DIM);
        }

        gfx->setTextSize(1);
        gfx->setTextColor(busHere ? b.color : CLR_LIGHT);
        int16_t tw = strlen(stops[i].name) * 6;
        gfx->setCursor(sx - tw / 2, routeY + 24);
        gfx->print(stops[i].name);

        gfx->setTextColor(CLR_DIM);
        char snum[8];
        snprintf(snum, sizeof(snum), "Stop %d", i + 1);
        int16_t sw = strlen(snum) * 6;
        gfx->setCursor(sx - sw / 2, routeY - 28);
        gfx->print(snum);
    }

    int16_t py = SCREEN_HEIGHT - 80;
    gfx->fillRoundRect(16, py, SCREEN_WIDTH - 32, 55, 6, CLR_MODAL_BG);
    gfx->drawRoundRect(16, py, SCREEN_WIDTH - 32, 55, 6, CLR_MODAL_BORDER);

    gfx->setTextSize(1);
    gfx->setTextColor(CLR_DIM);
    gfx->setCursor(28, py + 8);
    gfx->print(F("ROUTE NAME"));
    gfx->setTextColor(CLR_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(28, py + 22);
    gfx->print(b.destination);

    gfx->setTextSize(1);
    gfx->setTextColor(CLR_DIM);
    gfx->setCursor(230, py + 8);
    gfx->print(F("STATUS"));
    gfx->setTextColor(CLR_GREEN_BRIGHT);
    gfx->setTextSize(2);
    gfx->setCursor(230, py + 22);
    gfx->print(b.arrivalTime);

    gfx->setTextSize(1);
    gfx->setTextColor(CLR_DIM);
    gfx->setCursor(350, py + 8);
    gfx->print(F("STOPS LEFT"));
    gfx->setTextColor(CLR_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(350, py + 22);
    int rem = (NUM_STOPS - 1) - b.stopIndex;
    gfx->print(rem);
}

void drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const char* label, uint16_t bg, uint16_t fg) {
    gfx->fillRoundRect(x, y, w, h, BTN_R, bg);
    gfx->drawFastHLine(x + BTN_R, y + 1, w - 2 * BTN_R, bg + 0x2104);
    gfx->drawRoundRect(x, y, w, h, BTN_R, fg);

    gfx->setTextSize(2);
    gfx->setTextColor(fg);
    int16_t tw = strlen(label) * 12;
    gfx->setCursor(x + (w - tw) / 2, y + (h - 14) / 2);
    gfx->print(label);
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

void connectWiFi() {
    Serial.print(F("Connecting to SSID: "));
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 15) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(F("\n✅ Wi-Fi Connected!"));
        Serial.print(F("IP Address: "));
        Serial.println(WiFi.localIP());
    } else {
        Serial.println(F("\n❌ Wi-Fi Connection Failed. Running offline mode."));
    }
}

void fetchLiveBuses() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("API: Wi-Fi disconnected. Cannot fetch."));
        return;
    }

    Serial.println(F("Fetching live bus status from backend..."));

    HTTPClient http;
    http.begin(API_URL);
    http.setTimeout(4000);
    int httpCode = http.GET();

    if (httpCode == 200) {
        String payload = http.getString();
        
        DynamicJsonDocument doc(8192);
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
            Serial.print(F("JSON parsing failed: "));
            Serial.println(error.f_str());
            return;
        }

        JsonArray arr = doc.as<JsonArray>();
        uint8_t count = 0;

        for (JsonObject obj : arr) {
            if (count >= MAX_BUSES) break;

            BusInfo &b = buses[count];
            b.id = count;
            
            // Extract bus details
            const char* busId = obj["busId"] | "";
            const char* name = obj["name"] | "Bus";
            const char* routeName = obj["routeName"] | "Unknown Route";
            const char* type = obj["type"] | "ordinary";
            bool isOnTrip = obj["isOnTrip"] | false;

            strncpy(b.busNumber, busId, sizeof(b.busNumber));
            strncpy(b.destination, routeName, sizeof(b.destination));

            if (isOnTrip) {
                strncpy(b.arrivalTime, "ACTIVE", sizeof(b.arrivalTime));
            } else {
                strncpy(b.arrivalTime, "OFF-TRIP", sizeof(b.arrivalTime));
            }

            // Assign stopIndex based on simple layout (cycling stops or mapped indices)
            b.stopIndex = count % NUM_STOPS; 

            // Color-code based on type
            if (strcmp(type, "express") == 0) {
                b.color = CLR_BUS_2; // Orange
            } else if (strcmp(type, "ac") == 0) {
                b.color = CLR_BUS_3; // Blue
            } else {
                b.color = CLR_BUS_1; // Green
            }

            count++;
        }

        if (count > 0) {
            activeBusCount = count;
            needsRedraw = true;
            Serial.printf("Successfully updated %d active buses from database!\n", activeBusCount);
        }

    } else {
        Serial.printf("HTTP GET failed, error code: %d\n", httpCode);
    }
    http.end();
}
