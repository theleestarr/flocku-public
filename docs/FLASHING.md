# Flashing the Heltec Wireless Tracker

## Prerequisites

- [Heltec Wireless Tracker](https://heltec.org/project/wireless-tracker/) connected over **USB** (data-capable cable)
- [PlatformIO](https://platformio.org/) (`pio` on your PATH)
- Clone this repo and open a terminal at the repo root

Check the board appears (example port name — yours may differ):

```bash
pio device list
# e.g. /dev/cu.usbmodem101  (macOS)
# e.g. /dev/ttyACM0           (Linux)
```

Pick the PlatformIO environment that matches your PCB:

| Board | Env |
|-------|-----|
| Current Tracker | `heltec_wireless_tracker` |
| v1.0 Tracker (older pin map) | `heltec_wireless_tracker_v1_0` |

Set `ENV=heltec_wireless_tracker` below, or swap for `_v1_0` if needed.

Optional — fix the serial port when auto-detect picks the wrong device:

```bash
export UPLOAD_PORT=/dev/cu.usbmodem101   # macOS example
# pio run ... --upload-port "$UPLOAD_PORT"
```

---

## First-time install (firmware + camera data)

Flash **both** the application and the LittleFS filesystem. The filesystem holds nationwide camera packs and the state index.

```bash
cd firmware
pio run -e heltec_wireless_tracker -t upload
pio run -e heltec_wireless_tracker -t uploadfs
```

The board resets when each step finishes. After `uploadfs`, you should have:

- Firmware **v1.1.8** (shown on the idle screen footer after GPS lock)
- Camera dataset under LittleFS — see `firmware/data/index/dataset_manifest.json` for the shipped **`dataset_version`**

---

## Camera data update only (`states.bin`, `cams/`, manifest)

When this repo updates **camera data** but **not** firmware, you only need **`uploadfs`**. That rewrites the LittleFS partition with:

| Path on device | Role |
|----------------|------|
| `/index/states.bin` | **Required.** Buffered bbox + state centroid per jurisdiction (51 entries). GPS → which `ST.bin` to open. Without this file the device falls back to a **6-state west-coast subset only**. |
| `/cams/{ST}.bin` | Per-state clustered camera packs (all 50 states + DC) |
| `/index/dataset_manifest.json` | Dataset version and per-state checksums |

**Always flash the full `firmware/data/` tree with `uploadfs`.** Do not copy individual `cams/*.bin` files without also updating `index/states.bin` — border overlap resolution and nationwide state lookup depend on both.

Each state row in `states.bin` uses a **25-mile buffered** bounding box (avoids dead zones near state lines) plus the true state **centroid** for disambiguation when multiple boxes overlap.

**You do not need `-t upload`** unless the release notes also say the firmware binary changed.

### After `git pull` (prebuilt data in the repo)

```bash
git pull
cd firmware
pio run -e heltec_wireless_tracker -t uploadfs
```

Typical runtime: ~30 seconds. Firmware already on the device is unchanged.

### After rebuilding data locally (`tools/build_dataset.py`)

If you refreshed GeoJSON and rebuilt `firmware/data/` yourself:

```bash
python3 tools/build_dataset.py \
  --input tools/data/us_flock_cameras.geojson \
  --states tools/data/ne_110m_admin_1_states_provinces.geojson \
  --output firmware/data
cd firmware
pio run -e heltec_wireless_tracker -t uploadfs
```

Each rebuild bumps **`dataset_version`** in `firmware/data/index/dataset_manifest.json`. Compare that number before and after to confirm the flash wrote new data.

---

## When to flash firmware again (`upload`)

Re-run **both** steps if any of these changed in the release:

- C++ source under `firmware/src/`
- `platformio.ini` or `partitions_tbeam_8mb.csv`
- PlatformIO library versions affecting the binary

```bash
cd firmware
pio run -e heltec_wireless_tracker -t upload
pio run -e heltec_wireless_tracker -t uploadfs
```

Camera-only repo updates → **`uploadfs` only** (previous section).

---

## Verify

1. **Manifest on disk (before flash):**  
   `cat firmware/data/index/dataset_manifest.json | head` — note `dataset_version` and `built_at`.

2. **On device:** power cycle, wait for GPS lock, drive or simulate in a state with cameras. You should get alerts near mapped Flock ALPR nodes instead of “no data file” for that state.

3. **Missing state file screen** after a data update usually means `uploadfs` did not run, the wrong `ENV` was used, or the USB cable/port failed mid-write. Re-run `uploadfs` with `--upload-port` set explicitly.

---

## Troubleshooting

| Symptom | Likely fix |
|---------|------------|
| TFT stays black | Wrong env — use `heltec_wireless_tracker` or try `_v1_0` |
| “No data file” with `--` after GPS lock | Position outside all state boxes, or **`states.bin` missing/stale** — run full `uploadfs` |
| Wrong state near a border | Reflash **`index/states.bin`** from a current build (centroid tie-break needs v2 format) |
| `uploadfs` fails / timeout | Different USB port/cable; set `--upload-port` |
| Alerts stale after pull | You ran `upload` only — camera data lives on LittleFS; run **`uploadfs`** |
| **Persistent** "NO DATA FILE / `cams/{ST}.bin` missing" for **every** state, unchanged after re-running `uploadfs` | Partition table at `0x8000` is missing the `littlefs` label — see **[Recovery: partition-table / label mismatch](#recovery-partition-table--label-mismatch)** below |

### Recovery: partition-table / label mismatch

If **every** state reads "missing" right after GPS lock — and re-running `uploadfs` changes nothing — the camera data is almost certainly intact, but the **partition table at `0x8000`** does not define the `littlefs`-labeled partition the firmware mounts by name (`camera_store.cpp` → `LittleFS.begin(false, "/littlefs", 10, "littlefs")`).

This can happen on a unit that was provisioned or previously flashed with a **different partition table** (e.g. a stock 8 MB layout): the app and the LittleFS image get written at the offsets in `partitions_tbeam_8mb.csv` (`0x10000` and `0x210000`), but the table at `0x8000` never got the `littlefs` label. **Re-running `uploadfs` cannot fix this** — it rewrites the filesystem at `0x210000` and never touches the `0x8000` table.

Either fix works:

**A. One-time full firmware flash** — rewrites the `0x8000` table from this repo's CSV, leaves your camera data in place:

```bash
cd firmware
pio run -e heltec_wireless_tracker -t upload
```

**B. Write only the partition table** — one 4 KB sector; firmware and camera data untouched. Build `partitions.bin` from `partitions_tbeam_8mb.csv`, then:

```bash
esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 460800 \
  write_flash 0x8000 partitions.bin
```

Use your own port (`pio device list`); `/dev/ttyACM0` is just an example. Power-cycle after either fix and the "missing" screen should clear.

---

## Summary

| Situation | Command |
|-----------|---------|
| New board | `upload` then `uploadfs` |
| Repo update to `firmware/data/**` only | **`uploadfs` only** |
| Repo update to firmware source | `upload` then `uploadfs` |
| Local dataset rebuild | `uploadfs` (after `build_dataset.py`) |
