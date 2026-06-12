#pragma once

#include <cstdint>

// Shown on OLED boot splash (`DisplayManager::show_boot_acquiring`). `heltec_wireless_tracker` overrides in platformio.ini (v1.1.0).
#ifndef FIRMWARE_VERSION_STRING
#define FIRMWARE_VERSION_STRING "1.0.1"
#endif

#if defined(FLOCKU_BOARD_LILYGO_PAGER)
// LilyGo T-LoRa Pager (ESP32-S3, ST7796 222×480, u-blox MIA-M10Q, XL9555 IO expander).
// Pin map: Meshtastic variants/esp32s3/tlora-pager/variant.h + bmorcelli/Launcher.
#ifndef PAGER_POWER_ON_PIN
#define PAGER_POWER_ON_PIN 15
#endif
#ifndef PAGER_I2C_SDA
#define PAGER_I2C_SDA 3
#endif
#ifndef PAGER_I2C_SCL
#define PAGER_I2C_SCL 2
#endif
#ifndef PAGER_IO_EXPANDER_ADDR
#define PAGER_IO_EXPANDER_ADDR 0x20
#endif
#ifndef PAGER_EXPANDS_DRV_EN
#define PAGER_EXPANDS_DRV_EN 0
#endif
#ifndef PAGER_EXPANDS_AMP_EN
#define PAGER_EXPANDS_AMP_EN 1
#endif
#ifndef PAGER_EXPANDS_LORA_EN
#define PAGER_EXPANDS_LORA_EN 3
#endif
#ifndef PAGER_EXPANDS_GPS_EN
#define PAGER_EXPANDS_GPS_EN 4
#endif
#ifndef PAGER_EXPANDS_KB_EN
#define PAGER_EXPANDS_KB_EN 8
#endif
#ifndef PAGER_EXPANDS_GPIO_EN
#define PAGER_EXPANDS_GPIO_EN 9
#endif
#ifndef PAGER_EXPANDS_SD_PULLEN
#define PAGER_EXPANDS_SD_PULLEN 11
#endif
#ifndef PAGER_EXPANDS_SD_EN
#define PAGER_EXPANDS_SD_EN 12
#endif
#ifndef PAGER_TFT_CS_PIN
#define PAGER_TFT_CS_PIN 38
#endif
#ifndef PAGER_SDCARD_CS_PIN
#define PAGER_SDCARD_CS_PIN 21
#endif
#ifndef PAGER_LORA_CS_PIN
#define PAGER_LORA_CS_PIN 36
#endif
#ifndef PAGER_TFT_BL_PIN
#define PAGER_TFT_BL_PIN 42
#endif
#ifndef PAGER_TFT_ROTATION
#define PAGER_TFT_ROTATION 3
#endif
#ifndef GPS_UART_NUM
#define GPS_UART_NUM 1
#endif
#ifndef GPS_RX_PIN
#define GPS_RX_PIN 4
#endif
#ifndef GPS_TX_PIN
#define GPS_TX_PIN 12
#endif
#ifndef GPS_UBLOX_INITIAL_BAUD
#define GPS_UBLOX_INITIAL_BAUD 38400
#endif
#ifndef GPS_UBLOX_RUNTIME_BAUD
#define GPS_UBLOX_RUNTIME_BAUD 38400
#endif
#ifndef GPS_UBLOX_USE_PMTK
#define GPS_UBLOX_USE_PMTK 1
#endif
#ifndef GPS_UBLOX_USE_5HZ
#define GPS_UBLOX_USE_5HZ 1
#endif
#ifndef GPS_UBLOX_UPGRADE_BAUD
#define GPS_UBLOX_UPGRADE_BAUD 0
#endif
#ifndef GPS_BAUD
#define GPS_BAUD GPS_UBLOX_INITIAL_BAUD
#endif
#ifndef GPS_WARMUP_MS
#define GPS_WARMUP_MS 300
#endif
#ifndef BQ27220_I2C_ADDR
#define BQ27220_I2C_ADDR 0x55
#endif
#ifndef BQ27220_DESIGN_CAPACITY
#define BQ27220_DESIGN_CAPACITY 1500
#endif
#ifndef LED_ALERT_PIN
#define LED_ALERT_PIN (-1)
#endif
#ifndef LED_ALERT_USE_WS2812
#define LED_ALERT_USE_WS2812 0
#endif
#ifndef PAGER_TFT_FULL_REDRAW_MIN_MS
#define PAGER_TFT_FULL_REDRAW_MIN_MS 500u
#endif
#ifndef PAGER_SPLASH_MAX_MS
#define PAGER_SPLASH_MAX_MS 10000u
#endif
#define TFT_FULL_REDRAW_MIN_MS PAGER_TFT_FULL_REDRAW_MIN_MS

