#pragma once
#include "lvgl.h"

// ── 7-Segment rectangles with per-segment slide animation ────────────────────
// bit0=A(top) bit1=B(top-R) bit2=C(bot-R) bit3=D(bot) bit4=E(bot-L)
// bit5=F(top-L) bit6=G(mid)

#define ANIM_SEG_MS 260

static const uint8_t SEG_MAP[10] = {
    0b0111111,  // 0
    0b0000110,  // 1
    0b1011011,  // 2
    0b1001111,  // 3
    0b1100110,  // 4
    0b1101101,  // 5
    0b1111101,  // 6
    0b0000111,  // 7
    0b1111111,  // 8
    0b1101111,  // 9
};

static const int  SEG_FULL[7]    = {42, 44, 44, 42, 44, 44, 42};
static const bool SEG_IS_H[7]    = {true,false,false,true,false,false,true};

struct SegDigit { lv_obj_t* s[7]; int8_t val; };
static SegDigit  g_digs[6];
static lv_obj_t* g_ghost[6][7];
static lv_obj_t* g_colon_act[2];

// ── init ─────────────────────────────────────────────────────────────────────

inline void seg_init(int d,
    lv_obj_t* sa, lv_obj_t* sb, lv_obj_t* sc, lv_obj_t* sd,
    lv_obj_t* se, lv_obj_t* sf, lv_obj_t* sg,
    lv_obj_t* ga, lv_obj_t* gb, lv_obj_t* gc, lv_obj_t* gd,
    lv_obj_t* ge, lv_obj_t* gf, lv_obj_t* gg)
{
    g_digs[d].s[0]=sa; g_digs[d].s[1]=sb; g_digs[d].s[2]=sc; g_digs[d].s[3]=sd;
    g_digs[d].s[4]=se; g_digs[d].s[5]=sf; g_digs[d].s[6]=sg;
    g_ghost[d][0]=ga; g_ghost[d][1]=gb; g_ghost[d][2]=gc; g_ghost[d][3]=gd;
    g_ghost[d][4]=ge; g_ghost[d][5]=gf; g_ghost[d][6]=gg;
    g_digs[d].val = -1;
    for (int i = 0; i < 7; i++) {
        lv_obj_t* obj = g_digs[d].s[i];
        if (SEG_IS_H[i]) {
            // Store right_x = orig_x + full_width so _sa_h can keep it fixed.
            lv_coord_t rx = (lv_coord_t)lv_obj_get_x(obj) + (lv_coord_t)SEG_FULL[i];
            lv_obj_set_user_data(obj, (void*)(intptr_t)rx);
            lv_obj_set_x(obj, rx);   // park at right edge with zero width
            lv_obj_set_width(obj, 0);
        } else {
            lv_obj_set_height(obj, 0);
        }
    }
}

inline void colon_init(lv_obj_t* at, lv_obj_t* ab) {
    g_colon_act[0]=at; g_colon_act[1]=ab;
}

// ── Boot guard flag — true until NTP sync ────────────────────────────────────
static bool g_boot_pulsing = false;

// ── animation ─────────────────────────────────────────────────────────────────
//
// Horizontal segments (A, D, G) are RIGHT-EDGE anchored:
//   right_x = orig_x + full_width  (stored in user_data at seg_init)
//   animated:  x = right_x - v,  width = v
// Effect: segments grow from the B/C side (right) leftward, and retract
// back toward B/C.  This matches how digits morph into their neighbours —
// the segments that connect to the right-side verticals (which are almost
// always present) emerge from and absorb into that junction.
//
// Vertical segments (B, C, E, F) remain TOP-anchored (y fixed, height varies).
// They already grow down from the A/G junctions, which is correct.

static void _sa_h(void* obj, int32_t v) {
    lv_obj_t* o = (lv_obj_t*)obj;
    lv_coord_t rx = (lv_coord_t)(intptr_t)lv_obj_get_user_data(o);
    lv_obj_set_x(o, rx - v);
    lv_obj_set_width(o, v);
}

static void _run_anim(lv_obj_t* obj, lv_coord_t from, lv_coord_t to, bool is_h) {
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, is_h ? _sa_h : (lv_anim_exec_xcb_t)lv_obj_set_height);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, ANIM_SEG_MS);
    lv_anim_set_path_cb(&a, to > from ? lv_anim_path_ease_out : lv_anim_path_ease_in);
    lv_anim_start(&a);
}

