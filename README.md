# Flocku

Offline ALPR **awareness** for the **[Heltec Wireless Tracker](https://heltec.org/project/wireless-tracker/)** (ESP32‑S3, UC6580 GNSS, ST7735 TFT, WS2812 alert bar).

From a GPS fix the firmware resolves your U.S. state, loads the matching onboard camera database from **LittleFS**, and warns when mapped Flock ALPR cameras are nearby — with no cloud lookup at runtime.

**Firmware:** v1.1.8 (`heltec_wireless_tracker` env).  
**Camera data:** nationwide dataset (50 states + DC) under `firmware/data/cams/` — see `firmware/data/index/dataset_manifest.json`.

**Flashing:** first install needs firmware + filesystem. **Camera-only updates** (new `states.bin`, `cams/*.bin`, or manifest) need **`uploadfs` only** — see **[`docs/FLASHING.md`](docs/FLASHING.md)**.

## Quick start

```bash
cd firmware
pio run -e heltec_wireless_tracker -t upload
pio run -e heltec_wireless_tracker -t uploadfs
```

**v1.0 Tracker PCB** (different VEXT / GPS power pins):

```bash
pio run -e heltec_wireless_tracker_v1_0 -t upload
pio run -e heltec_wireless_tracker_v1_0 -t uploadfs
```

## How it works

- **Boot:** fox splash on the TFT while GNSS acquires a fix.
- **State lookup:** opens `/cams/{ST}.bin` for the state you are in.
- **Spatial scan:** nearby cameras within a configurable look-ahead, not a statewide dump.
- **Alerts:** classifies encounters (ahead, left, right, intersection-style) and drives on-screen copy plus the WS2812 LED bar.

See [`firmware/README.md`](firmware/README.md) for pins, display notes, and GPS flags.

## Rebuild camera data

Prebuilt binaries ship in `firmware/data/`. To refresh from DeFlock’s public CDN:

```bash
pip install -r tools/requirements.txt
python3 tools/fetch_deflock_geojson.py --us --flock-only -o tools/data/us_flock_cameras.geojson
python3 tools/build_dataset.py \
  --input tools/data/us_flock_cameras.geojson \
  --states tools/data/ne_110m_admin_1_states_provinces.geojson \
  --output firmware/data
cd firmware && pio run -e heltec_wireless_tracker -t uploadfs
```

Respect **ODbL** for OSM-derived inputs — see [`docs/DATA_ATTRIBUTION.md`](docs/DATA_ATTRIBUTION.md).

## Repo layout

| Path | Role |
|------|------|
| [`firmware/`](firmware/README.md) | PlatformIO project — **Heltec Wireless Tracker only** |
| [`firmware/data/`](firmware/data/index/dataset_manifest.json) | LittleFS camera packs (`cams/*.bin`) + state index |
| [`tools/`](tools/README.md) | Python pipeline to rebuild `firmware/data/` |
| [`docs/`](docs/) | **[Flashing guide](docs/FLASHING.md)**, OTA roadmap, data attribution |

## Disclaimer

Flocku is an **awareness** tool, not legal advice. Camera locations come from community-maintained maps and may be incomplete or outdated. You are responsible for safe, lawful use while driving.
