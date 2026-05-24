/**
 * ============================================================
 *  ui.h  —  Color Palette, LVGL Style & Layout Constants
 *  Smart Bus Stop Display v2.0
 * ============================================================
 */

#ifndef UI_H
#define UI_H

#include <lvgl.h>

// ─── Color Palette (ARGB32 for LVGL) ────────────────────────
#define CLR_BG_DARK      lv_color_hex(0x050D1A)  // Deep space navy
#define CLR_BG_PANEL     lv_color_hex(0x0A1628)  // Panel card
#define CLR_BG_HEADER    lv_color_hex(0x0D1F35)  // Header bar
#define CLR_ACCENT_BLUE  lv_color_hex(0x00A8FF)  // Primary accent
#define CLR_ACCENT_CYAN  lv_color_hex(0x00D4FF)  // Secondary accent
#define CLR_ACCENT_TEAL  lv_color_hex(0x00C9A7)  // Teal highlight

#define CLR_ETA_GREEN    lv_color_hex(0x00E676)  // < 5 min arriving
#define CLR_ETA_YELLOW   lv_color_hex(0xFFD600)  // 5–20 min moderate
#define CLR_ETA_ORANGE   lv_color_hex(0xFF6D00)  // 20–40 min waiting
#define CLR_ETA_RED      lv_color_hex(0xFF1744)  // > 40 min / delayed

#define CLR_WHITE        lv_color_hex(0xFFFFFF)
#define CLR_GRAY_LIGHT   lv_color_hex(0xB0BEC5)
#define CLR_GRAY_DIM     lv_color_hex(0x546E7A)
#define CLR_SEPARATOR    lv_color_hex(0x1A2D42)

// Bus type badge colors
#define CLR_TYPE_ORD     lv_color_hex(0x37474F)  // Ordinary
#define CLR_TYPE_EXP     lv_color_hex(0x1565C0)  // Express
#define CLR_TYPE_DLX     lv_color_hex(0x6A1B9A)  // Deluxe
#define CLR_TYPE_AC      lv_color_hex(0x00695C)  // A/C

// Alert severity colors
#define CLR_ALERT_INFO   lv_color_hex(0x0D47A1)
#define CLR_ALERT_WARN   lv_color_hex(0xE65100)
#define CLR_ALERT_CRIT   lv_color_hex(0xB71C1C)

// ─── Layout Constants ────────────────────────────────────────
#define HEADER_HEIGHT     48
#define FOOTER_HEIGHT     32
#define TABBAR_HEIGHT     50
#define BUS_ROW_HEIGHT    46
#define CARD_RADIUS       8
#define CARD_PAD          10
#define TICKER_HEIGHT     FOOTER_HEIGHT

// ─── Screen Pages (tab indices) ──────────────────────────────
#define PAGE_HOME       0
#define PAGE_SEARCH     1
#define PAGE_SCHEDULE   2
#define PAGE_ALERTS     3
#define PAGE_SETTINGS   4

// ─── ETA Color Helper ────────────────────────────────────────
static inline lv_color_t eta_color(int eta_minutes, bool is_delayed) {
    if (is_delayed) return CLR_ETA_RED;
    if (eta_minutes <= 5)  return CLR_ETA_GREEN;
    if (eta_minutes <= 20) return CLR_ETA_YELLOW;
    if (eta_minutes <= 40) return CLR_ETA_ORANGE;
    return CLR_ETA_RED;
}

// ─── Bus Type Badge Color ────────────────────────────────────
static inline lv_color_t type_badge_color(const char* type) {
    if (strcmp(type, "express") == 0) return CLR_TYPE_EXP;
    if (strcmp(type, "deluxe")  == 0) return CLR_TYPE_DLX;
    if (strcmp(type, "ac")      == 0) return CLR_TYPE_AC;
    return CLR_TYPE_ORD;
}

#endif // UI_H
