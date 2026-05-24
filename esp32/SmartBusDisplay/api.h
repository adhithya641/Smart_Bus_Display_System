/**
 * ============================================================
 *  api.h  —  HTTP/REST Client for TN Bus Tracking Backend
 *  Smart Bus Stop Display v2.0
 * ============================================================
 *  Handles all API calls:
 *   GET /api/gps/esp32/:stopId        → live bus arrivals
 *   GET /api/alerts/active            → ticker alerts
 *   GET /api/schedules?active=true    → timetable
 *   GET /api/routes/search?from=...   → route search
 *   GET /api/stops                    → stop list
 *   GET /api/health                   → connectivity check
 */

#ifndef API_H
#define API_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ─── Data Structures ────────────────────────────────────────

struct BusArrival {
    char busId[16];
    char name[32];
    char route[48];
    char type[12];        // ordinary / express / deluxe / ac
    int  etaMinutes;
    float distanceKm;
    int  speedKmh;
    bool isDelayed;
    char confidence[12];  // high / medium / low
};

struct AlertInfo {
    char title[120];
    char message[256];
    char severity[12];    // info / warning / critical
    char type[16];        // delay / breakdown / diversion / emergency
};

struct ScheduleEntry {
    char routeName[48];
    char busName[32];
    char busType[12];
    char departure[8];    // "HH:MM"
    char arrival[8];      // "HH:MM"
    bool isActive;
};

struct RouteResult {
    char routeId[28];
    char routeNumber[16];
    char name[64];
    char origin[40];
    char destination[40];
    int  activeBusCount;
};

struct StopDisplayData {
    char stopId[28];
    char name[64];
    char district[32];
    BusArrival buses[MAX_BUSES];
    int  busCount;
    char timestamp[30];
};

struct AlertsData {
    AlertInfo alerts[MAX_ALERTS];
    int count;
};

struct ScheduleData {
    ScheduleEntry entries[20];
    int count;
};

struct RouteSearchData {
    RouteResult routes[MAX_ROUTES];
    int count;
};

// ─── Global result buffers ────────────────────────────────────
extern StopDisplayData  g_stopData;
extern AlertsData       g_alertsData;
extern ScheduleData     g_scheduleData;
extern RouteSearchData  g_routeSearch;
extern bool             g_apiOnline;

// ─── Function Declarations ────────────────────────────────────
bool api_fetchLiveBuses(const char* stopId);
bool api_fetchAlerts();
bool api_fetchSchedules();
bool api_searchRoutes(const char* query);
bool api_healthCheck();
String api_get(const char* endpoint, int timeoutMs = 5000);

#endif // API_H
