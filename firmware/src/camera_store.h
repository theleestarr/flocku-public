#pragma once

#include <Arduino.h>
#include <FS.h>

#include "types.h"

// Reads LittleFS `/cams/{ST}.bin` with packed header + AlertObject stream.
class CameraStore {
 public:
  bool mountLittleFs();
  bool open_state_file(const char state_two_letter[2]);
  void close();

  bool header_valid() const { return header_ok_; }
  const CameraFileHeader& header() const { return hdr_; }
  uint32_t record_count() const { return hdr_.record_count; }
  uint32_t dataset_version() const { return hdr_.dataset_version; }

  // Linear scan (V1). Invokes cb for each record within a coarse lat/lon window.
  using ScanCallback = void (*)(const AlertObject& obj, void* user);
  void scan_near(double lat_deg, double lon_deg, float max_ft, ScanCallback cb, void* user);

 private:
  bool read_header();
  bool seek_to_records();

  File f_{};
  CameraFileHeader hdr_{};
  bool header_ok_{false};
  bool mounted_{false};
};
