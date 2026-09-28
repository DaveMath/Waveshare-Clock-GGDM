#pragma once
// ── WiFi Provisioning + Settings Server ──────────────────────────────────────
// First boot (no stored WiFi): AP "WaveShare-Clock" at 1.2.3.4, captive DNS,
// setup web page to enter SSID/password/timezone.
// Clock mode: same AP stays up at 1.2.3.4, serves settings page so the user
// can change theme, time format, timezone, and night mode at any time.
//
// NVS namespace "clk":
//   ssid, pass, tz   – WiFi + timezone string
//   theme, h24, tzidx, nauto – clock settings (stored as strings)

#include <string>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "lwip/sockets.h"
#include "freertos/task.h"

// clock_helper.h must be included before this file (TZ_NAMES, NUM_TZ, etc.)

#define PROV_NVS_NS  "clk"
#define PROV_AP_SSID "WaveShare-Clock"

bool g_provisioning = false;
int  g_settings_changed = 0;  // set by web handler; checked in interval lambda

// ── NVS helpers ───────────────────────────────────────────────────────────────

static void _nvs_write(const char* key, const char* val) {
    nvs_handle_t h;
    if (nvs_open(PROV_NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, key, val);
    nvs_commit(h);
    nvs_close(h);
}
static bool _nvs_read(const char* key, char* buf, size_t len) {
    nvs_handle_t h;
    if (nvs_open(PROV_NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    esp_err_t e = nvs_get_str(h, key, buf, &len);
    nvs_close(h);
    return e == ESP_OK && buf[0] != '\0';
}
static int _nvs_read_int(const char* key, int def) {
    char b[8] = {}; return _nvs_read(key, b, sizeof(b)) ? atoi(b) : def;
}
static void _nvs_write_int(const char* key, int val) {
    char b[8]; snprintf(b, sizeof(b), "%d", val);
    _nvs_write(key, b);
}

// WiFi credentials
inline bool prov_needs_setup() {
    char b[64] = {}; return !_nvs_read("ssid", b, sizeof(b));
}
inline std::string prov_get_ssid() { char b[64]={}; _nvs_read("ssid",b,64); return b; }
inline std::string prov_get_pass() { char b[64]={}; _nvs_read("pass",b,64); return b; }

// Clock settings — theme (0-5), h24 (0/1), tzidx (0-9), nauto (0=auto,1=day,2=night)
inline int  prov_get_theme()   { return _nvs_read_int("theme", 0); }
inline bool prov_get_24h()     { return _nvs_read_int("h24",   0) != 0; }
inline int  prov_get_tz_idx()  { return _nvs_read_int("tzidx", 0); }
inline int  prov_get_night_mode() { return _nvs_read_int("nauto", 0); } // 0=auto,1=day,2=night

inline void prov_save_theme(int v)     { _nvs_write_int("theme", v); g_settings_changed=1; }
inline void prov_save_24h(bool v)      { _nvs_write_int("h24",   v?1:0); g_settings_changed=1; }
inline void prov_save_tz_idx(int v)    { _nvs_write_int("tzidx", v); g_settings_changed=1; }
inline void prov_save_night_mode(int v){ _nvs_write_int("nauto", v); g_settings_changed=1; }

// ── Set AP IP to 1.2.3.4 ─────────────────────────────────────────────────────

static void prov_set_ap_ip() {
    esp_netif_t* ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap) return;
    esp_netif_dhcps_stop(ap);
    esp_netif_ip_info_t ip = {};
    IP4_ADDR(&ip.ip,      1, 2, 3, 4);
    IP4_ADDR(&ip.gw,      1, 2, 3, 4);
    IP4_ADDR(&ip.netmask, 255, 255, 255, 0);
    esp_netif_set_ip_info(ap, &ip);
    esp_netif_dhcps_start(ap);
}

// ── WiFi connection (clock mode — raw ESP-IDF) ────────────────────────────────

inline void prov_connect_wifi() {
    std::string ssid = prov_get_ssid();
    std::string pass = prov_get_pass();
    if (ssid.empty()) return;
    wifi_config_t cfg = {};
    strncpy((char*)cfg.sta.ssid,     ssid.c_str(), 32);
    strncpy((char*)cfg.sta.password, pass.c_str(), 64);
    cfg.sta.scan_method = WIFI_FAST_SCAN;
    cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    esp_wifi_set_mode(WIFI_MODE_APSTA);    // STA can't connect in AP-only mode
    vTaskDelay(pdMS_TO_TICKS(300));        // let STA netif initialize
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_connect();
}

// ── DNS captive portal ────────────────────────────────────────────────────────

static void prov_dns_task(void*) {
    int sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int opt = 1;
    ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(53);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (::bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(sock); vTaskDelete(NULL); return;
    }
    uint8_t buf[512];
    struct sockaddr_in client; socklen_t clen = sizeof(client);
    while (true) {
        int n = ::recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&client, &clen);
        if (n < 12) continue;
        buf[2]=0x81; buf[3]=0x80;
        uint16_t qc=(buf[4]<<8)|buf[5];
        buf[6]=0; buf[7]=(uint8_t)qc;
        int pos = 12;
        for (int q = 0; q < qc && pos < n; q++) {
            while (pos < n && buf[pos]) pos += buf[pos]+1;
            pos += 5;
        }
        if (pos+16 < (int)sizeof(buf)) {
            buf[pos++]=0xC0; buf[pos++]=0x0C;
            buf[pos++]=0;    buf[pos++]=1;
            buf[pos++]=0;    buf[pos++]=1;
            buf[pos++]=0;    buf[pos++]=0; buf[pos++]=0; buf[pos++]=60;
            buf[pos++]=0;    buf[pos++]=4;
            buf[pos++]=1;    buf[pos++]=2; buf[pos++]=3; buf[pos++]=4;
        }
        ::sendto(sock, buf, pos, 0, (struct sockaddr*)&client, clen);
    }
}

// ── WiFi scan (used by setup page) ───────────────────────────────────────────

// Pre-populated to valid JSON so /scan always returns something
static char prov_scan_json[2048] = "[]";

// Assumes WiFi already in APSTA mode — no mode switching
static void prov_do_scan() {
    wifi_scan_config_t cfg = {};
    cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    cfg.scan_time.active.min = 100;
    cfg.scan_time.active.max = 300;
    if (esp_wifi_scan_start(&cfg, true) != ESP_OK) return;
    uint16_t cnt = 20;
    static wifi_ap_record_t recs[20];
    esp_wifi_scan_get_ap_num(&cnt);
    if (cnt > 20) cnt = 20;
    esp_wifi_scan_get_ap_records(&cnt, recs);
    int pos = snprintf(prov_scan_json, sizeof(prov_scan_json), "[");
    for (int i = 0; i < cnt; i++) {
        if (pos >= (int)sizeof(prov_scan_json)-80) break;
        char esc[66]={}; int ei=0;
        for (int j=0; recs[i].ssid[j] && j<32; j++) {
            char c=recs[i].ssid[j];
            if (c=='"'||c=='\\') esc[ei++]='\\';
            esc[ei++]=c;
        }
        pos += snprintf(prov_scan_json+pos, sizeof(prov_scan_json)-pos,
            "%s{\"ssid\":\"%s\",\"rssi\":%d,\"open\":%s}",
            i?",":"", esc, recs[i].rssi,
            recs[i].authmode==WIFI_AUTH_OPEN?"true":"false");
    }
    snprintf(prov_scan_json+pos, sizeof(prov_scan_json)-pos, "]");
}

// Re-scans every 30 s in background; results cached in prov_scan_json
static void prov_scan_task(void*) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        prov_do_scan();
    }
}