// Snap helpers — set position correctly for the right-anchor scheme
static inline void _seg_show(lv_obj_t* obj, int full, bool is_h) {
    if (is_h) {
        lv_coord_t rx = (lv_coord_t)(intptr_t)lv_obj_get_user_data(obj);
        lv_obj_set_x(obj, rx - full);
        lv_obj_set_width(obj, full);
    } else {
        lv_obj_set_height(obj, full);
    }
}
static inline void _seg_hide(lv_obj_t* obj, int full, bool is_h) {
    if (is_h) {
        lv_coord_t rx = (lv_coord_t)(intptr_t)lv_obj_get_user_data(obj);
        lv_obj_set_x(obj, rx);
        lv_obj_set_width(obj, 0);
    } else {
        lv_obj_set_height(obj, 0);
    }
}

inline void seg_set(int d, int v, bool anim) {
    SegDigit& dig = g_digs[d];
    if (dig.val == v) return;
    uint8_t nm = SEG_MAP[v];
    uint8_t om = (dig.val >= 0) ? SEG_MAP[dig.val] : 0;
    for (int i = 0; i < 7; i++) {
        bool was=(om>>i)&1, now=(nm>>i)&1;
        lv_obj_t* obj = dig.s[i]; int full = SEG_FULL[i]; bool is_h = SEG_IS_H[i];
        if (now && !was) {
            if (anim) {
                lv_coord_t cur = is_h ? lv_obj_get_width(obj) : lv_obj_get_height(obj);
                lv_anim_del(obj, NULL);
                _run_anim(obj, cur, full, is_h);
            } else { _seg_show(obj, full, is_h); }
        } else if (!now && was) {
            if (anim) {
                lv_coord_t cur = is_h ? lv_obj_get_width(obj) : lv_obj_get_height(obj);
                lv_anim_del(obj, NULL);
                _run_anim(obj, cur, 0, is_h);
            } else { _seg_hide(obj, full, is_h); }
        } else if (now) {
            _seg_show(obj, full, is_h);
        }
    }
    dig.val = v;
}

// ── Theme ─────────────────────────────────────────────────────────────────────

struct ClockTheme {
    lv_color_t bg, digit, ghost, colon_c, secondary,
               bar_bg, bar_fg, header_bg, arc_bg_c, arc_fg_c, date_c;
};

struct ThemePair { ClockTheme day, night; const char* name; };

// Primary – cream bg + deep navy digits
static const ThemePair TP_PRIMARY = {
    { lv_color_hex(0xEDE8DB), lv_color_hex(0x0A246E), lv_color_hex(0xE3DDD4),
      lv_color_hex(0x1A3A8C), lv_color_hex(0x4A6AB0),
      lv_color_hex(0xD5CEBC), lv_color_hex(0x1A3A8C),
      lv_color_hex(0xD8D0C0), lv_color_hex(0xA8A090), lv_color_hex(0x1A3A8C),
      lv_color_hex(0x0A1E58) },
    { lv_color_hex(0x06080E), lv_color_hex(0x5CAAFF), lv_color_hex(0x0E1828),
      lv_color_hex(0x3A78DD), lv_color_hex(0x2A3E66),
      lv_color_hex(0x0E1828), lv_color_hex(0x3A78DD),
      lv_color_hex(0x0C1020), lv_color_hex(0x203560), lv_color_hex(0x3A78DD),
      lv_color_hex(0x6090D8) },
    "Primary"
};

// Pastel – light pink bg + vivid hot-pink/magenta digits
static const ThemePair TP_PASTEL = {
    { lv_color_hex(0xFDE8F2), lv_color_hex(0xCC1270), lv_color_hex(0xF5D0E6),
      lv_color_hex(0xCC1270), lv_color_hex(0xC060A0),
      lv_color_hex(0xECC8DE), lv_color_hex(0xCC1270),
      lv_color_hex(0xF8D8EB), lv_color_hex(0xD090B8), lv_color_hex(0xCC1270),
      lv_color_hex(0x8A0848) },
    { lv_color_hex(0x140410), lv_color_hex(0xFF40B8), lv_color_hex(0x1E0818),
      lv_color_hex(0xFF40B8), lv_color_hex(0xA02080),
      lv_color_hex(0x1A0614), lv_color_hex(0xE030A8),
      lv_color_hex(0x160510), lv_color_hex(0x380A28), lv_color_hex(0xE030A8),
      lv_color_hex(0xE898C8) },
    "Pastel"
};

