/**
 * ESP32 Bus Stop Display — Smart Passenger Information System
 * 
 * Hardware: ESP32 (38-pin) + 3.5" TFT ILI9341/ILI9488 (SPI)
 * 
 * Features:
 * - Connects to WiFi and polls cloud API every 10 seconds
 * - Shows upcoming buses with ETA, distance, delay status
 * - Color-coded: green=on time, yellow=delayed, red=major delay
 * - Clock display synced via NTP
 * - Emergency alert area
 * - Auto-dim after inactivity
 * 
 * Libraries Required (install via Arduino Library Manager):
 * - TFT_eSPI (configure User_Setup.h for your display)
 * - ArduinoJson
 * - WiFi (built-in for ESP32)
 * - HTTPClient (built-in for ESP32)
 * - NTPClient
 * - WiFiUdp
 * 
 * Pin Connections (ILI9341 SPI):
 * TFT_MOSI  -> GPIO 23
 * TFT_SCLK  -> GPIO 18
 * TFT_CS    -> GPIO 15
 * TFT_DC    -> GPIO 2
 * TFT_RST   -> GPIO 4
 * TFT_BL    -> GPIO 21 (Backlight)
 * TOUCH_CS  -> GPIO 5 (if touch enabled)
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// ===== CONFIGURATION - EDIT THESE =====
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* API_BASE_URL  = "http://YOUR_SERVER_IP:5000/api";
const char* STOP_ID       = "YOUR_STOP_ID_FROM_DATABASE";
const char* STOP_NAME     = "Gandhipuram Bus Stand";

// Timing
const unsigned long POLL_INTERVAL   = 10000;  // 10 seconds
const unsigned long CLOCK_INTERVAL  = 1000;   // 1 second
const unsigned long DIM_TIMEOUT     = 120000; // 2 min inactivity dim
const int           MAX_BUSES       = 6;      // Max buses to show on screen
// ======================================

// Display
TFT_eSPI tft = TFT_eSPI();
#define BL_PIN 21

// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000); // IST = UTC+5:30

// Colors (RGB565)
#define C_BG        0x0841  // Dark navy
#define C_HEADER    0x1082  // Darker header
#define C_CARD      0x10A2  // Card background
#define C_TEXT      0xFFFF  // White
#define C_TEXT_DIM  0x7BEF  // Gray
#define C_BLUE      0x3B7F  // Accent blue
#define C_GREEN     0x07E0  // On time
#define C_YELLOW    0xFDE0  // Delayed
#define C_RED       0xF800  // Major delay
#define C_CYAN      0x07FF  // Cyan accent
#define C_ORANGE    0xFD20  // Orange

// State
unsigned long lastPoll = 0;
unsigned long lastClock = 0;
unsigned long lastActivity = 0;
bool isDimmed = false;
bool wifiConnected = false;

// Bus data structure
struct BusInfo {
  char busId[16];
  char name[32];
  char route[48];
  char type[12];
  int  eta;        // minutes
  float distance;  // km
  int  speed;      // km/h
  char status[12]; // on_time, delayed
};

BusInfo buses[MAX_BUSES];
int busCount = 0;
String alertMessage = "";

void setup() {
  Serial.begin(115200);
  Serial.println("\n🚌 Bus Stop Display Starting...");

  // Initialize backlight
  pinMode(BL_PIN, OUTPUT);
  setBrightness(255);

  // Initialize TFT
  tft.init();
  tft.setRotation(1); // Landscape
  tft.fillScreen(C_BG);
  
  // Show splash screen
  drawSplashScreen();
  
  // Connect WiFi
  connectWiFi();
  
  // Start NTP
  timeClient.begin();
  timeClient.update();
  
  // Initial data fetch
  fetchBusData();
  drawFullScreen();
  
  lastActivity = millis();
}

void loop() {
  unsigned long now = millis();
  
  // Update clock every second
  if (now - lastClock >= CLOCK_INTERVAL) {
    lastClock = now;
    timeClient.update();
    drawClock();
  }
  
  // Poll API for bus data
  if (now - lastPoll >= POLL_INTERVAL) {
    lastPoll = now;
    if (wifiConnected) {
      fetchBusData();
      drawBusList();
      drawAlertBar();
    } else {
      connectWiFi();
    }
  }
  
  // Auto-dim
  if (!isDimmed && (now - lastActivity > DIM_TIMEOUT)) {
    setBrightness(50);
    isDimmed = true;
  }
  
  // Touch wake (simple check)
  // If using XPT2046 touch, add touch detection here
  // touchWake();
  
  delay(50);
}

// ===== WiFi =====
void connectWiFi() {
  tft.setTextColor(C_TEXT, C_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 140);
  tft.print("Connecting to WiFi...");
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\n✅ WiFi Connected: " + WiFi.localIP().toString());
  } else {
    wifiConnected = false;
    Serial.println("\n❌ WiFi Failed");
  }
}

// ===== API Fetch =====
void fetchBusData() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    return;
  }
  
  HTTPClient http;
  String url = String(API_BASE_URL) + "/gps/esp32/" + STOP_ID;
  
  http.begin(url);
  http.setTimeout(5000);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    parseResponse(payload);
    Serial.printf("📡 Got %d buses\n", busCount);
  } else {
    Serial.printf("❌ HTTP Error: %d\n", httpCode);
  }
  
  http.end();
  
  // Also fetch alerts
  fetchAlerts();
}

void fetchAlerts() {
  HTTPClient http;
  String url = String(API_BASE_URL) + "/alerts/active";
  
  http.begin(url);
  http.setTimeout(3000);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error && doc.is<JsonArray>() && doc.size() > 0) {
      alertMessage = doc[0]["title"].as<String>();
    } else {
      alertMessage = "";
    }
  }
  
  http.end();
}

void parseResponse(String json) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
  
  if (error) {
    Serial.println("JSON parse error: " + String(error.c_str()));
    return;
  }
  
  busCount = 0;
  JsonArray busArray = doc["buses"].as<JsonArray>();
  
  for (JsonObject bus : busArray) {
    if (busCount >= MAX_BUSES) break;
    
    strlcpy(buses[busCount].busId, bus["busId"] | "???", sizeof(buses[busCount].busId));
    strlcpy(buses[busCount].name, bus["name"] | "Unknown", sizeof(buses[busCount].name));
    strlcpy(buses[busCount].route, bus["route"] | "N/A", sizeof(buses[busCount].route));
    strlcpy(buses[busCount].type, bus["type"] | "ord", sizeof(buses[busCount].type));
    buses[busCount].eta = bus["eta"] | 0;
    buses[busCount].distance = bus["distance"] | 0.0;
    buses[busCount].speed = bus["speed"] | 0;
    strlcpy(buses[busCount].status, bus["status"] | "on_time", sizeof(buses[busCount].status));
    
    busCount++;
  }
}

// ===== Drawing Functions =====

void drawSplashScreen() {
  tft.fillScreen(C_BG);
  tft.setTextColor(C_BLUE);
  tft.setTextSize(3);
  tft.setCursor(60, 80);
  tft.print("TN Bus Tracker");
  
  tft.setTextColor(C_TEXT_DIM);
  tft.setTextSize(1);
  tft.setCursor(100, 130);
  tft.print("Smart Bus Stop Display");
  
  tft.setCursor(80, 160);
  tft.print("Initializing system...");
  
  delay(1500);
}

void drawFullScreen() {
  tft.fillScreen(C_BG);
  drawHeader();
  drawBusList();
  drawAlertBar();
}

void drawHeader() {
  // Header background
  tft.fillRect(0, 0, tft.width(), 40, C_HEADER);
  
  // Stop name
  tft.setTextColor(C_TEXT, C_HEADER);
  tft.setTextSize(2);
  tft.setCursor(10, 12);
  tft.print(STOP_NAME);
  
  // WiFi indicator
  tft.setTextSize(1);
  tft.setCursor(tft.width() - 40, 5);
  tft.setTextColor(wifiConnected ? C_GREEN : C_RED, C_HEADER);
  tft.print(wifiConnected ? "WiFi" : "N/C");
  
  // Clock
  drawClock();
  
  // Divider with accent
  tft.drawFastHLine(0, 40, tft.width(), C_BLUE);
  tft.drawFastHLine(0, 41, tft.width(), C_BG);
  
  // Column headers
  tft.fillRect(0, 44, tft.width(), 18, C_CARD);
  tft.setTextColor(C_TEXT_DIM, C_CARD);
  tft.setTextSize(1);
  tft.setCursor(10, 48);   tft.print("BUS");
  tft.setCursor(110, 48);  tft.print("ROUTE");
  tft.setCursor(260, 48);  tft.print("ETA");
  tft.setCursor(320, 48);  tft.print("DIST");
  tft.setCursor(380, 48);  tft.print("STATUS");
  
  tft.drawFastHLine(0, 62, tft.width(), C_BLUE);
}

void drawClock() {
  String timeStr = timeClient.getFormattedTime().substring(0, 5); // HH:MM
  
  tft.setTextColor(C_CYAN, C_HEADER);
  tft.setTextSize(1);
  tft.setCursor(tft.width() - 40, 18);
  tft.print(timeStr);
}

void drawBusList() {
  // Clear bus area
  int startY = 64;
  int rowHeight = 36;
  tft.fillRect(0, startY, tft.width(), rowHeight * MAX_BUSES, C_BG);
  
  if (busCount == 0) {
    tft.setTextColor(C_TEXT_DIM, C_BG);
    tft.setTextSize(2);
    tft.setCursor(60, startY + 60);
    tft.print("No buses approaching");
    tft.setTextSize(1);
    tft.setCursor(80, startY + 90);
    tft.print("Waiting for live data...");
    return;
  }
  
  for (int i = 0; i < busCount && i < MAX_BUSES; i++) {
    int y = startY + (i * rowHeight);
    
    // Alternating row background
    if (i % 2 == 0) {
      tft.fillRect(0, y, tft.width(), rowHeight - 2, C_CARD);
    }
    
    uint16_t bgColor = (i % 2 == 0) ? C_CARD : C_BG;
    
    // Status color indicator (left bar)
    uint16_t statusColor = C_GREEN;
    if (strcmp(buses[i].status, "delayed") == 0) statusColor = C_YELLOW;
    if (buses[i].eta > 30) statusColor = C_ORANGE;
    tft.fillRect(0, y, 3, rowHeight - 2, statusColor);
    
    // Bus name
    tft.setTextColor(C_TEXT, bgColor);
    tft.setTextSize(1);
    tft.setCursor(10, y + 5);
    tft.print(buses[i].name);
    
    // Bus type badge
    tft.setTextSize(1);
    tft.setCursor(10, y + 20);
    tft.setTextColor(C_CYAN, bgColor);
    tft.print(buses[i].type);
    
    // Route
    tft.setTextColor(C_TEXT_DIM, bgColor);
    tft.setCursor(110, y + 10);
    // Truncate route name if too long
    char routeShort[24];
    strlcpy(routeShort, buses[i].route, sizeof(routeShort));
    tft.print(routeShort);
    
    // ETA (large, colored)
    tft.setTextColor(statusColor, bgColor);
    tft.setTextSize(2);
    tft.setCursor(255, y + 6);
    tft.printf("%dm", buses[i].eta);
    
    // Distance
    tft.setTextColor(C_TEXT_DIM, bgColor);
    tft.setTextSize(1);
    tft.setCursor(320, y + 10);
    tft.printf("%.1fkm", buses[i].distance);
    
    // Status text
    tft.setTextColor(statusColor, bgColor);
    tft.setCursor(380, y + 5);
    if (strcmp(buses[i].status, "on_time") == 0) {
      tft.print("On Time");
    } else {
      tft.print("Delayed");
    }
    
    // Speed
    tft.setTextColor(C_TEXT_DIM, bgColor);
    tft.setCursor(380, y + 18);
    tft.printf("%dkm/h", buses[i].speed);
  }
}

void drawAlertBar() {
  int y = tft.height() - 25;
  tft.fillRect(0, y, tft.width(), 25, C_HEADER);
  
  if (alertMessage.length() > 0) {
    // Emergency/alert banner
    tft.fillRect(0, y, tft.width(), 25, 0x4000); // Dark red bg
    tft.setTextColor(C_RED, 0x4000);
    tft.setTextSize(1);
    tft.setCursor(10, y + 7);
    tft.print("! " + alertMessage);
  } else {
    // Normal footer
    tft.setTextColor(C_TEXT_DIM, C_HEADER);
    tft.setTextSize(1);
    tft.setCursor(10, y + 7);
    tft.printf("Buses: %d | Auto-refresh: %ds", busCount, POLL_INTERVAL / 1000);
    
    tft.setCursor(tft.width() - 120, y + 7);
    tft.print("TN Bus Tracker v1.0");
  }
}

void setBrightness(int level) {
  analogWrite(BL_PIN, level);
}
