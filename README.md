# Waveshare Clock — by GGDM

A 7-segment LCD clock for the Waveshare ESP32-S3 1.47" display, built with ESPHome.
Features hand-drawn segment animations, six color themes, auto day/night mode, captive-portal WiFi setup, and a live settings web page.

[![X (Twitter)](https://img.shields.io/badge/@ggdm-000000?logo=x)](https://x.com/ggdm)
[![GitHub](https://img.shields.io/badge/DaveMath%2FWaveshare--Clock--GGDM-181717?logo=github)](https://github.com/DaveMath/Waveshare-Clock-GGDM)

---

## Hardware

### Main Board

**ESP32-S3 Development Board with 1.47" LCD Screen**  
172×320, 262K Color Display, built-in WS2812 RGB LED

> [Buy on Amazon →](https://www.amazon.com/dp/B0DJZG3WCL?tag=gadgetguydavemat-20)

| Pin | Function |
|-----|----------|
| GPIO42 | Display CS |
| GPIO41 | Display DC |
| GPIO39 | Display RST |
| GPIO40 | SPI CLK |
| GPIO45 | SPI MOSI |
| GPIO48 | Backlight PWM |
| GPIO38 | WS2812 RGB LED (GRB) |
| GPIO0  | User Button (BOOT) |

Display driver: ST7789V · Resolution: 172×320 px · Rotated 90° → landscape 320×172

---

## Features

- **7-segment display** — HH:MM:SS drawn as individual LVGL rectangles with per-segment slide animation (no bitmapped font); seconds use the same animated style as hours and minutes
- **Seconds arc ring** — 270° progress gauge around the seconds digit display
- **Seconds progress bar** across the bottom
- **Full spelled-out date** — "Wednesday, September 24"
- **Six color themes** — Primary, Pastel, Neon, Neutral, Warm, Cool — each with day and night variants
- **Auto day/night** — switches at 06:00 (dawn) and 20:00 (dusk); can be overridden via web settings
- **12-hour / 24-hour** toggle
- **10 timezones** — cycle with the physical button
- **WS2812 LED** — gentle breathe in current theme accent color
- **Captive-portal WiFi setup** — no app required
- **Always-on settings web page** at `http://1.2.3.4` via the `Clock-By-GGDM` AP

---

## First Boot (WiFi Setup)

1. Power on the device — the screen shows **CLOCK SETUP** with instructions
2. On your phone, connect to WiFi network: **`Clock-By-GGDM`** (open, no password)
3. Open a browser and go to **`http://1.2.3.4`**
4. Select your WiFi network, enter the password, and pick your timezone
5. Tap **Save & Connect** — the clock restarts and begins syncing time via NTP

WiFi credentials and timezone are stored in ESP-IDF NVS (non-volatile storage) and survive OTA firmware updates.

---

## Button Controls (GPIO0 / BOOT)

| Press | Action |
|-------|--------|
| Short press — first tap | Show current timezone + "Hold for Colors" hint |
| Short press — tap again while toast is showing | Cycle to next timezone |
| Hold ≥ 1.5 s | Live color theme preview — cycles every 2 s while held; releases on the current theme |

**Short press flow:**
1. First tap — shows the current timezone and a "Hold for Colors" hint at the bottom of the screen
2. Tap again within 2.5 s — advances to the next timezone (repeat to keep cycling)

**Hold flow:**
1. Press and hold past 1.5 s — the theme changes automatically every 2 seconds so you can see each one live
2. Release — the clock stays on whatever theme is showing at the moment you let go

---

## Web Settings (always accessible)

While the clock is running, reconnect your phone to **`Clock-By-GGDM`** and visit `http://1.2.3.4` to:

- Pick a **color theme** (Primary / Pastel / Neon / Neutral / Warm / Cool)
- Set **Night Mode** — Auto / Always Day / Always Night
- Switch between **12-hour and 24-hour** time
- Change the **timezone**
- Reconfigure **WiFi** credentials

Changes apply to the clock immediately (no restart required).

---

## Color Themes

| Theme | Day | Night |
|-------|-----|-------|
| **Primary** | Cream + deep navy | Near-black + sky blue |
| **Pastel** | Blush + mauve | Midnight + rose |
| **Neon** | Black + lime green | Black + cyan |
| **Neutral** | Warm grey + charcoal | Dark grey + silver |
| **Warm** | Parchment + amber | Charcoal + orange |
| **Cool** | Soft white + teal | Deep blue + cyan |

---

## Build & Flash

Requires [ESPHome](https://esphome.io) 2025.2 or later.

```bash
# Compile
python3 -m esphome compile waveshare-clock.yaml

# Flash (replace port as needed)
python3 -m esphome upload --device /dev/cu.usbmodem1201 waveshare-clock.yaml

# Monitor logs
python3 -m esphome logs --device /dev/cu.usbmodem1201 waveshare-clock.yaml
```

### secrets.yaml (for API / OTA only — WiFi is set via the portal)

```yaml
api_key: "your-esphome-api-key="
ota_password: "your-ota-password"
```

---

## Project Structure

```
waveshare-clock.yaml   ESPHome main config
clock_helper.h         7-segment logic, theme definitions, timezone data
provisioning.h         WiFi setup portal + settings web server
qr_helper.h            QR code rendering via LVGL draw callback
secrets.yaml           API and OTA credentials (not committed)
```

---

## Credits

Built by [@ggdm](https://x.com/ggdm) · [GitHub](https://github.com/DaveMath/Waveshare-Clock-GGDM)

DSEG7 font by [keshikan](https://github.com/keshikan/DSEG) (SIL OFL 1.1)