#elif defined(FLOCKU_BOARD_HELTEC_TRACKER)
// Heltec Wireless Tracker (ESP32-S3 + UC6580 GNSS + ST7735S 160×80 TFT) — pins align with Meshtastic
// `variants/esp32s3/heltec_wireless_tracker/variant.h`.
// Display uses SPI ST7735 (not I²C OLED). VEXT powers GPS/LNA/TFT rail.
#if defined(FLOCKU_HELTEC_TRACKER_V1_0)
#define HELTEC_VEXT_PIN 36  // v1.0: active LOW
#else
#define HELTEC_VEXT_PIN 3   // v1.1+: active HIGH
#define HELTEC_VEXT_ACTIVE_HIGH 1
#endif
#ifndef GPS_UART_NUM
#define GPS_UART_NUM 1
#endif
#ifndef GPS_RX_PIN
#define GPS_RX_PIN 33
#endif
#ifndef GPS_TX_PIN
#define GPS_TX_PIN 34
#endif
#ifndef GPS_BAUD
#define GPS_BAUD 115200
#endif
#ifndef GPS_WARMUP_MS
#define GPS_WARMUP_MS 500
#endif
#ifndef GPS_RESET_PIN
#define GPS_RESET_PIN 35
#endif
// Heltec Wireless Tracker v1.0 only: GPIO 37 must be LOW to enable GNSS (v1.1 uses VEXT pin 3).
// Set via `heltec_wireless_tracker_v1_0` PlatformIO env — do not enable on v1.1 boards.
#if defined(FLOCKU_HELTEC_TRACKER_V1_0)
#define GPS_POWER_EN_PIN 37  // active LOW — required for GNSS on v1.0
#define ST7735_BL_PIN 45
#endif
// ST7735S TFT — SPI pins match Meshtastic `heltec_wireless_tracker` (firmware uses hardware SPI).
#ifndef ST7735_CS
#define ST7735_CS 38
#endif
#ifndef ST7735_DC
#define ST7735_DC 40
#endif
#ifndef ST7735_MOSI
#define ST7735_MOSI 42
#endif
#ifndef ST7735_SCK
#define ST7735_SCK 41
#endif
#ifndef ST7735_RST
#define ST7735_RST 39
#endif
#ifndef ST7735_BL_PIN
#define ST7735_BL_PIN 21
#endif
/// SPI clock to the ST7735 (Hz). 40 MHz can show line artifacts with long traces; 20 MHz is safer.
#ifndef ST7735_SPI_MAX_HZ
#define ST7735_SPI_MAX_HZ 20000000u
#endif
/// Minimum time between full-screen TFT redraws (reduces beat with LCD scan / tearing).
#ifndef ST7735_FULL_REDRAW_MIN_MS
#define ST7735_FULL_REDRAW_MIN_MS 120u
#endif
#define TFT_FULL_REDRAW_MIN_MS ST7735_FULL_REDRAW_MIN_MS
/// WS2812 / NeoPixel data — GPIO 4 is free on Wireless Tracker (see Meshtastic variant; not LoRa/TFT/GPS).
#ifndef LED_ALERT_PIN
#define LED_ALERT_PIN 4
#endif
#ifndef LED_ALERT_USE_WS2812
#define LED_ALERT_USE_WS2812 1
#endif
#ifndef NEOPIXEL_COUNT
#define NEOPIXEL_COUNT 8
#endif
#ifndef NEOPIXEL_MAX_BRIGHTNESS
#define NEOPIXEL_MAX_BRIGHTNESS 50
#endif
/// Global strip brightness during idle Larson scanner (Adafruit_NeoPixel scale 0–255); 1 is minimum.
#ifndef NEOPIXEL_IDLE_BRIGHTNESS
#define NEOPIXEL_IDLE_BRIGHTNESS 1
#endif
/// Milliseconds per step along the 14-step Larson ping-pong (0→7→0); lower = faster sweep.
#ifndef NEOPIXEL_IDLE_LARSON_STEP_MS
#define NEOPIXEL_IDLE_LARSON_STEP_MS 90u
#endif
// Unused on this board (no I²C OLED); Wire is not required for the TFT path.
#ifndef OLED_SDA_PIN
#define OLED_SDA_PIN 17
#endif
#ifndef OLED_SCL_PIN
#define OLED_SCL_PIN 18
#endif

