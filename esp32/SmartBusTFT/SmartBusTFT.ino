/*
 * ================================================================
 *  SMART BUS TRACKING UI
 *  ESP32 + ILI9488 3.5" TFT (8-Bit Parallel) + Resistive Touch
 * ================================================================
 *
 *  Hardware Overview:
 *    - ESP32 (38-pin DevKit or equivalent)
 *    - 3.5" ILI9488 TFT LCD Shield (8-bit parallel interface)
 *    - Resistive touchscreen (NO dedicated touch controller IC)
 *    - 8GB SD card on VSPI bus
 *
 *  CRITICAL DESIGN NOTE — PIN MULTIPLEXING:
 *    The resistive touch matrix shares physical pins with the
 *    display data bus (D0, D1) and control lines (RS, CS).
 *    We use TIME-DIVISION MULTIPLEXING in loop():
 *      1. Temporarily reconfigure shared pins as INPUT for touch read
 *      2. Read the analog touch values
 *      3. IMMEDIATELY restore pins to OUTPUT before ANY gfx-> call
 *    Failure to restore pins will corrupt the display output.
 *
 *  Display Libraries:
 *    - Arduino_GFX_Library  (by moononournation)
 *      Uses Arduino_ESP32PAR8Q bus + Arduino_ILI9488 driver
 *
 *  Touch Library:
 *    - TouchScreen.h  (Adafruit / Seeed)
 *
 *  SD Card:
 *    - SD.h (standard) on VSPI with custom pin mapping
 *
 *  UI Screens:
 *    SCREEN_SPLASH       — Boot animation
 *    SCREEN_MAIN         — Route map with bus icons at stops
 *    SCREEN_BUS_DETAIL   — Modal overlay: arrival, destination, buttons
 *    SCREEN_ROUTE_DETAIL — Full route view for a selected bus
 *
 *  Author : Smart Bus Tracking Project
 *  Date   : 2026-05-23
 * ================================================================
 */

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <TouchScreen.h>
#include <SPI.h>
#include <SD.h>

/* ================================================================
 *  PIN DEFINITIONS
 * ================================================================ */

// --- TFT Display Control Pins ---
#define TFT_RST   32
#define TFT_CS    33
#define TFT_RS    15      // a.k.a. DC (Data/Command)
#define TFT_WR    4
// TFT_RD is tied permanently to 3.3V — intentionally omitted.

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
//     XP & YP connect to the data bus; XM & YM to control lines.
#define XP        21      // Shared with TFT_D0
#define YP        13      // Shared with TFT_D1
#define XM        15      // Shared with TFT_RS (DC)
#define YM        33      // Shared with TFT_CS

// --- SD Card Pins (VSPI bus, custom mapping) ---
#define SD_CS     23
#define SD_MOSI   22      // SD card D1
#define SD_MISO   19      // SD card D0
#define SD_SCK    18

/* ================================================================
 *  TOUCH CALIBRATION CONSTANTS
 * ================================================================
 *  These map the raw ADC range from the resistive panel to pixel
 *  coordinates.  Run a calibration sketch (or touch all four
 *  corners while watching Serial) to dial these in for YOUR panel.
 */
#define TS_MINX               150
#define TS_MAXX               920
#define TS_MINY               120
#define TS_MAXY               940
#define TS_MIN_PRESSURE       200
#define TS_MAX_PRESSURE       1200
#define TOUCH_DEBOUNCE_MS     300     // Min ms between accepted touches
#define TOUCH_READ_INTERVAL   50      // Poll touch every N ms

// Touch orientation — flip these if your axes are mirrored/swapped.
#define TOUCH_SWAP_XY         false
#define TOUCH_FLIP_X          false
#define TOUCH_FLIP_Y          false

/* ================================================================
 *  SCREEN DIMENSIONS  &  UI LAYOUT CONSTANTS
 * ================================================================
 *  ILI9488 native resolution = 480 × 320.
 *  rotation=1 gives landscape: W=480, H=320.
 *  Adjust SCREEN_WIDTH / SCREEN_HEIGHT if your panel differs.
 */
#define SCREEN_WIDTH          480
#define SCREEN_HEIGHT         320

// Vertical zones
#define HEADER_H              44
#define FOOTER_H              32
#define CONTENT_Y             (HEADER_H + 1)
#define CONTENT_H             (SCREEN_HEIGHT - HEADER_H - FOOTER_H)

// Bus icon touch target (min 40×40 for resistive touch)
#define BUS_ICON_W            62
#define BUS_ICON_H            46
#define BUS_ICON_R            8       // Corner radius

// Buttons
#define BTN_H                 42
#define BTN_R                 6

// Modal
#define MODAL_W               360
#define MODAL_H               210
#define MODAL_X               ((SCREEN_WIDTH  - MODAL_W) / 2)
#define MODAL_Y               ((SCREEN_HEIGHT - MODAL_H) / 2)
#define MODAL_R               10

/* ================================================================
 *  COLOR PALETTE  (RGB-565)
 * ================================================================ */

// Backgrounds
#define CLR_BG                0x0841    // Very dark navy
#define CLR_HEADER_BG         0x1126    // Deep blue
#define CLR_HEADER_ACCENT     0x2B5F    // Bright blue accent stripe
#define CLR_FOOTER_BG         0x0841

// Text
#define CLR_WHITE             0xFFFF
#define CLR_LIGHT             0xC638    // Light grey
#define CLR_DIM               0x7BEF    // Medium grey
#define CLR_YELLOW            0xFFE0