// Neon – green / cyan (unchanged)
static const ThemePair TP_NEON = {
    { lv_color_hex(0x050A05), lv_color_hex(0x00FF41), lv_color_hex(0x0A140A),
      lv_color_hex(0x00CC33), lv_color_hex(0x00AA28),
      lv_color_hex(0x071407), lv_color_hex(0x00FF41),
      lv_color_hex(0x080D08), lv_color_hex(0x143814), lv_color_hex(0x00FF41),
      lv_color_hex(0x44FF66) },
    { lv_color_hex(0x020408), lv_color_hex(0x00FFFF), lv_color_hex(0x021018),
      lv_color_hex(0x00CCCC), lv_color_hex(0x009999),
      lv_color_hex(0x030C14), lv_color_hex(0x00FFFF),
      lv_color_hex(0x030610), lv_color_hex(0x082040), lv_color_hex(0x00FFFF),
      lv_color_hex(0x66FFFF) },
    "Neon"
};

// Neutral – true high-contrast monochrome
static const ThemePair TP_NEUTRAL = {
    { lv_color_hex(0xF4F4F4), lv_color_hex(0x181818), lv_color_hex(0xE4E4E4),
      lv_color_hex(0x181818), lv_color_hex(0x686868),
      lv_color_hex(0xCCCCCC), lv_color_hex(0x181818),
      lv_color_hex(0xDCDCDC), lv_color_hex(0x909090), lv_color_hex(0x181818),
      lv_color_hex(0x282828) },
    { lv_color_hex(0x0E0E0E), lv_color_hex(0xEAEAEA), lv_color_hex(0x1C1C1C),
      lv_color_hex(0xEAEAEA), lv_color_hex(0x888888),
      lv_color_hex(0x1E1E1E), lv_color_hex(0xC0C0C0),
      lv_color_hex(0x181818), lv_color_hex(0x404040), lv_color_hex(0xC0C0C0),
      lv_color_hex(0xC8C8C8) },
    "Neutral"
};

// Warm – golden bg + deep amber/orange digits
static const ThemePair TP_WARM = {
    { lv_color_hex(0xFFF6E8), lv_color_hex(0xB84000), lv_color_hex(0xF0E0C4),
      lv_color_hex(0xB84000), lv_color_hex(0xCC6020),
      lv_color_hex(0xE8D0A0), lv_color_hex(0xB84000),
      lv_color_hex(0xF0DCB8), lv_color_hex(0xCC9840), lv_color_hex(0xB84000),
      lv_color_hex(0x782800) },
    { lv_color_hex(0x130700), lv_color_hex(0xFF8A00), lv_color_hex(0x1E0F00),
      lv_color_hex(0xFF8A00), lv_color_hex(0xB06020),
      lv_color_hex(0x200E00), lv_color_hex(0xE07800),
      lv_color_hex(0x1A0B00), lv_color_hex(0x402000), lv_color_hex(0xE07800),
      lv_color_hex(0xFFC060) },
    "Warm"
};

// Cool – sky blue bg + deep ocean-blue digits
static const ThemePair TP_COOL = {
    { lv_color_hex(0xE0F0FC), lv_color_hex(0x0060B0), lv_color_hex(0xC4DFF5),
      lv_color_hex(0x0060B0), lv_color_hex(0x3888C8),
      lv_color_hex(0xB0D4EE), lv_color_hex(0x0060B0),
      lv_color_hex(0xC8E4F4), lv_color_hex(0x70B0E0), lv_color_hex(0x0060B0),
      lv_color_hex(0x003870) },
    { lv_color_hex(0x02090F), lv_color_hex(0x00D0FF), lv_color_hex(0x041018),
      lv_color_hex(0x00D0FF), lv_color_hex(0x006888),
      lv_color_hex(0x041418), lv_color_hex(0x00AACC),
      lv_color_hex(0x030E14), lv_color_hex(0x083050), lv_color_hex(0x00AACC),
      lv_color_hex(0x48C8E8) },
    "Cool"
};

static const ThemePair* const THEME_PAIRS[] = {
    &TP_PRIMARY, &TP_PASTEL, &TP_NEON, &TP_NEUTRAL, &TP_WARM, &TP_COOL
};
#define NUM_THEMES 6

// ── Timezones ─────────────────────────────────────────────────────────────────

