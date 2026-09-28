# Waveshare Clock — GGDM

> *A throwback 7-segment clock that actually moves.*

[![X (Twitter)](https://img.shields.io/badge/@ggdm-000000?logo=x)](https://x.com/ggdm)
[![GitHub](https://img.shields.io/badge/DaveMath%2FWaveshare--Clock--GGDM-181717?logo=github)](https://github.com/DaveMath/Waveshare-Clock-GGDM)
[![ESPHome](https://img.shields.io/badge/ESPHome-2025.2-blue)](https://esphome.io)

---

## The Design Story

This started as a simple question: *what if a 7-segment display could feel alive?*

LCD digit clocks have been on desks and dashboards since the 1970s. The form is completely solved — two digits, a colon, repeat. But the execution has always been static. A digit changes, and it just *is* the new digit. No transition. No memory of what it was before. Snap-cut to the next state, forever.

The GGDM clock rejects that. Every segment is a living rectangle. When a digit flips from 6 to 7, you don't just see a 7 — you watch four segments retract and one grow, each on its own eased animation curve, timed to overlap so the change reads as transformation rather than replacement. The display remembers that a 6 *became* a 7. That distinction is the whole project.

Built as a **design study in UX with minimal hardware** — one physical button, a 1.47" panel, a WiFi radio, and nothing else. No touchscreen. No rotary encoder. No app. Every configuration path runs through either the single button or a web page the device hosts itself. The constraint was intentional: *if a feature can't be surfaced through one button or one URL, it doesn't belong in the design.*

The boot sequence is a signature moment — the display wakes showing **GGdH** in morphing segments (the maker's initials, rendered in 7-seg), adds colons, then counts up digit-by-digit to **12:34**, each transition demonstrating the morph engine before the clock ever shows real time. It's a calibration and a statement at once.

Six themes — from neon-on-black to warm parchment — shift automatically between day and night variants at dawn and dusk. The current timezone is always visible in the header. A circular arc counts the seconds. Nothing is ever more than a glance away.

**Built by GGDM via Claude Code as a UX design study.** The firmware is open; the philosophy is the point.

---

## Hardware

**Waveshare ESP32-S3 1.47" LCD Dev Board**  
172×320 px · 262K color · ST7789V · Built-in WS2812 RGB LED

> [Buy on Amazon →](https://www.amazon.com/dp/B0DJZG3WCL?tag=gadgetguydavemat-20)

| Pin | Function |
|-----|----------|
| GPIO42 | Display CS |
| GPIO41 | Display DC |
| GPIO39 | Display RST |
| GPIO40 | SPI CLK |
| GPIO45 | SPI MOSI |
| GPIO48 | Backlight PWM (LEDC) |
| GPIO38 | WS2812 RGB LED |
| GPIO0  | User Button (BOOT) |

---

## Features

### The Morph Engine
Each digit is seven independent LVGL rectangles. Horizontal segments grow and shrink from their right edge; vertical segments animate height from the anchor point nearest the center. A `seg_set(digit, value, animated)` call kicks off parallel LVGL animations — 260ms per segment — timed so a complex transition like `8→1` reads as a graceful collapse rather than a chaotic flicker.

### Display
- **Animated 7-segment digits** — HH:MM with per-segment morph animation
- **Animated seconds** — same morph style, live digit update every second
- **Seconds arc ring** — 270° progress gauge around the seconds cluster
- **Seconds bar** — full-width progress strip at the bottom
- **Full date header** — "Sunday, September 28  -  Pacific (PT)" — timezone always visible

### Themes
Six palettes, each with a day and night variant:

| Theme | Day | Night |
|-------|-----|-------|
| **Primary** | Cream + deep navy | Near-black + sky blue |
| **Pastel** | Blush + mauve | Midnight + rose |
| **Neon** | Black + lime green | Black + cyan |
| **Neutral** | Warm grey + charcoal | Dark grey + silver |
| **Warm** | Parchment + amber | Charcoal + orange |
| **Cool** | Soft white + teal | Deep blue + cyan |

Auto day/night switches at 06:00 and 20:00. Override available via web settings.

### Timezones
10 US/global zones, cycle with the button. Current zone is always shown in the date bar — no digging into menus to remember what you set.

### Connectivity
- **Captive-portal setup** — connect to `Clock-By-GGDM` AP, open `http://1.2.3.4`, done
- **Always-on settings page** — accessible while the clock is running, no restart required
- **APSTA mode** — the provisioning AP stays up while the clock is connected to your home network
- Credentials and settings stored in ESP-IDF NVS; survive OTA updates

### Boot Sequence
1. All segments flash on (segment test)
2. Morph to **GGdH** — maker's initials in 7-segment form
3. Colons blink on/off
4. Morph to `00:00`, then count up digit-by-digit to `12:34` — showcasing the transition engine
5. Clock takes over once NTP syncs

---

## One Button, Everything

The entire physical interface is a single button (GPIO0 / BOOT):

| Gesture | Action |
|---------|--------|
| Short press | Show current timezone + "Hold for Colors" hint |
| Short press again (within 2.5 s) | Cycle to next timezone |
| Hold ≥ 1.5 s | Live theme preview — cycles every 2 s while held |
| Release from hold | Lock in the current theme |

The button also recovers the backlight if it dims. No dedicated brightness control needed.

---

## Web Settings

Reconnect to **`Clock-By-GGDM`** and visit `http://1.2.3.4`:

- Choose **color theme**
- Set **Night Mode** — Auto / Always Day / Always Night
- Toggle **12h / 24h**
- Change **timezone**
- Update **WiFi credentials**

Changes apply immediately. No restart.

---

## Build & Flash

Requires [ESPHome](https://esphome.io) 2025.2+.

```bash
# Compile
python3 -m esphome compile waveshare-clock.yaml

# Flash (replace port as needed)
python3 -m esphome upload --device /dev/cu.usbmodem1201 waveshare-clock.yaml

# Monitor
python3 -m esphome logs --device /dev/cu.usbmodem1201 waveshare-clock.yaml
```

### secrets.yaml (optional — for ESPHome API/OTA only)

```yaml
api_key: "your-esphome-api-key="
ota_password: "your-ota-password"
```

WiFi credentials are configured through the captive portal, not secrets.yaml.

---

## Project Structure

```
waveshare-clock.yaml   ESPHome config, LVGL widget tree, all clock logic
clock_helper.h         Segment animation engine, themes, timezones, globals
provisioning.h         Captive portal + always-on settings HTTP server
qr_helper.h            QR code rendering via LVGL draw callback
secrets.yaml           API/OTA credentials — gitignored, never committed
```

---

## Stack

- **ESPHome** 2025.2.2 — config-driven firmware framework
- **ESP-IDF** v5 — underlying RTOS and hardware drivers
- **LVGL** 8.4.0 — UI engine; rectangle primitives + animation system
- **FreeRTOS** — DNS task, WiFi scan task, cross-task volatile state
- **ESP-IDF NVS** — persistent settings storage
- **ST7789V / ili9xxx** — SPI display driver

---

## Credits

Built by [GGDM](https://x.com/ggdm) · Design study in constrained UX · Firmware via [Claude Code](https://claude.ai/claude-code)

DSEG7 font by [keshikan](https://github.com/keshikan/DSEG) (SIL OFL 1.1)