// Route map
#define CLR_ROUTE             0x4A69    // Grey route line
#define CLR_ROUTE_HI          0x07FF    // Cyan highlight
#define CLR_STOP_FILL         0xFFFF
#define CLR_STOP_RING         0x4A69
#define CLR_STOP_DOT          0x2B5F    // Accent dot inside stop

// Bus icon colours
#define CLR_BUS_1             0x2E6A    // Forest green
#define CLR_BUS_2             0xFC80    // Warm orange
#define CLR_BUS_3             0x34BF    // Sky blue
#define CLR_BUS_4             0xE8C4    // Coral red

// Modal / buttons
#define CLR_MODAL_BG          0x18C3
#define CLR_MODAL_BORDER      0x3186
#define CLR_BTN_PRIMARY       0x2E6A    // Green
#define CLR_BTN_SECONDARY     0x4208    // Dark grey
#define CLR_GREEN_BRIGHT      0x07E0

/* ================================================================
 *  DATA  STRUCTURES
 * ================================================================ */

// --- UI state machine ---
enum ScreenState {
    SCREEN_SPLASH,
    SCREEN_MAIN,
    SCREEN_BUS_DETAIL,
    SCREEN_ROUTE_DETAIL
};

// --- Bus stop along the route ---
struct BusStop {
    const char* name;
    int16_t     x;      // Screen X (set for the main-screen map layout)
    int16_t     y;      // Screen Y
};

// --- Individual bus record ---
//     Placeholder fields; replace with live MQTT / HTTP data later.
struct BusInfo {
    uint8_t     id;
    const char* busNumber;      // "101", "42-A", etc.
    const char* destination;
    const char* arrivalTime;    // Human-readable ETA string
    uint8_t     stopIndex;      // Which stop the bus is currently at
    uint16_t    color;          // RGB-565 icon colour
    // Computed each time the main screen is drawn:
    int16_t     iconX;
    int16_t     iconY;
};

// --- Rectangular touch-hit region ---
struct TouchRegion {
    int16_t  x, y, w, h;
    uint8_t  id;                // Arbitrary identifier
};

/* ================================================================
 *  PLACEHOLDER  BUS  DATA
 * ================================================================
 *  TODO: Replace with live data injected via WiFi / BLE / Serial.
 *        When new data arrives, update the arrays below and set
 *        needsRedraw = true.
 */

#define NUM_STOPS   5
BusStop stops[NUM_STOPS] = {
    { "Central Stn",  55, 175 },
    { "Park Ave",    160, 132 },
    { "Market St",   265, 175 },
    { "Tech Hub",    370, 132 },
    { "Airport",     445, 175 }
};

#define NUM_BUSES   3
BusInfo buses[NUM_BUSES] = {
    //  id  number  destination      ETA       stop  colour       ix  iy
    {   1,  "101",  "Airport",       "3 min",  0,    CLR_BUS_1,   0,  0 },
    {   2,  "205",  "Tech Hub",      "7 min",  2,    CLR_BUS_2,   0,  0 },
    {   3,  "42",   "Central Stn",   "12 min", 3,    CLR_BUS_3,   0,  0 }
};

/* ================================================================
 *  GLOBAL  OBJECTS
 * ================================================================ */

// Display — 8-bit parallel bus → ILI9488 driver
Arduino_DataBus *dataBus = new Arduino_ESP32PAR8Q(
    TFT_RS  /* DC  */,
    TFT_CS  /* CS  */,
    TFT_WR  /* WR  */,
    GFX_NOT_DEFINED /* RD — tied to 3.3 V */,
    TFT_D0, TFT_D1, TFT_D2, TFT_D3,
    TFT_D4, TFT_D5, TFT_D6, TFT_D7
);

Arduino_GFX *gfx = new Arduino_ILI9488(
    dataBus,
    TFT_RST,
    1,       // rotation = 1 → landscape (480 × 320)
    false    // IPS panel = false
);

// Touch — resistive panel on shared pins
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// SD card — separate SPI instance on VSPI
SPIClass sdSPI(VSPI);

/* ================================================================
 *  STATE  VARIABLES
 * ================================================================ */

ScreenState currentScreen    = SCREEN_SPLASH;
int8_t      selectedBusIdx   = -1;      // Index into buses[]
bool        needsRedraw      = true;
bool        sdCardReady      = false;

// Touch timing
unsigned long lastTouchTime      = 0;
unsigned long lastTouchPollTime  = 0;
bool          touchWasActive     = false;

// Touch-region registry for the active screen
#define MAX_REGIONS  12
TouchRegion  regions[MAX_REGIONS];
uint8_t      regionCount = 0;

/* ================================================================
 *  FUNCTION  PROTOTYPES
 * ================================================================ */

// Core
void   restoreDisplayPins();
bool   readTouch(int16_t &tx, int16_t &ty);
void   handleTouch(int16_t tx, int16_t ty);

// Touch regions
void   clearRegions();
void   addRegion(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t id);
int8_t hitTest(int16_t tx, int16_t ty);

// Screens
void   drawSplashScreen();
void   drawMainScreen();
void   drawBusDetailModal(int idx);
void   drawRouteDetailScreen(int idx);

// UI primitives
void   drawHeader(const char* title);
void   drawFooter(const char* msg);
void   drawRouteMap();
void   drawBusIcon(int idx);
void   drawButton(int16_t x, int16_t y, int16_t w, int16_t h,
                   const char* label, uint16_t bg, uint16_t fg);