#elif defined(FLOCKU_BOARD_TBEAM) || defined(FLOCKU_BOARD_ESP32_WROVER_OLED)
// LilyGO TTGO T-Beam — OR generic ESP32‑WROVER dev board + wired SSD1306 (same defaults).
// UART / OLED pins match common Meshtastic / LilyGO wiring; override here if your OLED uses other I²C pins.
// Optional stackable OLED: I²C SDA/SCL; RST often not wired (use -1).
#ifndef GPS_UART_NUM
#define GPS_UART_NUM 1
#endif
#ifndef GPS_RX_PIN
#define GPS_RX_PIN 34
#endif
#ifndef GPS_TX_PIN
#define GPS_TX_PIN 12
#endif
// u-blox Neo: gps_manager sends PMTK at GPS_UBLOX_INITIAL_BAUD, then optionally switches to RUNTIME_BAUD.
#ifndef GPS_UBLOX_INITIAL_BAUD
#define GPS_UBLOX_INITIAL_BAUD 9600
#endif
#ifndef GPS_UBLOX_RUNTIME_BAUD
#define GPS_UBLOX_RUNTIME_BAUD 38400
#endif
#ifndef GPS_UBLOX_USE_PMTK
#define GPS_UBLOX_USE_PMTK 1
#endif
#ifndef GPS_UBLOX_USE_5HZ
#define GPS_UBLOX_USE_5HZ 1
#endif
// If 1, UART switches to GPS_UBLOX_RUNTIME_BAUD after PMTK251. Many modules/clones handle this;
// if yours never locks, leave 0 (stay at GPS_UBLOX_INITIAL_BAUD — typical 9600).
#ifndef GPS_UBLOX_UPGRADE_BAUD
#define GPS_UBLOX_UPGRADE_BAUD 0
#endif
#ifndef GPS_BAUD
#if GPS_UBLOX_USE_PMTK && GPS_UBLOX_UPGRADE_BAUD
#define GPS_BAUD GPS_UBLOX_RUNTIME_BAUD
#else
#define GPS_BAUD GPS_UBLOX_INITIAL_BAUD
#endif
#endif
#ifndef GPS_WARMUP_MS
#define GPS_WARMUP_MS 200
#endif
// No GPS_POWER_EN_PIN / GPS_STANDBY_PIN / GPS_RESET_PIN — gps_manager skips those when undefined.
#ifndef OLED_SDA_PIN
#define OLED_SDA_PIN 21
#endif
#ifndef OLED_SCL_PIN
#define OLED_SCL_PIN 22
#endif
#ifndef OLED_RST_PIN
#define OLED_RST_PIN (-1)
#endif
#ifndef OLED_I2C_ADDR
#define OLED_I2C_ADDR 0x3C
#endif
#ifndef LED_ALERT_PIN
#define LED_ALERT_PIN 4
#endif
#ifndef LED_ALERT_USE_WS2812
#define LED_ALERT_USE_WS2812 0
#endif

#else
// --- Heltec WiFi LoRa 32 V4 (ESP32‑S3) — Meshtastic `variants/esp32s3/heltec_v4/variant.h`

// --- VEXT: powers OLED + RF front‑end supply on Heltec V3/V4 class boards (active LOW = on) ---
#ifndef HELTEC_VEXT_PIN
#define HELTEC_VEXT_PIN 36
#endif