// ── JSON parse helper ─────────────────────────────────────────────────────────

static std::string _json_str(const char* src, const char* key) {
    std::string k = std::string("\"") + key + "\":\"";
    const char* p = strstr(src, k.c_str());
    if (!p) return "";
    p += k.size();
    std::string out;
    while (*p && *p != '"') {
        if (*p == '\\' && *(p+1)) p++;
        out += *p++;
    }
    return out;
}
static int _json_int(const char* src, const char* key, int def) {
    std::string k = std::string("\"") + key + "\":";
    const char* p = strstr(src, k.c_str());
    if (!p) return def;
    p += k.size();
    return atoi(p);
}
static bool _json_bool(const char* src, const char* key, bool def) {
    std::string k = std::string("\"") + key + "\":";
    const char* p = strstr(src, k.c_str());
    if (!p) return def;
    p += k.size();
    return strncmp(p, "true", 4) == 0;
}

// ── HTML pages ────────────────────────────────────────────────────────────────

static const char PROV_COMMON_CSS[] = R"css(
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;background:#EDE8DB;color:#152E6E;min-height:100vh;display:flex;flex-direction:column;align-items:center;justify-content:flex-start;padding:16px}
.card{background:#fff;border-radius:14px;padding:22px 20px;width:100%;max-width:440px;box-shadow:0 4px 20px rgba(21,46,110,.12);margin-bottom:12px}
h1{font-size:1.25rem;margin-bottom:3px}
.sub{color:#5C7AB8;font-size:.82rem;margin-bottom:16px}
h2{font-size:.85rem;font-weight:700;color:#2E5499;text-transform:uppercase;letter-spacing:.06em;margin:14px 0 6px}
label{display:flex;align-items:center;gap:8px;font-size:.9rem;color:#152E6E;margin:6px 0;cursor:pointer}
input[type=text],input[type=password],select{width:100%;padding:9px 11px;border:1.5px solid #C8BFA8;border-radius:7px;font-size:.92rem;color:#152E6E;background:#FAF8F4;outline:none}
input[type=text]:focus,input[type=password]:focus,select:focus{border-color:#2E5499}
input[type=radio]{accent-color:#2E5499;width:16px;height:16px}
.btn{display:inline-block;padding:9px 18px;background:#2E5499;color:#fff;border:none;border-radius:7px;font-size:.92rem;font-weight:600;cursor:pointer;text-decoration:none;transition:background .18s}
.btn:hover{background:#152E6E}
.btn-sm{padding:6px 12px;font-size:.82rem}
.btn-outline{background:transparent;border:1.5px solid #2E5499;color:#2E5499}
.btn-outline:hover{background:#EDE8DB}
.save-btn{width:100%;padding:11px;margin-top:16px}
.msg{margin-top:10px;padding:9px 13px;border-radius:7px;font-size:.88rem;display:none}
.ok{background:#d4edda;color:#155724;display:block}
.err{background:#f8d7da;color:#721c24;display:block}
.net-list{list-style:none;max-height:150px;overflow-y:auto;border:1.5px solid #C8BFA8;border-radius:7px;background:#FAF8F4}
.net-list li{padding:8px 11px;cursor:pointer;font-size:.88rem;display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid #EDE8DB}
.net-list li:last-child{border-bottom:none}
.net-list li:hover{background:#EDE8DB}
.bars{color:#5C7AB8;font-size:.78rem}
footer{text-align:center;font-size:.78rem;color:#8899BB;margin-top:8px;padding-bottom:8px}
footer a{color:#2E5499;text-decoration:none}
.theme-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin:4px 0}
.theme-btn{border:2.5px solid transparent;border-radius:9px;padding:9px 4px;text-align:center;font-size:.78rem;font-weight:700;cursor:pointer;transition:border-color .15s}
.theme-btn.selected{border-color:#152E6E}
)css";

static const char SETUP_HTML[] = R"html(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Clock Setup — Clock-By-GGDM</title><style>)html";

static const char SETTINGS_HTML[] = R"html(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Clock Settings — Clock-By-GGDM</title><style>)html";

// Shared footer
static const char HTML_FOOTER[] = R"html(
<footer>
  <p>Clock-By-GGDM &nbsp;|&nbsp; <a href="https://x.com/ggdm" target="_blank">@ggdm on X</a>
  &nbsp;|&nbsp; <a href="https://github.com/DaveMath/Waveshare-Clock-GGDM" target="_blank">GitHub</a></p>
</footer>
</body></html>)html";

// ── HTTP Handlers ─────────────────────────────────────────────────────────────

static esp_err_t h_scan(httpd_req_t* req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, prov_scan_json);
    return ESP_OK;
}

static esp_err_t h_save_wifi(httpd_req_t* req) {
    char body[512]={};
    int n = httpd_req_recv(req, body, sizeof(body)-1);
    if (n <= 0) { httpd_resp_send_500(req); return ESP_FAIL; }
    std::string ssid=_json_str(body,"ssid");
    std::string pass=_json_str(body,"pass");
    std::string tz  =_json_str(body,"tz");
    // find tz_idx from tz string name
    int tz_idx = 0;
    for (int i=0; i<NUM_TZ; i++) {
        if (tz == TZ_NAMES[i]) { tz_idx=i; break; }
    }
    httpd_resp_set_type(req, "application/json");
    if (ssid.empty()) {
        httpd_resp_sendstr(req, "{\"ok\":false,\"msg\":\"SSID required\"}");
        return ESP_OK;
    }
    _nvs_write("ssid", ssid.c_str());
    _nvs_write("pass", pass.c_str());
    _nvs_write_int("tzidx", tz_idx);
    httpd_resp_sendstr(req, "{\"ok\":true}");
    vTaskDelay(pdMS_TO_TICKS(600));
    esp_restart();
    return ESP_OK;
}

static esp_err_t h_api_get(httpd_req_t* req) {
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"theme\":%d,\"h24\":%s,\"tz\":%d,\"night\":%d}",
        prov_get_theme(),
        prov_get_24h() ? "true" : "false",
        prov_get_tz_idx(),
        prov_get_night_mode());
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

static esp_err_t h_api_post(httpd_req_t* req) {
    char body[256]={};
    int n = httpd_req_recv(req, body, sizeof(body)-1);
    if (n <= 0) { httpd_resp_send_500(req); return ESP_FAIL; }
    prov_save_theme(_json_int(body, "theme", prov_get_theme()));
    prov_save_24h(_json_bool(body, "h24", prov_get_24h()));
    prov_save_tz_idx(_json_int(body, "tz", prov_get_tz_idx()));
    prov_save_night_mode(_json_int(body, "night", prov_get_night_mode()));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

// ── Build and send setup page ─────────────────────────────────────────────────
static esp_err_t h_setup_page(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send_chunk(req, SETUP_HTML, sizeof(SETUP_HTML)-1);
    httpd_resp_send_chunk(req, PROV_COMMON_CSS, sizeof(PROV_COMMON_CSS)-1);
    static const char SETUP_BODY[] = R"html(</style></head><body>
<div class="card">
  <h1>⏰ Clock Setup</h1>
  <p class="sub">Connect your WiFi — WaveShare-Clock</p>
  <h2>Available Networks</h2>
  <ul class="net-list" id="nets"><li style="padding:10px;color:#8899BB">Scanning…</li></ul>
  <h2>Network Name (SSID)</h2>
  <input id="ssid" type="text" placeholder="Select above or type here" autocomplete="off">
  <h2>Password</h2>
  <input id="pass" type="password" placeholder="Leave blank for open networks">
  <h2>Timezone</h2>
  <select id="tz">
    <option value="America/Los_Angeles">Pacific (PT)</option>
    <option value="America/Denver">Mountain (MT)</option>
    <option value="America/Chicago">Central (CT)</option>
    <option value="America/New_York">Eastern (ET)</option>
    <option value="America/Anchorage">Alaska (AKT)</option>
    <option value="Pacific/Honolulu">Hawaii (HST)</option>
    <option value="Europe/London">London (GMT/BST)</option>
    <option value="Europe/Paris">Paris / Berlin (CET)</option>
    <option value="Asia/Tokyo">Tokyo (JST)</option>
    <option value="Australia/Sydney">Sydney (AEST)</option>
  </select>
  <button class="btn save-btn" id="btn" onclick="save()">Save &amp; Connect</button>
  <div class="msg" id="msg"></div>
</div>
<script>
function bars(r){return r>-55?'▂▄▆█':r>-70?'▂▄▆':r>-85?'▂▄':'▂'}
fetch('/scan').then(r=>r.json()).then(nets=>{
  const ul=document.getElementById('nets');
  if(!nets.length){ul.innerHTML='<li style="padding:10px;color:#8899BB">No networks found</li>';return}
  ul.innerHTML='';
  nets.forEach(n=>{
    const li=document.createElement('li');
    li.innerHTML='<span>'+(n.open?'':'🔒 ')+n.ssid+'</span><span class="bars">'+bars(n.rssi)+'</span>';
    li.onclick=()=>document.getElementById('ssid').value=n.ssid;
    ul.appendChild(li);
  });
}).catch(()=>{document.getElementById('nets').innerHTML='<li style="padding:10px">Scan unavailable</li>';});
function save(){
  const ssid=document.getElementById('ssid').value.trim();
  if(!ssid){showMsg('err','Please enter a network name.');return}
  const d={ssid,pass:document.getElementById('pass').value,tz:document.getElementById('tz').value};
  const btn=document.getElementById('btn');
  btn.disabled=true; btn.textContent='Saving…';
  fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)})
    .then(r=>r.json()).then(j=>{
      if(j.ok){showMsg('ok','✓ Saved! Clock is restarting…'); btn.textContent='Done';}
      else{showMsg('err','✗ '+j.msg); btn.disabled=false; btn.textContent='Save & Connect';}
    }).catch(()=>{showMsg('err','✗ Request failed.'); btn.disabled=false; btn.textContent='Save & Connect';});
}
function showMsg(cls,txt){const m=document.getElementById('msg');m.className='msg '+cls;m.textContent=txt;}
</script>)html";
    httpd_resp_send_chunk(req, SETUP_BODY, sizeof(SETUP_BODY)-1);
    httpd_resp_send_chunk(req, HTML_FOOTER, sizeof(HTML_FOOTER)-1);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// ── Build and send settings page ──────────────────────────────────────────────
static esp_err_t h_settings_page(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send_chunk(req, SETTINGS_HTML, sizeof(SETTINGS_HTML)-1);
    httpd_resp_send_chunk(req, PROV_COMMON_CSS, sizeof(PROV_COMMON_CSS)-1);
    static const char SETTINGS_BODY[] = R"html(</style></head><body>
<div class="card">
  <h1>&#9881; Clock Settings</h1>
  <p class="sub">Changes apply instantly &mdash; no restart needed.</p>

  <h2>Color Theme</h2>
  <div class="theme-grid" id="themes">
    <div class="theme-btn" id="t0" onclick="setTheme(0)">Primary</div>
    <div class="theme-btn" id="t1" onclick="setTheme(1)">Pastel</div>
    <div class="theme-btn" id="t2" onclick="setTheme(2)">Neon</div>
    <div class="theme-btn" id="t3" onclick="setTheme(3)">Neutral</div>
    <div class="theme-btn" id="t4" onclick="setTheme(4)">Warm</div>
    <div class="theme-btn" id="t5" onclick="setTheme(5)">Cool</div>
  </div>

  <h2>Night Mode</h2>
  <label><input type="radio" name="night" value="0"> Auto (dark 20:00&ndash;06:00)</label>
  <label><input type="radio" name="night" value="1"> Always Day</label>
  <label><input type="radio" name="night" value="2"> Always Night</label>

  <h2>Time Format</h2>
  <label><input type="radio" name="fmt" value="false"> 12-hour (AM / PM)</label>
  <label><input type="radio" name="fmt" value="true"> 24-hour</label>

  <h2>Timezone</h2>
  <select id="tz">
    <option value="0">Pacific (PT)</option>
    <option value="1">Mountain (MT)</option>
    <option value="2">Central (CT)</option>
    <option value="3">Eastern (ET)</option>
    <option value="4">Alaska (AKT)</option>
    <option value="5">Hawaii (HST)</option>
    <option value="6">London (GMT/BST)</option>
    <option value="7">Paris / Berlin (CET)</option>
    <option value="8">Tokyo (JST)</option>
    <option value="9">Sydney (AEST)</option>
  </select>

  <button class="btn save-btn" onclick="saveSettings()">Apply Settings</button>
  <div class="msg" id="msg"></div>
</div>

<div class="card">
  <h2>WiFi Credentials</h2>
  <p style="font-size:.88rem;color:#5C7AB8;margin-bottom:10px">Enter new credentials and tap Save &amp; Reconnect. The clock will restart.</p>
  <h2>Network Name (SSID)</h2>
  <input id="w_ssid" type="text" placeholder="Your WiFi network name" autocomplete="off">
  <h2>Password</h2>
  <input id="w_pass" type="password" placeholder="Leave blank for open networks">
  <button class="btn save-btn btn-outline" onclick="saveWifi()" style="margin-top:12px">Save &amp; Reconnect</button>
  <div class="msg" id="wmsg"></div>
</div>

<script>
// Theme palettes [dayBg, dayFg, nightBg, nightFg]
const PALETTES=[
  ['#EDE8DB','#0A246E','#06080E','#5CAAFF'],
  ['#FDE8F2','#CC1270','#140410','#FF40B8'],
  ['#050A05','#00FF41','#020408','#00FFFF'],
  ['#F4F4F4','#181818','#0E0E0E','#EAEAEA'],
  ['#FFF6E8','#B84000','#130700','#FF8A00'],
  ['#E0F0FC','#0060B0','#02090F','#00D0FF'],
];
let cur={theme:0,h24:false,tz:0,night:0};

function applyPalettes(){
  const h=new Date().getHours();
  const day=cur.night===1?true:cur.night===2?false:(h>=6&&h<20);
  document.querySelectorAll('.theme-btn').forEach((b,i)=>{
    const p=PALETTES[i];
    b.style.background=day?p[0]:p[2];
    b.style.color=day?p[1]:p[3];
  });
}

fetch('/api/settings').then(r=>r.json()).then(s=>{
  cur=s;
  applyPalettes();
  setThemeHighlight(s.theme);
  document.querySelectorAll('input[name=night]').forEach(r=>{ if(r.value==s.night) r.checked=true; });
  document.querySelectorAll('input[name=fmt]').forEach(r=>{ if(r.value==String(s.h24)) r.checked=true; });
  document.getElementById('tz').value=s.tz;
});

document.querySelectorAll('input[name=night]').forEach(r=>{
  r.addEventListener('change',()=>{ cur.night=parseInt(r.value); applyPalettes(); });
});

function setThemeHighlight(i){
  document.querySelectorAll('.theme-btn').forEach((b,idx)=>b.classList.toggle('selected',idx===i));
  cur.theme=i;
}
function setTheme(i){ setThemeHighlight(i); }

function saveSettings(){
  const night=parseInt(document.querySelector('input[name=night]:checked')?.value??cur.night);
  const h24=document.querySelector('input[name=fmt]:checked')?.value==='true';
  const tz=parseInt(document.getElementById('tz').value);
  const d={theme:cur.theme,h24,tz,night};
  fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)})
    .then(r=>r.json()).then(j=>{
      const m=document.getElementById('msg');
      m.className='msg '+(j.ok?'ok':'err');
      m.textContent=j.ok?'&#10003; Settings applied!':'&#10007; Save failed';
    }).catch(()=>{
      document.getElementById('msg').className='msg err';
      document.getElementById('msg').textContent='&#10007; Request failed';
    });
}

function saveWifi(){
  const ssid=document.getElementById('w_ssid').value.trim();
  if(!ssid){
    const m=document.getElementById('wmsg');
    m.className='msg err'; m.textContent='&#10007; Please enter a network name.'; return;
  }
  fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({ssid,pass:document.getElementById('w_pass').value,tz:String(cur.tz)})})
    .then(r=>r.json()).then(j=>{
      const m=document.getElementById('wmsg');
      m.className='msg '+(j.ok?'ok':'err');
      m.textContent=j.ok?'&#10003; Saved! Restarting…':'&#10007; '+j.msg;
    }).catch(()=>{
      document.getElementById('wmsg').className='msg err';
      document.getElementById('wmsg').textContent='&#10007; Request failed';
    });
}
</script>)html";
    httpd_resp_send_chunk(req, SETTINGS_BODY, sizeof(SETTINGS_BODY)-1);
    httpd_resp_send_chunk(req, HTML_FOOTER, sizeof(HTML_FOOTER)-1);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t h_root(httpd_req_t* req) {
    if (g_provisioning) return h_setup_page(req);
    // In clock mode: redirect to settings unless path is exactly /
    const char* uri = req->uri;
    if (strcmp(uri, "/") == 0) return h_settings_page(req);
    // Catch-all: redirect to root (captive portal)
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://1.2.3.4/");
    httpd_resp_send(req, "", 0);
    return ESP_OK;
}

static esp_err_t h_setup_get(httpd_req_t* req) {
    return h_setup_page(req);
}

// ── Start web server ──────────────────────────────────────────────────────────

static httpd_handle_t prov_httpd = NULL;

static void _start_httpd(bool captive) {
    if (prov_httpd) return;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port       = 80;
    cfg.max_uri_handlers  = 10;
    cfg.uri_match_fn      = captive ? httpd_uri_match_wildcard : httpd_uri_match_wildcard;
    httpd_start(&prov_httpd, &cfg);

    httpd_uri_t r_root    = {"/",             HTTP_GET,  h_root,         NULL};
    httpd_uri_t r_setup   = {"/setup",        HTTP_GET,  h_setup_get,    NULL};
    httpd_uri_t r_scan    = {"/scan",         HTTP_GET,  h_scan,         NULL};
    httpd_uri_t r_save    = {"/save",         HTTP_POST, h_save_wifi,    NULL};
    httpd_uri_t r_api_g   = {"/api/settings", HTTP_GET,  h_api_get,      NULL};
    httpd_uri_t r_api_p   = {"/api/settings", HTTP_POST, h_api_post,     NULL};
    httpd_uri_t r_catch   = {"/*",            HTTP_GET,  h_root,         NULL};

    httpd_register_uri_handler(prov_httpd, &r_root);
    httpd_register_uri_handler(prov_httpd, &r_setup);
    httpd_register_uri_handler(prov_httpd, &r_scan);
    httpd_register_uri_handler(prov_httpd, &r_save);
    httpd_register_uri_handler(prov_httpd, &r_api_g);
    httpd_register_uri_handler(prov_httpd, &r_api_p);
    httpd_register_uri_handler(prov_httpd, &r_catch);
}

// Call from on_boot when provisioning is needed
inline void prov_start() {
    g_provisioning = true;
    esp_wifi_set_mode(WIFI_MODE_APSTA);   // stay APSTA throughout provisioning
    vTaskDelay(pdMS_TO_TICKS(500));       // let STA interface init
    prov_set_ap_ip();
    prov_do_scan();                        // pre-scan before HTTP starts so /scan is ready
    xTaskCreate(prov_dns_task,   "prov_dns",  4096, NULL, 5, NULL);
    xTaskCreate(prov_scan_task,  "prov_scan", 4096, NULL, 4, NULL);
    _start_httpd(true);
}

// Call from on_boot when WiFi credentials exist (clock mode)
inline void settings_start() {
    g_provisioning = false;
    vTaskDelay(pdMS_TO_TICKS(200));
    prov_set_ap_ip();
    xTaskCreate(prov_dns_task, "settings_dns", 4096, NULL, 5, NULL);
    _start_httpd(false);
}