void   drawThickLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      uint8_t t, uint16_t c);

// SD card
bool   initSDCard();
void   drawBmpFromSD(const char* path, int16_t x, int16_t y);

/* ================================================================
 *
 *  S E T U P
 *
 * ================================================================ */

void setup() {
    Serial.begin(115200);
    Serial.println(F("\n========================================"));
    Serial.println(F("  Smart Bus Tracker — Booting"));
    Serial.println(F("========================================"));

    // ---- Initialise display ----
    if (!gfx->begin()) {
        Serial.println(F("FATAL: gfx->begin() failed.  Halting."));
        while (true) delay(100);
    }
    gfx->fillScreen(CLR_BG);
    gfx->setTextWrap(false);
    Serial.println(F("[OK] Display: ILI9488  480x320  Landscape"));

    // ---- Initialise SD card (optional) ----
    sdCardReady = initSDCard();
    Serial.println(sdCardReady
        ? F("[OK] SD card mounted")
        : F("[--] SD card not available — geometric UI only"));

    // ---- Splash screen ----
    drawSplashScreen();
    delay(2200);

    // ---- Transition to main UI ----
    currentScreen = SCREEN_MAIN;
    needsRedraw   = true;

    Serial.println(F("[OK] Boot complete — entering main loop\n"));
}

/* ================================================================
 *
 *  M A I N   L O O P
 *
 *  Cycle:
 *    1.  Poll resistive touch  (reconfigures shared pins briefly)
 *    2.  Process any touch event through the state machine
 *    3.  Redraw the active screen if the dirty flag is set
 *    4.  (Placeholder) Ingest live bus data updates
 *
 * ================================================================ */

void loop() {

    // ---- 1. TOUCH READ (time-division multiplex) ----
    unsigned long now = millis();
    int16_t tx = -1, ty = -1;
    bool    touched = false;

    if (now - lastTouchPollTime >= TOUCH_READ_INTERVAL) {
        lastTouchPollTime = now;
        touched = readTouch(tx, ty);
        // NOTE: restoreDisplayPins() is called inside readTouch()
        //       so the bus is already safe for gfx-> calls here.
    }

    // ---- 2. HANDLE TOUCH ----
    if (touched) {
        handleTouch(tx, ty);
    }

    // ---- 3. REDRAW (only when dirty) ----
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

    // ---- 4. LIVE DATA PLACEHOLDER ----
    // TODO: Periodically poll your backend or read from MQTT / BLE
    //       for real-time bus positions.  When new data arrives:
    //
    //   updateBusPositions();                     // parse into buses[]
    //   if (currentScreen == SCREEN_MAIN)
    //       needsRedraw = true;
}

/* ================================================================
 *  restoreDisplayPins()
 * ================================================================
 *  After the TouchScreen library reads the resistive panel it
 *  leaves the shared pins (XP, YP, XM, YM) in INPUT / ANALOG
 *  mode.  We MUST switch them back to OUTPUT before performing
 *  any display writes, otherwise the parallel bus is broken and
 *  the screen shows garbage.
 */
void restoreDisplayPins() {
    pinMode(XP, OUTPUT);      // → TFT_D0
    pinMode(YP, OUTPUT);      // → TFT_D1
    pinMode(XM, OUTPUT);      // → TFT_RS  (DC)
    pinMode(YM, OUTPUT);      // → TFT_CS
}

/* ================================================================
 *  readTouch()
 * ================================================================
 *  1. Calls ts.getPoint()       — this mutates pin modes internally
 *  2. Immediately calls restoreDisplayPins()
 *  3. Validates pressure, applies debounce timing
 *  4. Maps raw ADC → screen pixel coordinates
 *  5. Optionally swaps / flips axes
 *
 *  Returns true  when a valid, debounced press is detected.
 *  tx, ty are filled with pixel coordinates on the screen.
 */
bool readTouch(int16_t &tx, int16_t &ty) {

    // -- read raw analog values (CHANGES pin modes!) --
    TSPoint p = ts.getPoint();

    // -- IMMEDIATELY restore pins for the display bus --
    restoreDisplayPins();

    // -- Pressure gate --
    if (p.z < TS_MIN_PRESSURE || p.z > TS_MAX_PRESSURE) {
        touchWasActive = false;         // finger lifted
        return false;
    }

    // -- Debounce: reject if still within guard window --
    //    We also require the finger to have been lifted (touchWasActive
    //    == false) so that a long press doesn't rapid-fire.
    unsigned long now = millis();
    if (touchWasActive) return false;
    if (now - lastTouchTime < TOUCH_DEBOUNCE_MS) return false;

    // -- Map raw ADC → pixel coordinates --
    tx = map(p.x, TS_MINX, TS_MAXX, 0, SCREEN_WIDTH);
    ty = map(p.y, TS_MINY, TS_MAXY, 0, SCREEN_HEIGHT);

    // -- Optional axis adjustments --
#if TOUCH_SWAP_XY
    int16_t tmp = tx; tx = ty; ty = tmp;
#endif
#if TOUCH_FLIP_X
    tx = SCREEN_WIDTH  - 1 - tx;
#endif
#if TOUCH_FLIP_Y
    ty = SCREEN_HEIGHT - 1 - ty;
#endif

    // -- Clamp to screen bounds --
    tx = constrain(tx, 0, SCREEN_WIDTH  - 1);
    ty = constrain(ty, 0, SCREEN_HEIGHT - 1);

    // -- Accept this touch --
    lastTouchTime  = now;
    touchWasActive = true;

    Serial.printf("[TOUCH] screen(%d,%d)  raw(%d,%d)  z=%d\n",
                  tx, ty, p.x, p.y, p.z);
    return true;
}

