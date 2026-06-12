# Firmware — Heltec Wireless Tracker

**Release:** v1.1.8 (`platformio.ini` → `-DFIRMWARE_VERSION_STRING`).

**Hardware:** [Heltec Wireless Tracker](https://heltec.org/project/wireless-tracker/) — ESP32‑S3, **8 MB** flash, UC6580 GNSS @ **115200**, ST7735 **160×80** TFT (SPI), WS2812 alert LED on GPIO **18**.

PlatformIO uses `board = heltec_wifi_lora_32_V3` as a close ESP32‑S3 target; confirm flash size and USB on your PCB.

| Env | Use |
|-----|-----|
| `heltec_wireless_tracker` | Current Tracker PCB (default) |
| `heltec_wireless_tracker_v1_0` | v1.0 pin map (`VEXT` GPIO **36** active LOW, GPS power GPIO **37**, backlight GPIO **45**) |

## Flash

Full step-by-step guide: **[`../docs/FLASHING.md`](../docs/FLASHING.md)** (first install, camera-data-only updates, troubleshooting).

```bash
pio run -e heltec_wireless_tracker -t upload
pio run -e heltec_wireless_tracker -t uploadfs
```

**Camera data update** (new `index/states.bin`, `cams/*.bin`, or `dataset_manifest.json` — no firmware change):

```bash
git pull   # if you pulled prebuilt data from this repo
cd firmware
pio run -e heltec_wireless_tracker -t uploadfs
```

Flash **firmware** (`upload`) when `firmware/src/` or `platformio.ini` changed. Flash **filesystem** (`uploadfs`) whenever `firmware/data/` changed — including after `tools/build_dataset.py`. Nationwide camera packs are ~1.5 MB and fit the 8 MB partition’s LittleFS slot (`partitions_tbeam_8mb.csv`).

## Pins (`src/config.h`)

- **VEXT** GPIO **3**, active **HIGH** (v1.0: GPIO **36**, active **LOW**)
- **GNSS** UART RX **33** / TX **34** @ **115200** (UC6580, NMEA only)
- **ST7735** SPI: CS **38**, DC **39**, MOSI **42**, SCK **41**, RST **40**, backlight **21**
- **WS2812** alert bar GPIO **18**

## Display

ST7735 (`INITR_MINI160x80_PLUGIN`, rotation **1**) on hardware SPI. Boot splash from `src/boot_logo_tracker_bitmap.h` (regenerate with `tools/image_to_boot_bitmap.py --stem TrackerBoot --width 160 --height 80`).

Operational screens use a dark grey **dashboard strip** (satellites, speed). Idle view repaints only when fields change to reduce TFT flicker.

If the TFT stays black: use a data USB cable, confirm you flashed **`heltec_wireless_tracker`**, and try **`heltec_wireless_tracker_v1_0`** if you have an early PCB.

## Camera data

LittleFS layout:

```
data/
  cams/{ST}.bin       # one pack per state (+ DC)
  index/states.bin    # buffered state bboxes + centroids (required for nationwide lookup)
  index/dataset_manifest.json
```

Rebuild from source GeoJSON: see [`../tools/README.md`](../tools/README.md).

## Simulation

Set `-DSIMULATION_MODE=1` in `platformio.ini` for desk testing without GPS.

## Roadmap

Planned Tracker features (OTA, etc.): [`../docs/HELTEC_TRACKER_ROADMAP.md`](../docs/HELTEC_TRACKER_ROADMAP.md).