static const char* TZ_POSIX[] = {
    "PST8PDT,M3.2.0,M11.1.0",            // Pacific
    "MST7MDT,M3.2.0,M11.1.0",            // Mountain
    "CST6CDT,M3.2.0,M11.1.0",            // Central
    "EST5EDT,M3.2.0,M11.1.0",            // Eastern
    "AKST9AKDT,M3.2.0,M11.1.0",          // Alaska
    "HST10",                              // Hawaii (no DST)
    "GMT0BST,M3.5.0/1,M10.5.0",          // London
    "CET-1CEST,M3.5.0,M10.5.0/3",        // Paris / Berlin
    "JST-9",                              // Tokyo (no DST)
    "AEST-10AEDT,M10.1.0,M4.1.0/3",      // Sydney
};
static const char* TZ_LABELS[] = {
    "Pacific (PT)", "Mountain (MT)", "Central (CT)", "Eastern (ET)",
    "Alaska (AKT)", "Hawaii (HST)", "London (GMT)", "Paris (CET)",
    "Tokyo (JST)",  "Sydney (AEST)",
};
// NVS timezone string names (for web UI and settings)
static const char* TZ_NAMES[] = {
    "America/Los_Angeles", "America/Denver", "America/Chicago", "America/New_York",
    "America/Anchorage",   "Pacific/Honolulu", "Europe/London", "Europe/Paris",
    "Asia/Tokyo",          "Australia/Sydney",
};
#define NUM_TZ 10

// ── Runtime globals ───────────────────────────────────────────────────────────

static bool g_is_day      = true;
static int  g_theme_idx   = 0;      // index into THEME_PAIRS
static bool g_use_24h     = false;
static int  g_tz_idx      = 0;      // index into TZ_POSIX / TZ_LABELS
static bool g_night_auto  = true;   // auto day/night from hour, else force

// ── apply_theme ───────────────────────────────────────────────────────────────

inline void apply_theme(const ClockTheme& t,
    lv_obj_t* scr, lv_obj_t* hdr,
    lv_obj_t* lbl_date, lv_obj_t* lbl_ampm,
    lv_obj_t* bar_bg_o, lv_obj_t* bar_fg_o,
    lv_obj_t* sec_arc, lv_obj_t* lbl_sec_dig)
{
    lv_obj_set_style_bg_color(scr,              t.bg,        LV_PART_MAIN);
    lv_obj_set_style_bg_color(hdr,              t.header_bg, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_date,       t.date_c,    LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_ampm,       t.secondary, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_sec_dig,    t.digit,     LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_bg_o,         t.bar_bg,    LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_fg_o,         t.bar_fg,    LV_PART_MAIN);
    lv_obj_set_style_arc_color(sec_arc,         t.arc_bg_c,  LV_PART_MAIN);
    lv_obj_set_style_arc_color(sec_arc,         t.arc_fg_c,  LV_PART_INDICATOR);
    for (int d = 0; d < 4; d++)  // H0 H1 M0 M1 only — seconds use lbl_sec_dig
        for (int i = 0; i < 7; i++) {
            lv_obj_set_style_bg_color(g_digs[d].s[i],  t.digit, LV_PART_MAIN);
            lv_obj_set_style_bg_color(g_ghost[d][i],   t.ghost, LV_PART_MAIN);
        }
    for (int i = 0; i < 2; i++)
        lv_obj_set_style_bg_color(g_colon_act[i], t.colon_c, LV_PART_MAIN);
}

// Convenience: apply the current theme pair (day or night side)
inline void apply_current_theme(
    lv_obj_t* scr, lv_obj_t* hdr,
    lv_obj_t* lbl_date, lv_obj_t* lbl_ampm,
    lv_obj_t* bar_bg_o, lv_obj_t* bar_fg_o,
    lv_obj_t* sec_arc, lv_obj_t* lbl_sec_dig)
{
    const ClockTheme& t = g_is_day ? THEME_PAIRS[g_theme_idx]->day
                                   : THEME_PAIRS[g_theme_idx]->night;
    apply_theme(t, scr, hdr, lbl_date, lbl_ampm, bar_bg_o, bar_fg_o, sec_arc, lbl_sec_dig);
}

// Cycle to next theme, returns name of new theme
inline const char* cycle_theme() {
    g_theme_idx = (g_theme_idx + 1) % NUM_THEMES;
    return THEME_PAIRS[g_theme_idx]->name;
}

// Apply current timezone from TZ_POSIX[g_tz_idx]
inline void apply_tz() {
    setenv("TZ", TZ_POSIX[g_tz_idx], 1);
    tzset();
}

// Cycle to next timezone, returns short label
inline const char* cycle_tz() {
    g_tz_idx = (g_tz_idx + 1) % NUM_TZ;
    apply_tz();
    return TZ_LABELS[g_tz_idx];
}
