#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "alert_ui.h"
#include "types.h"

struct VehicleUiState {
  bool gps_ok{false};
  char state_code[3]{"--"};  // "WA\0"
  uint32_t cam_count{0};
  float speed_mph{0};
  /// GGA satellite count when `gps_sat_count_valid`; shown next to sat glyph on Tracker dashboard.
  uint8_t gps_sat_count{0};
  bool gps_sat_count_valid{false};
  bool sim_mode{false};
  /// Reserved; demo builds are identified only by `FIRMWARE_VERSION_STRING` (e.g. demo-1.1.7).
  bool demo_mode{false};
  char demo_caption[24]{};
  /// T-Beam AXP192: 0–100, or -1 if unknown (Heltec ignores for UI).
  int8_t battery_percent{-1};
  /// AXP192: lithium charging active (hint for charge-circuit verification).
  bool battery_charging{false};
  /// AXP192: USB VBUS present (show USB glyph when powered from USB but not charging).
  bool battery_usb_present{false};
};

struct AlertUiState {
  bool active{false};
  char title[22]{};
  char line2[22]{};
  char line3[22]{};
  char brand[12]{};
  AlertArrow arrow{AlertArrow::None};
};

// OLED: 128×64 SSD1306 (Heltec V4, T‑Beam, WROVER) or 160×80 ST7735 (Heltec Wireless Tracker).
class DisplayManager {
 public:
  bool begin();
  /** Boot: fox (+ version on OLED); Tracker splash is logo-only; operational Tracker screens use dark grey title bar. */
  void show_boot_acquiring(uint32_t now_ms, const VehicleUiState& v);
  void show_gps_searching(const VehicleUiState& v);
  void show_no_data_file(const VehicleUiState& v, const char* state_code);
  void show_idle(const VehicleUiState& v);
  void show_alert(const VehicleUiState& v, const AlertUiState& a);

  /// Booth demo: force next operational screen draw (carousel paints once per step).
  void invalidate_screen();

  // Throttle full redraws when idle (ms).
  void set_min_idle_redraw_ms(uint32_t ms) { min_idle_redraw_ms_ = ms; }

 private:
  void render_all();
  bool ensure_display();

  uint32_t last_idle_draw_{0};
  uint32_t min_idle_redraw_ms_{500};
  bool inited_{false};
};
