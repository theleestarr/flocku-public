#pragma once

#include <Arduino.h>

#include "config.h"
#include "types.h"

#if LED_ALERT_USE_WS2812
#include <Adafruit_NeoPixel.h>
#endif

enum class LedPatternKind : uint8_t { None = 0, Camera, Cluster, Intersection };

/// Non-alert NeoPixel bar modes (Wireless Tracker WS2812 strip).
enum class LedBarMode : uint8_t {
  Off = 0,
  IdleLarson,    // white / blue scanner — “No ALPR nearby”
  GpsLarson,     // yellow scanner — GPS searching
  NoDataYellow,  // solid yellow — missing state cam file
};

// Non-blocking alert output: optional WS2812 or GPIO LED (Heltec V4 OLED build uses GPIO35 per Meshtastic).
class LedAlert {
 public:
  void begin(uint8_t pin, bool active_high = true);
  /// Wireless Tracker (NeoPixel strip): brief red flash at power-up; no-op elsewhere.
  void boot_flash_red();
  void set_active(bool on);
  void update(uint32_t now_ms, bool alert_active, float distance_ft, uint8_t object_kind,
              LedBarMode bar_mode = LedBarMode::Off);

 private:
  void apply_level(uint8_t level);  // 0 off .. 5 solid
  uint8_t distance_band(float distance_ft) const;

  uint8_t pin_{255};
  bool active_high_{true};
  bool enabled_{false};

  uint8_t level_{0};
  LedPatternKind pattern_{LedPatternKind::None};

#if LED_ALERT_USE_WS2812
  Adafruit_NeoPixel* rgb_strip_{nullptr};
#endif
};
