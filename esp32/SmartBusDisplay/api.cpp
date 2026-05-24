/**
 * ============================================================
 *  api.cpp  —  REST API Implementation
 *  Smart Bus Stop Display v2.0
 * ============================================================
 */

#include "api.h"

// ─── Global Data Buffers ─────────────────────────────────────
StopDisplayData  g_stopData    = {};
AlertsData       g_alertsData  = {};
ScheduleData     g_scheduleData = {};
RouteSearchData  g_routeSearch  = {};
bool             g_apiOnline   = false;

// ─── Low-level HTTP GET ───────────────────────────────────────
String api_get(const char* endpoint, int timeoutMs) {
    if (WiFi.status() != WL_CONNECTED) return "";

    HTTPClient http;
    String url = String(API_BASE_URL) + endpoint;

    http.begin(url);
    http.setTimeout(timeoutMs);
    http.addHeader("Accept", "application/json");
    http.addHeader("User-Agent", "ESP32-BusDisplay/2.0");

    int code = http.GET();
    String body = "";

    if (code == 200) {
        body = http.getString();
        g_apiOnline = true;
    } else {
        Serial.printf("[API] %s → HTTP %d\n", endpoint, code);
        if (code < 0) g_apiOnline = false;
    }

    http.end();
    return body;
}

// ─── Health Check ─────────────────────────────────────────────
bool api_healthCheck() {
    String resp = api_get("/health", 3000);
    if (resp.length() == 0) { g_apiOnline = false; return false; }

    JsonDocument doc;
    if (deserializeJson(doc, resp) == DeserializationError::Ok) {
        g_apiOnline = (strcmp(doc["status"] | "", "ok") == 0);
    }
    return g_apiOnline;
}

// ─── Fetch Live Bus Arrivals ──────────────────────────────────
//  GET /api/gps/esp32/:stopId
//  Response: { stop:{id,name,district}, busCount, buses:[...] }
bool api_fetchLiveBuses(const char* stopId) {
    String endpoint = String("/gps/esp32/") + stopId;
    String resp = api_get(endpoint.c_str());
    if (resp.length() == 0) return false;

    // Allocate on heap for large JSON
    JsonDocument* doc = new JsonDocument();
    DeserializationError err = deserializeJson(*doc, resp);
    if (err) {
        Serial.printf("[API] JSON parse error: %s\n", err.c_str());
        delete doc;
        return false;
    }

    // Stop info
    strlcpy(g_stopData.stopId,   (*doc)["stop"]["id"]       | stopId,      sizeof(g_stopData.stopId));
    strlcpy(g_stopData.name,     (*doc)["stop"]["name"]      | STOP_NAME,   sizeof(g_stopData.name));
    strlcpy(g_stopData.district, (*doc)["stop"]["district"]  | STOP_DISTRICT, sizeof(g_stopData.district));
    strlcpy(g_stopData.timestamp, (*doc)["timestamp"]        | "",           sizeof(g_stopData.timestamp));

    JsonArray arr = (*doc)["buses"].as<JsonArray>();
    g_stopData.busCount = 0;

    for (JsonObject bus : arr) {
        if (g_stopData.busCount >= MAX_BUSES) break;
        int i = g_stopData.busCount;

        strlcpy(g_stopData.buses[i].busId,      bus["busId"]     | "?",        16);
        strlcpy(g_stopData.buses[i].name,       bus["name"]      | "Unknown",  32);
        strlcpy(g_stopData.buses[i].route,      bus["route"]     | "N/A",      48);
        strlcpy(g_stopData.buses[i].type,       bus["type"]      | "ordinary", 12);
        strlcpy(g_stopData.buses[i].confidence, bus["confidence"]| "medium",   12);

        g_stopData.buses[i].etaMinutes  = bus["eta"]      | 0;
        g_stopData.buses[i].distanceKm  = bus["distance"] | 0.0f;
        g_stopData.buses[i].speedKmh    = bus["speed"]    | 0;
        g_stopData.buses[i].isDelayed   = (strcmp(bus["status"] | "on_time", "delayed") == 0);

        g_stopData.busCount++;
    }

    delete doc;
    Serial.printf("[API] Live: %d buses at %s\n", g_stopData.busCount, g_stopData.name);
    return true;
}