// --- UART L76K: must match `meshtastic/firmware` `GPS::createGps()` + `heltec_v4/variant.h` ---
// Variant defines GPS_TX_PIN(38) / GPS_RX_PIN(39) by *cable direction*; HardwareSerial::begin(rx,tx,…) uses:
//   rx_gpio = GPS_RX_PIN (39) = ESP UART **RX**, tx_gpio = GPS_TX_PIN (38) = ESP UART **TX**.
#ifndef GPS_UART_NUM
#define GPS_UART_NUM 1  // Meshtastic uses Serial1 for GPS on ESP32
#endif
#ifndef GPS_RX_PIN
#define GPS_RX_PIN 39
#endif
#ifndef GPS_TX_PIN
#define GPS_TX_PIN 38
#endif
#ifndef GPS_BAUD
#define GPS_BAUD 9600
#endif

// L76K power / sleep (from Meshtastic heltec_v4 variant.h)
#ifndef GPS_POWER_EN_PIN
#define GPS_POWER_EN_PIN 34
#endif
// When defined without GPS_POWER_EN_ACTIVE_HIGH, gps_manager drives LOW to enable (matches GPS_EN_ACTIVE LOW).
#ifndef GPS_STANDBY_PIN
#define GPS_STANDBY_PIN 40  // HIGH = force wake (allow sleep when LOW)
#endif
// Optional L76K hardware reset (same as Meshtastic — pulse active level GPS_RESET_MODE for >=100ms)
#ifndef GPS_RESET_PIN
#define GPS_RESET_PIN 42
#endif
#ifndef GPS_WARMUP_MS
#define GPS_WARMUP_MS 500  // variant.h uses 1000ms for I2C; UART path 500ms is enough after EN
#endif

// --- OLED: SSD1306/SSD1315‑class, Meshtastic `-D I2C_SDA=17 -D I2C_SCL=18 -D RESET_OLED=21` ---
#ifndef OLED_SDA_PIN
#define OLED_SDA_PIN 17
#endif
#ifndef OLED_SCL_PIN
#define OLED_SCL_PIN 18
#endif
#ifndef OLED_RST_PIN
#define OLED_RST_PIN 21
#endif
#ifndef OLED_I2C_ADDR
#define OLED_I2C_ADDR 0x3C
#endif

// Status LED: Meshtastic `LED_POWER=35` for Heltec V4 OLED builds — UART uses RX=39 / TX=38, not this pin.
#ifndef LED_ALERT_USE_WS2812
#define LED_ALERT_USE_WS2812 0
#endif
#ifndef LED_ALERT_PIN
#define LED_ALERT_PIN 35
#endif

#endif  // !(TBEAM || ESP32_WROVER_OLED)

#if defined(FLOCKU_BOARD_HELTEC_TRACKER) || defined(FLOCKU_BOARD_LILYGO_PAGER)
#define FLOCKU_TFT_TRACKER_UI 1
#endif

// Boot: max wait for first GPS fix before continuing to main loop (shows GPS SEARCHING there).
#ifndef GPS_BOOT_FIX_TIMEOUT_MS
#define GPS_BOOT_FIX_TIMEOUT_MS 120000u
#endif
// Optional fix quality (u-blox / NMEA). 0 = disabled (only fresh lat/lon required).
#ifndef GPS_FIX_MAX_HDOP
#define GPS_FIX_MAX_HDOP 0.0f
#endif
#ifndef GPS_FIX_MIN_SATELLITES
#define GPS_FIX_MIN_SATELLITES 0u
#endif

// --- Tunables (mirrors brief; later: /config/settings.bin) ---
struct Settings {
  uint16_t forward_lookahead_ft = 800;
  uint16_t intersection_lookahead_ft = 800;
  uint16_t near_alert_ft = 150;
  /// When speed is below heading_trust_min_mph, still alert if an ALPR is within this radius (parking lots).
  uint16_t stopped_proximity_ft = 450;
  uint8_t forward_cone_deg = 30;
  uint8_t turn_fan_min_deg = 30;
  uint8_t turn_fan_max_deg = 110;
  uint8_t heading_trust_min_mph = 5;
  uint16_t alert_cooldown_ms = 30000;
};

Settings& settings();
