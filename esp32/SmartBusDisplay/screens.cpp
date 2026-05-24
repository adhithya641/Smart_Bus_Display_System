/**
 * screens.cpp — LVGL UI implementation
 * Smart Bus Stop Display v2.0
 *
 * Layout (480x320):
 *  [0..47]   Header bar  — stop name, clock, WiFi dot
 *  [48..269] Content     — tab pages (Home/Schedule/Search/Alerts/Settings)
 *  [270..287] Tab bar    — 5 icon+label tabs
 *  [288..319] Ticker     — scrolling alert/news bar
 */

#include "screens.h"
#include <Arduino.h>

// ── Global LVGL objects ──────────────────────────────────────
static lv_obj_t *scr;
static lv_obj_t *lbl_stopName, *lbl_clock, *lbl_wifi;
static lv_obj_t *tabview;
static lv_obj_t *tab_home, *tab_sched, *tab_search, *tab_alerts, *tab_settings;
static lv_obj_t *cont_buses;          // home: bus rows container
static lv_obj_t *lbl_noBus;          // home: "no buses" label
static lv_obj_t *cont_sched;         // schedule list
static lv_obj_t *ta_search;          // search text area
static lv_obj_t *kb_search;          // on-screen keyboard
static lv_obj_t *cont_results;       // search results
static lv_obj_t *cont_alerts;        // alerts list
static lv_obj_t *lbl_ticker;         // bottom ticker label
static lv_obj_t *lbl_apiDot;         // header API status dot
static lv_timer_t *ticker_timer;
static int ticker_x = SCREEN_WIDTH;
static char ticker_text[512] = "Welcome to Smart Bus Stop — Live tracking active";

static NTPClient* _ntp = nullptr;