// ─── Fetch Active Alerts ──────────────────────────────────────
//  GET /api/alerts/active
//  Response: [ { type, title, message, severity, ... } ]
bool api_fetchAlerts() {
    String resp = api_get("/alerts/active", 4000);
    if (resp.length() == 0) { g_alertsData.count = 0; return false; }

    JsonDocument doc;
    if (deserializeJson(doc, resp) != DeserializationError::Ok) return false;

    g_alertsData.count = 0;
    JsonArray arr = doc.as<JsonArray>();

    for (JsonObject a : arr) {
        if (g_alertsData.count >= MAX_ALERTS) break;
        int i = g_alertsData.count;

        strlcpy(g_alertsData.alerts[i].title,    a["title"]    | "Alert",  120);
        strlcpy(g_alertsData.alerts[i].message,  a["message"]  | "",       256);
        strlcpy(g_alertsData.alerts[i].severity, a["severity"] | "info",   12);
        strlcpy(g_alertsData.alerts[i].type,     a["type"]     | "info",   16);

        g_alertsData.count++;
    }

    return true;
}

// ─── Fetch Today's Schedules ──────────────────────────────────
//  GET /api/schedules?active=true
//  Response: [ { route:{routeNumber,name}, bus:{busId,name,type},
//               departureTime, arrivalTime } ]
bool api_fetchSchedules() {
    String resp = api_get("/schedules?active=true", 6000);
    if (resp.length() == 0) return false;

    JsonDocument* doc = new JsonDocument();
    if (deserializeJson(*doc, resp) != DeserializationError::Ok) {
        delete doc; return false;
    }

    g_scheduleData.count = 0;
    JsonArray arr = doc->as<JsonArray>();

    for (JsonObject s : arr) {
        if (g_scheduleData.count >= 20) break;
        int i = g_scheduleData.count;

        // Nested objects
        String rName = String(s["route"]["name"] | "");
        String rNum  = String(s["route"]["routeNumber"] | "");
        String combined = rNum.length() > 0 ? rNum + " - " + rName : rName;
        strlcpy(g_scheduleData.entries[i].routeName, combined.c_str(), 48);

        strlcpy(g_scheduleData.entries[i].busName,  s["bus"]["name"]  | "?", 32);
        strlcpy(g_scheduleData.entries[i].busType,  s["bus"]["type"]  | "ordinary", 12);
        strlcpy(g_scheduleData.entries[i].departure, s["departureTime"] | "--:--", 8);
        strlcpy(g_scheduleData.entries[i].arrival,   s["arrivalTime"]  | "--:--", 8);
        g_scheduleData.entries[i].isActive = s["isActive"] | true;

        g_scheduleData.count++;
    }

    delete doc;
    Serial.printf("[API] Schedules: %d entries\n", g_scheduleData.count);
    return true;
}

// ─── Search Routes ────────────────────────────────────────────
//  GET /api/routes/search?from=<query>
//  Response: [ { routeNumber, name, origin, destination,
//                activeBuses:[...] } ]
bool api_searchRoutes(const char* query) {
    // URL-encode basic (replace space with %20)
    String q = String(query);
    q.replace(" ", "%20");

    String endpoint = String("/routes/search?from=") + q;
    String resp = api_get(endpoint.c_str(), 6000);
    if (resp.length() == 0) { g_routeSearch.count = 0; return false; }

    JsonDocument* doc = new JsonDocument();
    if (deserializeJson(*doc, resp) != DeserializationError::Ok) {
        delete doc; g_routeSearch.count = 0; return false;
    }

    g_routeSearch.count = 0;
    JsonArray arr = doc->as<JsonArray>();

    for (JsonObject r : arr) {
        if (g_routeSearch.count >= MAX_ROUTES) break;
        int i = g_routeSearch.count;

        strlcpy(g_routeSearch.routes[i].routeId,     r["_id"]         | "", 28);
        strlcpy(g_routeSearch.routes[i].routeNumber,  r["routeNumber"] | "", 16);
        strlcpy(g_routeSearch.routes[i].name,         r["name"]        | "", 64);
        strlcpy(g_routeSearch.routes[i].origin,       r["origin"]      | "", 40);
        strlcpy(g_routeSearch.routes[i].destination,  r["destination"] | "", 40);

        JsonArray buses = r["activeBuses"].as<JsonArray>();
        g_routeSearch.routes[i].activeBusCount = buses.size();

        g_routeSearch.count++;
    }

    delete doc;
    return (g_routeSearch.count > 0);
}
