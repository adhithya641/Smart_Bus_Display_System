/**
 * screens.h — LVGL screen builder declarations
 * Smart Bus Stop Display v2.0
 *
 * Pages:
 *   Home      → live arrivals dashboard
 *   Schedule  → timetable list
 *   Search    → route/bus keyboard search
 *   Alerts    → full alert list
 *   Settings  → WiFi status, IP, firmware info
 */

#ifndef SCREENS_H
#define SCREENS_H

#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <NTPClient.h>
#include "config.h"
#include "ui.h"
#include "api.h"

// Build all screens and register tab navigation
void screens_init(NTPClient* ntpClient);

// Draw splash using raw GFX (before LVGL loads)
void screens_showSplash(Arduino_GFX* gfx);

// Incremental refresh functions (called from loop)
void screens_updateHome();
void screens_updateClock(NTPClient* ntpClient);
void screens_updateTicker();
void screens_updateSchedule();
void screens_showSearchResults();
void screens_setApiStatus(bool online);

#endif // SCREENS_H
