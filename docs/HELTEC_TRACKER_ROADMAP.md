# Heltec Wireless Tracker — development roadmap

Planned work for the **`heltec_wireless_tracker`** PlatformIO environment (Heltec Wireless Tracker, ESP32-S3, ST7735, UC6580). Other board envs may share some items later; this file tracks **Tracker-first** plans. Build and pin detail: **[`../firmware/README.md`](../firmware/README.md)**.

---

## ESP32-S3 OTA firmware update system

| Field | Value |
|--------|--------|
| **Priority** | Medium |
| **Phase** | Firmware / device update infrastructure |
| **Status** | Proposed |
| **Dependencies** | Wi-Fi provisioning, OTA partition layout (`ota_0`, `ota_1`, `otadata`), firmware versioning scheme, hosted firmware repository |

### Goal

Enable ESP32-S3 boards to automatically check for, download, and install newer firmware versions over Wi-Fi.

### Description

On boot, the ESP32-S3 will connect to Wi-Fi and check a public GitHub-hosted firmware **manifest**. If the manifest describes a firmware version **newer** than the one currently installed, the device will download the new firmware binary, verify it, install it to the **inactive** OTA partition, and reboot into the updated firmware.

### Key requirements

- ESP32-S3 connects to Wi-Fi on boot (or on a defined schedule / user action — TBD with provisioning design).
- Device stores and reports its **current firmware version** (see existing `FIRMWARE_VERSION_STRING` / build flags).
- A public GitHub repository (or equivalent) hosts:
  - **`manifest.json`** (or versioned manifest URL),
  - **versioned** `*.bin` firmware artifacts.
- Device compares installed version against the **online** manifest version.
- If newer firmware exists:
  - download firmware binary over **HTTPS**;
  - verify **checksum** (and optionally signature in production);
  - write to the OTA **inactive** partition;
  - reboot into the updated app; use **ESP-IDF / Arduino OTA rollback** so a failed boot can fall back and avoid bricking.
- **Rollback protection:** failed or unverified updates must not leave the device unbootable (use dual OTA slots, validate before `esp_ota_set_boot_partition`, and/or confirm new image before switching).

### Manifest example

```json
{
  "version": "1.0.3",
  "board": "esp32s3",
  "firmware_url": "https://raw.githubusercontent.com/org/repo/main/firmware/esp32s3-v1.0.3.bin",
  "sha256": "..."
}
```

*(Tracker builds may use a `board` or `product` field such as `heltec_wireless_tracker` once the manifest schema is fixed.)*

### Implementation notes

- Use **HTTPS** for OTA download (certificate validation / bundle as required by the stack).
- Host binaries via **raw GitHub URLs** or **GitHub Releases** for development and prototypes.
- Partition table must include **OTA slots** (e.g. `ota_0`, `ota_1`) and **`otadata`**; align with PlatformIO `partitions*.csv` for the Tracker env.
- Validate firmware with **SHA-256** at minimum before flashing the inactive partition.
- Consider **signed** firmware for production trust.
- GitHub is acceptable for **development** and **prototype** deployments; **production** may move binaries to S3, Cloudflare R2, or CDN-backed hosting for scale, bandwidth, and SLAs.

---

*Add further Tracker roadmap items below as separate `##` sections.*