// ── Helper: create a styled card container ───────────────────
static lv_obj_t* make_card(lv_obj_t* parent, int y, int h) {
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_set_size(c, SCREEN_WIDTH - 8, h);
    lv_obj_set_pos(c, 4, y);
    lv_obj_set_style_bg_color(c, CLR_BG_PANEL, 0);
    lv_obj_set_style_border_color(c, CLR_SEPARATOR, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_radius(c, CARD_RADIUS, 0);
    lv_obj_set_style_pad_all(c, CARD_PAD, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

// ── Header ───────────────────────────────────────────────────
static void build_header() {
    lv_obj_t* hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, CLR_BG_HEADER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    // Stop name
    lbl_stopName = lv_label_create(hdr);
    lv_label_set_text(lbl_stopName, STOP_NAME);
    lv_obj_set_style_text_color(lbl_stopName, CLR_ACCENT_CYAN, 0);
    lv_obj_align(lbl_stopName, LV_ALIGN_LEFT_MID, 8, 0);

    // API online dot
    lbl_apiDot = lv_label_create(hdr);
    lv_label_set_text(lbl_apiDot, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(lbl_apiDot, CLR_ETA_GREEN, 0);
    lv_obj_align(lbl_apiDot, LV_ALIGN_RIGHT_MID, -48, 0);

    // Clock
    lbl_clock = lv_label_create(hdr);
    lv_label_set_text(lbl_clock, "--:--");
    lv_obj_set_style_text_color(lbl_clock, CLR_WHITE, 0);
    lv_obj_align(lbl_clock, LV_ALIGN_RIGHT_MID, -8, 0);

    // Blue accent line
    lv_obj_t* line = lv_obj_create(scr);
    lv_obj_set_size(line, SCREEN_WIDTH, 2);
    lv_obj_set_pos(line, 0, HEADER_HEIGHT);
    lv_obj_set_style_bg_color(line, CLR_ACCENT_BLUE, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);
}

// ── Ticker (bottom scroll bar) ────────────────────────────────
static void ticker_cb(lv_timer_t* t) {
    ticker_x -= 2;
    if (ticker_x < -(int)(strlen(ticker_text) * 8))
        ticker_x = SCREEN_WIDTH;
    lv_obj_set_x(lbl_ticker, ticker_x);
}

static void build_ticker() {
    lv_obj_t* bar = lv_obj_create(scr);
    lv_obj_set_size(bar, SCREEN_WIDTH, TICKER_HEIGHT);
    lv_obj_set_pos(bar, 0, SCREEN_HEIGHT - TICKER_HEIGHT);
    lv_obj_set_style_bg_color(bar, CLR_BG_HEADER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_clip_corner(bar, true, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lbl_ticker = lv_label_create(bar);
    lv_label_set_text(lbl_ticker, ticker_text);
    lv_obj_set_style_text_color(lbl_ticker, CLR_ACCENT_CYAN, 0);
    lv_obj_set_y(lbl_ticker, 8);
    lv_obj_set_x(lbl_ticker, ticker_x);

    ticker_timer = lv_timer_create(ticker_cb, SCROLL_SPEED_MS, nullptr);
}

// ── Home Tab — Bus Arrival Rows ───────────────────────────────
static void build_home_tab() {
    // Column header row
    lv_obj_t* hrow = lv_obj_create(tab_home);
    lv_obj_set_size(hrow, SCREEN_WIDTH - 16, 20);
    lv_obj_set_pos(hrow, 0, 0);
    lv_obj_set_style_bg_color(hrow, CLR_BG_HEADER, 0);
    lv_obj_set_style_border_width(hrow, 0, 0);
    lv_obj_set_style_radius(hrow, 0, 0);
    lv_obj_clear_flag(hrow, LV_OBJ_FLAG_SCROLLABLE);

    const char* cols[] = {"BUS / ROUTE", "TYPE", "ETA", "DIST", "STATUS"};
    int cx[] = {4, 160, 230, 300, 370};
    for (int i = 0; i < 5; i++) {
        lv_obj_t* lh = lv_label_create(hrow);
        lv_label_set_text(lh, cols[i]);
        lv_obj_set_style_text_color(lh, CLR_GRAY_DIM, 0);
        lv_obj_set_pos(lh, cx[i], 2);
    }

    // Scrollable bus rows container
    cont_buses = lv_obj_create(tab_home);
    lv_obj_set_size(cont_buses, SCREEN_WIDTH - 16, BUS_ROW_HEIGHT * MAX_BUSES);
    lv_obj_set_pos(cont_buses, 0, 22);
    lv_obj_set_style_bg_opa(cont_buses, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_buses, 0, 0);
    lv_obj_set_layout(cont_buses, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_buses, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont_buses, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(cont_buses, 2, 0);

    lbl_noBus = lv_label_create(tab_home);
    lv_label_set_text(lbl_noBus, "No buses approaching — waiting for live data...");
    lv_obj_set_style_text_color(lbl_noBus, CLR_GRAY_DIM, 0);
    lv_obj_align(lbl_noBus, LV_ALIGN_CENTER, 0, 20);
    lv_obj_add_flag(lbl_noBus, LV_OBJ_FLAG_HIDDEN);
}

// ── Schedule Tab ──────────────────────────────────────────────
static void build_schedule_tab() {
    cont_sched = lv_obj_create(tab_sched);
    lv_obj_set_size(cont_sched, SCREEN_WIDTH - 16, 200);
    lv_obj_set_pos(cont_sched, 0, 0);
    lv_obj_set_style_bg_opa(cont_sched, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_sched, 0, 0);
    lv_obj_set_layout(cont_sched, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_sched, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cont_sched, 3, 0);
}

// ── Search Tab ────────────────────────────────────────────────
static void search_event_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_READY) {
        const char* q = lv_textarea_get_text(ta_search);
        if (strlen(q) >= 2) {
            api_searchRoutes(q);
            screens_showSearchResults();
        }
    }
}

static void build_search_tab() {
    lv_obj_t* lbl = lv_label_create(tab_search);
    lv_label_set_text(lbl, "Search bus number or route:");
    lv_obj_set_style_text_color(lbl, CLR_GRAY_LIGHT, 0);
    lv_obj_set_pos(lbl, 4, 4);

    ta_search = lv_textarea_create(tab_search);
    lv_textarea_set_one_line(ta_search, true);
    lv_textarea_set_placeholder_text(ta_search, "e.g. 47B or Gandhipuram");
    lv_obj_set_size(ta_search, SCREEN_WIDTH - 24, 36);
    lv_obj_set_pos(ta_search, 4, 24);
    lv_obj_set_style_bg_color(ta_search, CLR_BG_PANEL, 0);
    lv_obj_set_style_text_color(ta_search, CLR_WHITE, 0);
    lv_obj_add_event_cb(ta_search, search_event_cb, LV_EVENT_READY, nullptr);

    kb_search = lv_keyboard_create(tab_search);
    lv_obj_set_size(kb_search, SCREEN_WIDTH - 16, 100);
    lv_obj_set_pos(kb_search, 0, 64);
    lv_keyboard_set_textarea(kb_search, ta_search);
    lv_obj_set_style_bg_color(kb_search, CLR_BG_PANEL, 0);

    cont_results = lv_obj_create(tab_search);
    lv_obj_set_size(cont_results, SCREEN_WIDTH - 16, 60);
    lv_obj_set_pos(cont_results, 4, 168);
    lv_obj_set_style_bg_opa(cont_results, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_results, 0, 0);
    lv_obj_set_layout(cont_results, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_results, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cont_results, 2, 0);
}

// ── Alerts Tab ────────────────────────────────────────────────
static void build_alerts_tab() {
    lv_obj_t* title = lv_label_create(tab_alerts);
    lv_label_set_text(title, "System Alerts");
    lv_obj_set_style_text_color(title, CLR_ACCENT_CYAN, 0);
    lv_obj_set_pos(title, 4, 4);

    cont_alerts = lv_obj_create(tab_alerts);
    lv_obj_set_size(cont_alerts, SCREEN_WIDTH - 16, 190);
    lv_obj_set_pos(cont_alerts, 0, 24);
    lv_obj_set_style_bg_opa(cont_alerts, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_alerts, 0, 0);
    lv_obj_set_layout(cont_alerts, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_alerts, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cont_alerts, 4, 0);
}

// ── Settings Tab ─────────────────────────────────────────────
static void build_settings_tab() {
    char buf[256];
    snprintf(buf, sizeof(buf),
        "Stop:      %s\nDistrict:  %s\nServer:    %s\nFirmware:  v%s (%s)\nDisplay:   480x320 ILI9488\nRefresh:   %ds live / %ds alerts",
        STOP_NAME, STOP_DISTRICT, API_BASE_URL, FW_VERSION, FW_DATE,
        POLL_INTERVAL_MS/1000, ALERT_INTERVAL_MS/1000);

    lv_obj_t* info = lv_label_create(tab_settings);
    lv_label_set_text(info, buf);
    lv_obj_set_style_text_color(info, CLR_GRAY_LIGHT, 0);
    lv_obj_set_pos(info, 8, 8);

    lv_obj_t* ip_lbl = lv_label_create(tab_settings);
    String ip = "IP: " + WiFi.localIP().toString() + "   RSSI: " + String(WiFi.RSSI()) + " dBm";
    lv_label_set_text(ip_lbl, ip.c_str());
    lv_obj_set_style_text_color(ip_lbl, CLR_ACCENT_CYAN, 0);
    lv_obj_set_pos(ip_lbl, 8, 150);
}

// ── screens_init ──────────────────────────────────────────────
void screens_init(NTPClient* ntpClient) {
    _ntp = ntpClient;
    scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, CLR_BG_DARK, 0);

    build_header();

    // Tabview (content area between header and ticker)
    int tv_y = HEADER_HEIGHT + 2;
    int tv_h = SCREEN_HEIGHT - HEADER_HEIGHT - TICKER_HEIGHT - 2;
    tabview = lv_tabview_create(scr, LV_DIR_BOTTOM, TABBAR_HEIGHT);
    lv_obj_set_size(tabview, SCREEN_WIDTH, tv_h);
    lv_obj_set_pos(tabview, 0, tv_y);
    lv_obj_set_style_bg_color(tabview, CLR_BG_DARK, 0);

    // Tab button bar style
    lv_obj_t* tbar = lv_tabview_get_tab_btns(tabview);
    lv_obj_set_style_bg_color(tbar, CLR_BG_HEADER, 0);
    lv_obj_set_style_text_color(tbar, CLR_GRAY_LIGHT, 0);
    lv_obj_set_style_text_color(tbar, CLR_ACCENT_CYAN, LV_STATE_CHECKED);
    lv_obj_set_style_border_side(tbar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(tbar, CLR_ACCENT_BLUE, 0);

    tab_home     = lv_tabview_add_tab(tabview, LV_SYMBOL_HOME   " Home");
    tab_sched    = lv_tabview_add_tab(tabview, LV_SYMBOL_LIST   " Times");
    tab_search   = lv_tabview_add_tab(tabview, LV_SYMBOL_SEARCH " Search");
    tab_alerts   = lv_tabview_add_tab(tabview, LV_SYMBOL_WARNING " Alerts");
    tab_settings = lv_tabview_add_tab(tabview, LV_SYMBOL_SETTINGS " Info");

    for (lv_obj_t* t : {tab_home, tab_sched, tab_search, tab_alerts, tab_settings}) {
        lv_obj_set_style_bg_color(t, CLR_BG_DARK, 0);
        lv_obj_set_style_pad_all(t, 4, 0);
    }

    build_home_tab();
    build_schedule_tab();
    build_search_tab();
    build_alerts_tab();
    build_settings_tab();
    build_ticker();
}

// ── screens_showSplash ────────────────────────────────────────
void screens_showSplash(Arduino_GFX* gfx) {
    gfx->fillScreen(0x050D); // near-black navy
    gfx->setTextSize(3);
    gfx->setTextColor(0x07FF); // cyan
    gfx->setCursor(60, 80);
    gfx->print("TN Bus Tracker");
    gfx->setTextSize(1);
    gfx->setTextColor(0xB5B6);
    gfx->setCursor(90, 130);
    gfx->print("Smart Passenger Information Display");
    gfx->setCursor(140, 155);
    gfx->print("v" FW_VERSION " — " STOP_NAME);
    gfx->setTextColor(0x07E0);
    gfx->setCursor(170, 190);
    gfx->print("Initializing...");
}

// ── screens_updateHome ────────────────────────────────────────
void screens_updateHome() {
    lv_obj_clean(cont_buses);

    if (g_stopData.busCount == 0) {
        lv_obj_clear_flag(lbl_noBus, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_add_flag(lbl_noBus, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < g_stopData.busCount; i++) {
        BusArrival& b = g_stopData.buses[i];
        lv_color_t ec = eta_color(b.etaMinutes, b.isDelayed);
        lv_color_t tc = type_badge_color(b.type);
        lv_color_t rowbg = (i % 2 == 0) ? CLR_BG_PANEL : CLR_BG_DARK;

        lv_obj_t* row = lv_obj_create(cont_buses);
        lv_obj_set_size(row, SCREEN_WIDTH - 24, BUS_ROW_HEIGHT);
        lv_obj_set_style_bg_color(row, rowbg, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_set_style_pad_all(row, 4, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        // Left ETA color bar (3 px)
        lv_obj_t* bar = lv_obj_create(row);
        lv_obj_set_size(bar, 3, BUS_ROW_HEIGHT - 8);
        lv_obj_set_pos(bar, 0, 0);
        lv_obj_set_style_bg_color(bar, ec, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_radius(bar, 2, 0);

        // Bus name + route
        char nameRoute[64];
        snprintf(nameRoute, sizeof(nameRoute), "%s\n%s", b.name, b.route);
        lv_obj_t* ln = lv_label_create(row);
        lv_label_set_text(ln, nameRoute);
        lv_obj_set_style_text_color(ln, CLR_WHITE, 0);
        lv_obj_set_pos(ln, 8, 2);
        lv_label_set_long_mode(ln, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(ln, 148);

        // Type badge
        lv_obj_t* badge = lv_obj_create(row);
        lv_obj_set_size(badge, 52, 18);
        lv_obj_set_pos(badge, 162, 10);
        lv_obj_set_style_bg_color(badge, tc, 0);
        lv_obj_set_style_border_width(badge, 0, 0);
        lv_obj_set_style_radius(badge, 4, 0);
        lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* bt = lv_label_create(badge);
        lv_label_set_text(bt, b.type);
        lv_obj_set_style_text_color(bt, CLR_WHITE, 0);
        lv_obj_align(bt, LV_ALIGN_CENTER, 0, 0);

        // ETA (large)
        char eta_s[12];
        snprintf(eta_s, sizeof(eta_s), "%d min", b.etaMinutes);
        lv_obj_t* le = lv_label_create(row);
        lv_label_set_text(le, eta_s);
        lv_obj_set_style_text_color(le, ec, 0);
        lv_obj_set_pos(le, 222, 10);

        // Distance
        char dist_s[12];
        snprintf(dist_s, sizeof(dist_s), "%.1f km", b.distanceKm);
        lv_obj_t* ld = lv_label_create(row);
        lv_label_set_text(ld, dist_s);
        lv_obj_set_style_text_color(ld, CLR_GRAY_LIGHT, 0);
        lv_obj_set_pos(ld, 298, 10);

        // Status
        lv_obj_t* ls = lv_label_create(row);
        lv_label_set_text(ls, b.isDelayed ? "Delayed" : "On Time");
        lv_obj_set_style_text_color(ls, ec, 0);
        lv_obj_set_pos(ls, 370, 4);

        // Speed
        char sp_s[12];
        snprintf(sp_s, sizeof(sp_s), "%d km/h", b.speedKmh);
        lv_obj_t* lsp = lv_label_create(row);
        lv_label_set_text(lsp, sp_s);
        lv_obj_set_style_text_color(lsp, CLR_GRAY_DIM, 0);
        lv_obj_set_pos(lsp, 370, 20);
    }
}

// ── screens_updateClock ───────────────────────────────────────
void screens_updateClock(NTPClient* ntpClient) {
    if (!_ntp) return;
    String t = ntpClient->getFormattedTime().substring(0, 5);
    lv_label_set_text(lbl_clock, t.c_str());
}

// ── screens_updateTicker ──────────────────────────────────────
void screens_updateTicker() {
    if (g_alertsData.count == 0) {
        snprintf(ticker_text, sizeof(ticker_text),
            "  TN Bus Tracker — Live tracking active  |  Stop: %s  |  Data refreshes every %ds  ",
            STOP_NAME, POLL_INTERVAL_MS/1000);
    } else {
        String s = "  ";
        for (int i = 0; i < g_alertsData.count; i++) {
            s += String("⚠ ") + g_alertsData.alerts[i].title + "  |  ";
        }
        strlcpy(ticker_text, s.c_str(), sizeof(ticker_text));
    }
    lv_label_set_text(lbl_ticker, ticker_text);
    ticker_x = SCREEN_WIDTH;
}

// ── screens_updateSchedule ────────────────────────────────────
void screens_updateSchedule() {
    lv_obj_clean(cont_sched);
    if (g_scheduleData.count == 0) {
        lv_obj_t* lbl = lv_label_create(cont_sched);
        lv_label_set_text(lbl, "No schedule data available.");
        lv_obj_set_style_text_color(lbl, CLR_GRAY_DIM, 0);
        return;
    }
    for (int i = 0; i < g_scheduleData.count; i++) {
        ScheduleEntry& e = g_scheduleData.entries[i];
        char row[96];
        snprintf(row, sizeof(row), "%s  %s → %s  [%s]",
            e.departure, e.routeName, e.busName, e.busType);
        lv_obj_t* lbl = lv_label_create(cont_sched);
        lv_label_set_text(lbl, row);
        lv_obj_set_style_text_color(lbl,
            i % 2 == 0 ? CLR_WHITE : CLR_GRAY_LIGHT, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(lbl, SCREEN_WIDTH - 24);
    }
}

// ── screens_showSearchResults ─────────────────────────────────
void screens_showSearchResults() {
    lv_obj_clean(cont_results);
    if (g_routeSearch.count == 0) {
        lv_obj_t* lbl = lv_label_create(cont_results);
        lv_label_set_text(lbl, "No routes found.");
        lv_obj_set_style_text_color(lbl, CLR_GRAY_DIM, 0);
        return;
    }
    for (int i = 0; i < g_routeSearch.count; i++) {
        RouteResult& r = g_routeSearch.routes[i];
        char row[128];
        snprintf(row, sizeof(row), "[%s] %s  %s → %s  (%d buses)",
            r.routeNumber, r.name, r.origin, r.destination, r.activeBusCount);
        lv_obj_t* lbl = lv_label_create(cont_results);
        lv_label_set_text(lbl, row);
        lv_obj_set_style_text_color(lbl, CLR_ACCENT_CYAN, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lbl, SCREEN_WIDTH - 24);
    }
}

// ── screens_setApiStatus ──────────────────────────────────────
void screens_setApiStatus(bool online) {
    lv_obj_set_style_text_color(lbl_apiDot,
        online ? CLR_ETA_GREEN : CLR_ETA_RED, 0);
}