/* ================================================================
 *  handleTouch()
 * ================================================================
 *  Dispatches the touch event to the correct handler based on
 *  the current ScreenState.  Touch regions are populated by the
 *  draw* functions so they always match what's visible on screen.
 */
void handleTouch(int16_t tx, int16_t ty) {

    int8_t rid = hitTest(tx, ty);
    if (rid < 0) return;                // tapped empty space

    switch (currentScreen) {

        /* ---- MAIN SCREEN ---- */
        case SCREEN_MAIN:
            // Region IDs 0 .. NUM_BUSES-1 = bus icons
            if (rid < NUM_BUSES) {
                selectedBusIdx = rid;
                currentScreen  = SCREEN_BUS_DETAIL;
                needsRedraw    = true;
                Serial.printf("[UI] Opened details for Bus %s\n",
                              buses[rid].busNumber);
            }
            break;

        /* ---- BUS DETAIL MODAL ---- */
        case SCREEN_BUS_DETAIL:
            if (rid == 100) {               // "View Route"
                currentScreen = SCREEN_ROUTE_DETAIL;
                needsRedraw   = true;
            } else if (rid == 101) {        // "Close"
                selectedBusIdx = -1;
                currentScreen  = SCREEN_MAIN;
                needsRedraw    = true;
            }
            break;

        /* ---- ROUTE DETAIL SCREEN ---- */
        case SCREEN_ROUTE_DETAIL:
            if (rid == 200) {               // "< Back"
                selectedBusIdx = -1;
                currentScreen  = SCREEN_MAIN;
                needsRedraw    = true;
            }
            break;

        default: break;
    }
}

/* ================================================================
 *  TOUCH  REGION  HELPERS
 * ================================================================ */

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
        if (tx >= r.x && tx < r.x + r.w &&
            ty >= r.y && ty < r.y + r.h) {
            return (int8_t)r.id;
        }
    }
    return -1;
}

/* ================================================================
 *
 *  D R A W I N G   F U N C T I O N S
 *
 * ================================================================ */

/* ----------------------------------------------------------------
 *  SPLASH  SCREEN
 * ---------------------------------------------------------------- */
void drawSplashScreen() {
    gfx->fillScreen(CLR_BG);

    // Top accent stripe
    for (int i = 0; i < 4; i++)
        gfx->drawFastHLine(0, i, SCREEN_WIDTH, CLR_HEADER_ACCENT);

    // ---- Geometric bus icon (centred) ----
    int16_t bx = SCREEN_WIDTH  / 2 - 40;
    int16_t by = SCREEN_HEIGHT / 2 - 60;

    // Body
    gfx->fillRoundRect(bx, by, 80, 50, 8, CLR_BUS_1);
    // Windshield strip
    gfx->fillRect(bx + 4, by + 4, 72, 2, CLR_LIGHT);
    // Windows
    gfx->fillRect(bx + 8,  by + 10, 16, 16, CLR_BG);
    gfx->fillRect(bx + 28, by + 10, 16, 16, CLR_BG);
    gfx->fillRect(bx + 48, by + 10, 16, 16, CLR_BG);
    // Door
    gfx->fillRect(bx + 68, by + 18, 8, 28, CLR_DIM);
    gfx->drawRect(bx + 68, by + 18, 8, 28, CLR_LIGHT);
    // Wheels
    gfx->fillCircle(bx + 18, by + 53, 7, CLR_DIM);
    gfx->fillCircle(bx + 18, by + 53, 3, CLR_BG);
    gfx->fillCircle(bx + 60, by + 53, 7, CLR_DIM);
    gfx->fillCircle(bx + 60, by + 53, 3, CLR_BG);
    // Headlight
    gfx->fillCircle(bx + 3, by + 38, 4, CLR_YELLOW);

    // ---- Title ----
    gfx->setTextSize(3);
    gfx->setTextColor(CLR_WHITE);
    gfx->setCursor(SCREEN_WIDTH / 2 - 142, SCREEN_HEIGHT / 2 + 14);
    gfx->print(F("Smart Bus Tracker"));

    // ---- Tagline ----
    gfx->setTextSize(1);
    gfx->setTextColor(CLR_DIM);
    gfx->setCursor(SCREEN_WIDTH / 2 - 60, SCREEN_HEIGHT / 2 + 48);
    gfx->print(F("Loading route data..."));

    // Bottom accent stripe
    for (int i = 0; i < 4; i++)
        gfx->drawFastHLine(0, SCREEN_HEIGHT - 1 - i,
                            SCREEN_WIDTH, CLR_HEADER_ACCENT);
}

/* ----------------------------------------------------------------
 *  HEADER  BAR
 * ---------------------------------------------------------------- */
