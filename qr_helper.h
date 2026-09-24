#pragma once
// WiFi QR code for "WIFI:T:nopass;S:WaveShare-Clock;;"
// Pre-computed: Python qrcode lib, ECC=L, Version 3, 29×29 modules
// Scan with any phone camera to auto-connect to the setup AP.
//
// Rendering: draw event callback on the container obj — no canvas or img
// module needed; uses only lv_draw_rect() which is always available.

static const uint8_t WIFI_QR_SIZE = 29;
static const uint8_t WIFI_QR_DATA[29][29] = {
    {1,1,1,1,1,1,1,0,0,1,0,0,0,0,0,1,1,0,1,1,1,0,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,1,0,1,1,0,1,1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,1},
    {1,0,1,1,1,0,1,0,0,1,1,1,0,0,1,0,1,0,0,0,0,0,1,0,1,1,1,0,1},
    {1,0,1,1,1,0,1,0,1,1,0,1,0,0,0,0,1,1,0,1,0,0,1,0,1,1,1,0,1},
    {1,0,1,1,1,0,1,0,0,0,1,0,1,1,1,1,0,0,0,1,1,0,1,0,1,1,1,0,1},
    {1,0,0,0,0,0,1,0,1,0,1,0,0,1,1,1,1,1,1,0,1,0,1,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,1,1,1,1,1,1},
    {0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,1,0,0,1,1,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,0,1,0,0,1,1,0,1,0,1,0,1,0},
    {0,0,1,0,1,1,0,0,0,1,0,0,0,0,0,1,1,0,1,0,1,0,1,1,1,0,1,1,0},
    {1,0,1,0,0,1,1,1,0,1,0,1,1,0,0,0,0,0,0,0,1,1,0,1,0,1,0,0,0},
    {1,0,0,1,0,0,0,1,1,1,1,1,0,0,1,0,1,0,0,0,1,0,1,0,1,0,0,1,1},
    {1,0,0,1,1,0,1,0,1,1,0,1,0,0,0,0,0,1,1,0,0,1,0,1,0,1,1,0,0},
    {0,0,0,1,0,0,0,1,1,0,1,0,1,1,1,1,0,1,1,1,1,0,1,1,1,0,1,1,0},
    {0,1,1,0,0,1,1,1,1,0,1,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,1,0,0},
    {1,1,0,0,0,0,0,1,0,0,0,0,1,1,0,0,0,0,0,1,1,0,1,0,1,1,0,0,0},
    {1,0,0,1,0,1,1,1,1,1,1,0,1,0,1,0,1,0,0,0,1,0,0,0,0,1,0,1,1},
    {1,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1,1,1,0,0,1,1,1,0,1,0},
    {1,0,0,0,1,1,1,0,1,1,1,1,1,0,0,1,1,1,1,0,1,1,0,0,1,0,0,0,0},
    {1,0,0,1,0,0,0,0,1,0,1,1,0,0,1,0,0,0,0,0,0,0,1,0,1,0,0,0,1},
    {1,0,1,1,0,1,1,1,1,0,0,1,0,0,0,0,1,1,0,0,1,1,1,1,1,1,1,0,0},
    {0,0,0,0,0,0,0,0,1,0,1,0,1,1,1,0,0,1,1,0,1,0,0,0,1,0,1,0,0},
    {1,1,1,1,1,1,1,0,1,0,1,0,0,1,1,1,1,0,0,1,1,0,1,0,1,0,1,0,0},
    {1,0,0,0,0,0,1,0,0,1,1,0,1,1,0,0,0,0,1,1,1,0,0,0,1,1,0,0,0},
    {1,0,1,1,1,0,1,0,1,0,1,0,1,0,1,0,0,1,0,0,1,1,1,1,1,0,1,1,0},
    {1,0,1,1,1,0,1,0,1,1,1,0,0,0,0,0,1,1,1,1,1,1,0,1,0,1,1,1,1},
    {1,0,1,1,1,0,1,0,1,0,1,1,1,0,0,1,1,1,0,1,0,0,1,1,1,0,1,1,0},
    {1,0,0,0,0,0,1,0,1,1,1,1,0,0,1,0,0,0,0,1,0,1,1,1,0,1,0,1,0},
    {1,1,1,1,1,1,1,0,1,1,1,1,0,0,0,1,0,1,0,1,0,1,1,1,0,0,1,0,0}
};

// Draw event callback: fires on LV_EVENT_DRAW_MAIN_END so QR renders
// on top of the white background fill of the container.
static void _qr_draw_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN_END) return;

    lv_obj_t*      obj      = lv_event_get_target(e);
    lv_draw_ctx_t* draw_ctx = lv_event_get_draw_ctx(e);

    lv_area_t obj_coords;
    lv_obj_get_coords(obj, &obj_coords);

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color    = lv_color_black();
    dsc.bg_opa      = LV_OPA_COVER;
    dsc.radius      = 0;
    dsc.border_width = 0;

    const int SCALE = 3, MARGIN = 2;
    for (int r = 0; r < WIFI_QR_SIZE; r++) {
        for (int c = 0; c < WIFI_QR_SIZE; c++) {
            if (!WIFI_QR_DATA[r][c]) continue;
            lv_area_t a;
            a.x1 = obj_coords.x1 + MARGIN + c * SCALE;
            a.y1 = obj_coords.y1 + MARGIN + r * SCALE;
            a.x2 = a.x1 + SCALE - 1;
            a.y2 = a.y1 + SCALE - 1;
            lv_draw_rect(draw_ctx, &dsc, &a);
        }
    }
}

// Call once with the white 91×91 container obj (qr_placeholder).
// No canvas or image module required — only core lv_draw_rect.
static void draw_wifi_qr(lv_obj_t* parent) {
    lv_obj_add_event_cb(parent, _qr_draw_cb, LV_EVENT_DRAW_MAIN_END, NULL);
    lv_obj_invalidate(parent);
}
