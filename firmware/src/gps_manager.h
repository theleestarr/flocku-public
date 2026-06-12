#pragma once

#include <Arduino.h>

// Parses NMEA on a UART, exposes fix + smoothed course (TinyGPSPlus).
class GpsManager {
 public:
  void begin();
  void update();  // call often; drains UART and updates fix

  bool has_fix() const { return has_fix_; }
  double lat_deg() const { return lat_; }
  double lon_deg() const { return lon_; }
  float speed_mph() const { return speed_mph_; }
  float course_deg() const { return course_deg_; }
  float smoothed_heading_deg() const { return smoothed_heading_deg_; }
  uint32_t fix_age_ms() const { return fix_age_ms_; }

  /// GGA “satellites used in fix” when fresh (for dashboard).
  bool satellites_valid() const;
  uint8_t satellites_used() const;

 private:
  void ingest_char(int c);
  void recompute_heading_smooth();

  bool has_fix_{false};
  double lat_{0};
  double lon_{0};
  float speed_mph_{0};
  float course_deg_{0};
  float smoothed_heading_deg_{NAN};
  uint32_t fix_age_ms_{0};

  static constexpr size_t kHeadingBuf = 5;
  float heading_ring_[kHeadingBuf]{};
  size_t heading_count_{0};
  size_t heading_pos_{0};
};