void drawHeader(const char* title) {
    // Background
    gfx->fillRect(0, 0, SCREEN_WIDTH, HEADER_H, CLR_HEADER_BG);
    // Accent stripe along bottom edge
    gfx->drawFastHLine(0, HEADER_H - 2, SCREEN_WIDTH, CLR_HEADER_ACCENT);
    gfx->drawFastHLine(0, HEADER_H - 1, SCREEN_WIDTH, CLR_HEADER_ACCENT);

    // Mini bus icon in header
    gfx->fillRoundRect(10, 8, 26, 18, 3, CLR_BUS_1);
    gfx->fillRect(13, 11, 7, 7, CLR_HEADER_BG);
    gfx->fillRect(22, 11, 7, 7, CLR_HEADER_BG);
    gfx->fillCircle(16, 28, 3, CLR_DIM);
    gfx->fillCircle(31, 28, 3, CLR_DIM);

    // Title text
    gfx->setTextSize(2);
    gfx->setTextColor(CLR_WHITE);
    gfx->setCursor(44, 14);
    gfx->print(title);

    // "LIVE" indicator (top-right)
    gfx->fillCircle(SCREEN_WIDTH - 72, 22, 4, CLR_GREEN_BRIGHT);
    gfx->setTextSize(1);
    gfx->setTextColor(CLR_GREEN_BRIGHT);
    gfx->setCursor(SCREEN_WIDTH - 64, 18);
    gfx->print(F("LIVE"));

    // Placeholder clock
    gfx->setTextColor(CLR_LIGHT);
    gfx->setCursor(SCREEN_WIDTH - 50, 8);
    gfx->print(F("23:25"));
}

/* ----------------------------------------------------------------
 *  FOOTER  BAR
 * ---------------------------------------------------------------- */
void drawFooter(const char* msg) {
    int16_t fy = SCREEN_HEIGHT - FOOTER_H;
    gfx->fillRect(0, fy, SCREEN_WIDTH, FOOTER_H, CLR_FOOTER_BG);
    gfx->drawFastHLine(0, fy, SCREEN_WIDTH, CLR_MODAL_BORDER);

    gfx->setTextSize(1);
    gfx->setTextColor(CLR_DIM);
    gfx->setCursor(10, fy + 11);
    gfx->print(msg);
}

/* ----------------------------------------------------------------
 *  MAIN  SCREEN
 * ---------------------------------------------------------------- */
void drawMainScreen() {
    clearRegions();
    gfx->fillScreen(CLR_BG);

    drawHeader("Smart Bus Tracker");
    drawRouteMap();

    // Draw each bus icon & register its touch region
    for (int i = 0; i < NUM_BUSES; i++) {
        drawBusIcon(i);
    }

    // Footer
    char footer[64];
    snprintf(footer, sizeof(footer),
             "Tap a bus for details        %d buses in service", NUM_BUSES);
    drawFooter(footer);
}

/* ----------------------------------------------------------------
 *  ROUTE  MAP  (geometric, no bitmap needed)
 * ---------------------------------------------------------------- */
void drawRouteMap() {

    // ---- Connecting route lines ----
    for (int i = 0; i < NUM_STOPS - 1; i++) {
        drawThickLine(stops[i].x, stops[i].y,
                      stops[i + 1].x, stops[i + 1].y,
                      3, CLR_ROUTE);
    }

    // ---- Direction arrows between stops ----
    for (int i = 0; i < NUM_STOPS - 1; i++) {
        int16_t mx = (stops[i].x + stops[i + 1].x) / 2;
        int16_t my = (stops[i].y + stops[i + 1].y) / 2;
        gfx->fillTriangle(mx - 5, my - 4,
                           mx + 5, my,
                           mx - 5, my + 4,
                           CLR_DIM);
    }

    // ---- Stop markers ----
    for (int i = 0; i < NUM_STOPS; i++) {
        // Outer ring
        gfx->fillCircle(stops[i].x, stops[i].y, 11, CLR_STOP_FILL);
        gfx->drawCircle(stops[i].x, stops[i].y, 11, CLR_STOP_RING);
        // Inner accent dot
        gfx->fillCircle(stops[i].x, stops[i].y, 5, CLR_STOP_DOT);

        // Stop name label (centred below marker)
        gfx->setTextSize(1);
        gfx->setTextColor(CLR_LIGHT);
        int16_t tw = strlen(stops[i].name) * 6;   // ~6 px per char
        gfx->setCursor(stops[i].x - tw / 2, stops[i].y + 17);
        gfx->print(stops[i].name);
    }
}

/* ----------------------------------------------------------------
 *  BUS  ICON  (drawn above the stop it's currently at)
 * ---------------------------------------------------------------- */
void drawBusIcon(int idx) {
    BusInfo  &b = buses[idx];
    BusStop  &s = stops[b.stopIndex];

    // Position icon above its stop circle
    b.iconX = s.x - BUS_ICON_W / 2;
    b.iconY = s.y - BUS_ICON_H - 22;

    // ---- Body ----
    gfx->fillRoundRect(b.iconX, b.iconY,
                        BUS_ICON_W, BUS_ICON_H,
                        BUS_ICON_R, b.color);

    // Subtle darker header strip inside icon
    gfx->fillRoundRect(b.iconX + 2, b.iconY + 2,
                        BUS_ICON_W - 4, 13,
                        BUS_ICON_R - 2,
                        b.color > 0x2000 ? b.color - 0x1082 : 0x0000);

    // Border
    gfx->drawRoundRect(b.iconX, b.iconY,
                        BUS_ICON_W, BUS_ICON_H,
                        BUS_ICON_R, CLR_WHITE);

    // ---- Bus number text (centred) ----
    gfx->setTextSize(2);
    gfx->setTextColor(CLR_WHITE);
    int16_t nw = strlen(b.busNumber) * 12;
    gfx->setCursor(b.iconX + (BUS_ICON_W - nw) / 2,
                    b.iconY + 17);
    gfx->print(b.busNumber);

    // ---- Mini wheels ----
    gfx->fillCircle(b.iconX + 14,             b.iconY + BUS_ICON_H + 1, 4, CLR_DIM);
    gfx->fillCircle(b.iconX + BUS_ICON_W - 14, b.iconY + BUS_ICON_H + 1, 4, CLR_DIM);
    gfx->fillCircle(b.iconX + 14,             b.iconY + BUS_ICON_H + 1, 2, CLR_BG);
    gfx->fillCircle(b.iconX + BUS_ICON_W - 14, b.iconY + BUS_ICON_H + 1, 2, CLR_BG);

    // ---- Dashed connector to stop circle ----
    for (int16_t dy = b.iconY + BUS_ICON_H + 5; dy < s.y - 12; dy += 6)
        gfx->drawFastVLine(s.x, dy, 3, CLR_DIM);

    // ---- ETA badge (to the right of the icon) ----
    int16_t bx = b.iconX + BUS_ICON_W + 4;
    int16_t by = b.iconY + 4;
    int16_t bw = strlen(b.arrivalTime) * 6 + 10;
    gfx->fillRoundRect(bx, by, bw, 16, 4, 0x2104);
    gfx->drawRoundRect(bx, by, bw, 16, 4, CLR_DIM);
    gfx->setTextSize(1);
    gfx->setTextColor(CLR_YELLOW);
    gfx->setCursor(bx + 5, by + 4);
    gfx->print(b.arrivalTime);

    // ---- Register touch region (generous target) ----
    addRegion(b.iconX - 6, b.iconY - 6,
              BUS_ICON_W + bw + 16, BUS_ICON_H + 18,
              (uint8_t)idx);
}

/* ----------------------------------------------------------------
 *  BUS  DETAIL  MODAL  (popup overlay)
 * ---------------------------------------------------------------- */
void drawBusDetailModal(int idx) {
    clearRegions();
    BusInfo &b = buses[idx];

    // ---- Dim overlay (we can't do real alpha, so fill black) ----
    //      We draw the overlay in strips around the modal.
    gfx->fillRect(0, 0, SCREEN_WIDTH, MODAL_Y, 0x0000);
    gfx->fillRect(0, MODAL_Y, MODAL_X, MODAL_H, 0x0000);
    gfx->fillRect(MODAL_X + MODAL_W, MODAL_Y,
                   SCREEN_WIDTH - MODAL_X - MODAL_W, MODAL_H, 0x0000);
    gfx->fillRect(0, MODAL_Y + MODAL_H,
                   SCREEN_WIDTH, SCREEN_HEIGHT - MODAL_Y - MODAL_H, 0x0000);

    // ---- Modal card ----
    gfx->fillRoundRect(MODAL_X, MODAL_Y,
                        MODAL_W, MODAL_H, MODAL_R, CLR_MODAL_BG);
    // Double border
    gfx->drawRoundRect(MODAL_X, MODAL_Y,
                        MODAL_W, MODAL_H, MODAL_R, CLR_MODAL_BORDER);
    gfx->drawRoundRect(MODAL_X + 1, MODAL_Y + 1,
                        MODAL_W - 2, MODAL_H - 2,
                        MODAL_R, CLR_MODAL_BORDER);

    // ---- Coloured title bar ----
    gfx->fillRoundRect(MODAL_X + 3, MODAL_Y + 3,
                        MODAL_W - 6, 36,
                        MODAL_R - 2, b.color);

    gfx->setTextSize(2);
    gfx->setTextColor(CLR_WHITE);
    gfx->setCursor(MODAL_X + 14, MODAL_Y + 12);
    gfx->print(F("Bus "));
    gfx->print(b.busNumber);
    gfx->print(F("  Details"));

    // Divider
    gfx->drawFastHLine(MODAL_X + 8, MODAL_Y + 44,
                        MODAL_W - 16, CLR_MODAL_BORDER);

    // ---- Info rows ----
    int16_t col1 = MODAL_X + 18;
    int16_t col2 = MODAL_X + 195;
    int16_t rowY = MODAL_Y + 54;

    // Arrival Time
    gfx->setTextSize(1);
    gfx->setTextColor(CLR_DIM);
    gfx->setCursor(col1, rowY);
    gfx->print(F("ARRIVAL TIME"));
    gfx->setTextSize(2);
    gfx->setTextColor(CLR_GREEN_BRIGHT);
    gfx->setCursor(col1, rowY + 14);
    gfx->print(b.arrivalTime);

    // Current Stop
    gfx->setTextSize(1);
    gfx->setTextColor(CLR_DIM);
    gfx->setCursor(col2, rowY);
    gfx->print(F("CURRENT STOP"));
    gfx->setTextSize(2);
    gfx->setTextColor(CLR_LIGHT);
    gfx->setCursor(col2, rowY + 14);
    gfx->print(stops[b.stopIndex].name);

    // Destination
    rowY += 46;
    gfx->setTextSize(1);
    gfx->setTextColor(CLR_DIM);
    gfx->setCursor(col1, rowY);
    gfx->print(F("DESTINATION"));
    gfx->setTextSize(2);
    gfx->setTextColor(CLR_WHITE);
    gfx->setCursor(col1, rowY + 14);
    gfx->print(b.destination);

    // Stops remaining
    gfx->setTextSize(1);
    gfx->setTextColor(CLR_DIM);
    gfx->setCursor(col2, rowY);
    gfx->print(F("STOPS LEFT"));
    gfx->setTextSize(2);
    gfx->setTextColor(CLR_LIGHT);
    gfx->setCursor(col2, rowY + 14);
    int remaining = (NUM_STOPS - 1) - b.stopIndex;
    gfx->print(remaining);

    // ---- Buttons ----
    int16_t btnY = MODAL_Y + MODAL_H - BTN_H - 14;
    int16_t vrBtnX = MODAL_X + 16;
    int16_t clBtnX = MODAL_X + MODAL_W - 116;

    drawButton(vrBtnX, btnY, 140, BTN_H,
               "View Route", CLR_BTN_PRIMARY, CLR_WHITE);
    addRegion(vrBtnX, btnY, 140, BTN_H, 100);

    drawButton(clBtnX, btnY, 100, BTN_H,
               "Close", CLR_BTN_SECONDARY, CLR_LIGHT);
    addRegion(clBtnX, btnY, 100, BTN_H, 101);
}

/* ----------------------------------------------------------------
 *  ROUTE  DETAIL  SCREEN
 * ---------------------------------------------------------------- */
void drawRouteDetailScreen(int idx) {
    clearRegions();
    BusInfo &b = buses[idx];

    gfx->fillScreen(CLR_BG);

    // Header with embedded Back button
    char hdr[40];
    snprintf(hdr, sizeof(hdr), "Route: Bus %s", b.busNumber);
    drawHeader(hdr);

    // "< Back" button overlaid on header (top-right area)
    int16_t backX = SCREEN_WIDTH - 100;
    int16_t backY = 3;
    drawButton(backX, backY, 92, 40, "< Back", CLR_BTN_SECONDARY, CLR_WHITE);
    addRegion(backX, backY, 92, 40, 200);

    // ---- Linear route visualisation ----
    int16_t rx0      = 45;
    int16_t rx1      = SCREEN_WIDTH - 45;
    int16_t routeY   = SCREEN_HEIGHT / 2 - 10;
    int16_t spacing  = (rx1 - rx0) / (NUM_STOPS - 1);

    // Route backbone
    drawThickLine(rx0, routeY, rx1, routeY, 5, CLR_ROUTE);

    // Highlight segment up to bus position
    if (b.stopIndex > 0) {
        int16_t hlEnd = rx0 + b.stopIndex * spacing;
        drawThickLine(rx0, routeY, hlEnd, routeY, 5, CLR_ROUTE_HI);
    }

    // ---- Stop circles along route ----
    for (int i = 0; i < NUM_STOPS; i++) {
        int16_t sx       = rx0 + i * spacing;
        bool    busHere  = (i == b.stopIndex);
        int16_t radius   = busHere ? 16 : 9;
        uint16_t fill    = busHere ? b.color : CLR_STOP_FILL;

        gfx->fillCircle(sx, routeY, radius, fill);
        gfx->drawCircle(sx, routeY, radius, CLR_WHITE);

        if (busHere) {
            // Bus number inside the large circle
            gfx->setTextSize(1);
            gfx->setTextColor(CLR_WHITE);
            int16_t nw = strlen(b.busNumber) * 6;
            gfx->setCursor(sx - nw / 2, routeY - 3);
            gfx->print(b.busNumber);

            // Pulsing rings (static representation)
            gfx->drawCircle(sx, routeY, radius + 4, b.color);
            gfx->drawCircle(sx, routeY, radius + 8, CLR_DIM);
        }

        // Stop label — below route
        gfx->setTextSize(1);
        gfx->setTextColor(busHere ? b.color : CLR_LIGHT);
        int16_t tw = strlen(stops[i].name) * 6;
        gfx->setCursor(sx - tw / 2, routeY + 24);
        gfx->print(stops[i].name);

        // Stop number — above route
        gfx->setTextColor(CLR_DIM);
        char snum[8];
        snprintf(snum, sizeof(snum), "Stop %d", i + 1);
        int16_t sw = strlen(snum) * 6;
        gfx->setCursor(sx - sw / 2, routeY - 28);
        gfx->print(snum);
    }

    // ---- Info panel at bottom ----
    int16_t py = SCREEN_HEIGHT - 80;
    gfx->fillRoundRect(16, py, SCREEN_WIDTH - 32, 55, 6, CLR_MODAL_BG);
    gfx->drawRoundRect(16, py, SCREEN_WIDTH - 32, 55, 6, CLR_MODAL_BORDER);

    // Column 1: Destination
    gfx->setTextSize(1);
    gfx->setTextColor(CLR_DIM);
    gfx->setCursor(28, py + 8);
    gfx->print(F("DESTINATION"));
    gfx->setTextColor(CLR_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(28, py + 22);
    gfx->print(b.destination);

    // Column 2: ETA
    gfx->setTextSize(1);
    gfx->setTextColor(CLR_DIM);
    gfx->setCursor(230, py + 8);
    gfx->print(F("ETA"));
    gfx->setTextColor(CLR_GREEN_BRIGHT);
    gfx->setTextSize(2);
    gfx->setCursor(230, py + 22);
    gfx->print(b.arrivalTime);

    // Column 3: Stops remaining
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

/* ================================================================
 *  UI  PRIMITIVES
 * ================================================================ */

/* ---- Rounded button with centred label ---- */
void drawButton(int16_t x, int16_t y, int16_t w, int16_t h,
                 const char* label, uint16_t bg, uint16_t fg) {

    gfx->fillRoundRect(x, y, w, h, BTN_R, bg);

    // Subtle highlight along top edge for 3-D feel
    gfx->drawFastHLine(x + BTN_R, y + 1,
                        w - 2 * BTN_R,
                        bg + 0x2104);

    gfx->drawRoundRect(x, y, w, h, BTN_R, fg);

    // Centre text
    gfx->setTextSize(2);
    gfx->setTextColor(fg);
    int16_t tw = strlen(label) * 12;
    gfx->setCursor(x + (w - tw) / 2,
                    y + (h - 14) / 2);
    gfx->print(label);
}

/* ---- Multi-pixel-thick line (offset parallel lines) ---- */
void drawThickLine(int16_t x0, int16_t y0,
                    int16_t x1, int16_t y1,
                    uint8_t t, uint16_t c) {

    int16_t half = t / 2;

    if (abs(x1 - x0) >= abs(y1 - y0)) {
        // Mostly horizontal → offset in Y
        for (int16_t off = -half; off <= half; off++)
            gfx->drawLine(x0, y0 + off, x1, y1 + off, c);
    } else {
        // Mostly vertical → offset in X
        for (int16_t off = -half; off <= half; off++)
            gfx->drawLine(x0 + off, y0, x1 + off, y1, c);
    }
}

/* ================================================================
 *  SD  CARD  INITIALISATION
 * ================================================================ */

bool initSDCard() {
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS, sdSPI)) {
        return false;
    }

    uint8_t type = SD.cardType();
    if (type == CARD_NONE) {
        Serial.println(F("[SD] No card detected"));
        return false;
    }

    Serial.printf("[SD] Type: %s   Size: %llu MB\n",
                  type == CARD_MMC  ? "MMC"  :
                  type == CARD_SD   ? "SD"   :
                  type == CARD_SDHC ? "SDHC" : "?",
                  SD.cardSize() / (1024ULL * 1024));
    return true;
}

/* ================================================================
 *  BMP  LOADER  (optional utility for SD-based route maps)
 * ================================================================
 *
 *  Draws a 24-bit uncompressed BMP file from the SD card to the
 *  screen at coordinates (x, y).
 *
 *  Usage:
 *    drawBmpFromSD("/maps/route_101.bmp", 0, HEADER_H);
 *
 *  Notes:
 *    - File must be 24-bit, uncompressed, bottom-up BMP.
 *    - Export from any image editor → "Windows BMP", 24 bits.
 *    - Large images are slow because we draw pixel-by-pixel.
 *      For speed, consider pre-converting to a raw RGB565 file
 *      and using gfx->draw16bitRGBBitmap().
 */
void drawBmpFromSD(const char* path, int16_t x, int16_t y) {

    if (!sdCardReady) {
        Serial.println(F("[BMP] SD card not ready"));
        return;
    }

    File f = SD.open(path);
    if (!f) {
        Serial.printf("[BMP] File not found: %s\n", path);
        return;
    }

    // Validate BMP signature
    if (f.read() != 'B' || f.read() != 'M') {
        Serial.println(F("[BMP] Invalid BMP header"));
        f.close();
        return;
    }

    // Read pixel-data offset (bytes 10-13)
    f.seek(10);
    uint32_t dataOffset = 0;
    f.read((uint8_t*)&dataOffset, 4);

    // Read width & height (bytes 18-25)
    f.seek(18);
    int32_t bmpW = 0, bmpH = 0;
    f.read((uint8_t*)&bmpW, 4);
    f.read((uint8_t*)&bmpH, 4);

    if (bmpW <= 0 || bmpH == 0) {
        Serial.println(F("[BMP] Invalid dimensions"));
        f.close();
        return;
    }

    bool bottomUp = (bmpH > 0);
    if (!bottomUp) bmpH = -bmpH;

    // Row size is padded to 4-byte boundary
    uint32_t rowSize = ((bmpW * 3) + 3) & ~3;

    // Use a stack buffer for one row (safe up to ~500 px wide)
    const uint16_t MAX_ROW_BYTES = 1440;   // 480 px * 3
    uint8_t rowBuf[MAX_ROW_BYTES];

    if (rowSize > MAX_ROW_BYTES) {
        Serial.println(F("[BMP] Image too wide for row buffer"));
        f.close();
        return;
    }

    f.seek(dataOffset);

    for (int32_t row = 0; row < bmpH; row++) {
        f.read(rowBuf, rowSize);

        int32_t drawRow = bottomUp ? (bmpH - 1 - row) : row;

        for (int32_t col = 0; col < bmpW; col++) {
            uint8_t b = rowBuf[col * 3 + 0];
            uint8_t g = rowBuf[col * 3 + 1];
            uint8_t r = rowBuf[col * 3 + 2];
            uint16_t c565 = ((r & 0xF8) << 8)
                          | ((g & 0xFC) << 3)
                          | (b >> 3);
            gfx->drawPixel(x + col, y + drawRow, c565);
        }
    }

    f.close();
    Serial.printf("[BMP] Loaded %s  (%dx%d)\n", path, bmpW, bmpH);
}

/* ================================================================
 *  END  OF  FILE
 * ================================================================ */
