#pragma once

#include <cstdint>

// --- Enums (must match tools/alpr_dataset/constants.py) ---

enum AlertKind : uint8_t {
  KIND_CAMERA_POINT = 1,
  KIND_CAMERA_CLUSTER = 2,
  KIND_INTERSECTION_ZONE = 3,
  KIND_TURN_RISK_ZONE = 4,
};

enum CameraType : uint8_t {
  TYPE_UNKNOWN_ALPR = 0,
  TYPE_FLOCK = 1,
  TYPE_OTHER_ALPR = 2,
  TYPE_MIXED = 3,
};

// Coverage bitmask: cardinal + turn-relative + ahead.
constexpr uint8_t COVER_NORTH = 1 << 0;
constexpr uint8_t COVER_EAST = 1 << 1;
constexpr uint8_t COVER_SOUTH = 1 << 2;
constexpr uint8_t COVER_WEST = 1 << 3;
constexpr uint8_t COVER_LEFT = 1 << 4;
constexpr uint8_t COVER_RIGHT = 1 << 5;
constexpr uint8_t COVER_AHEAD = 1 << 6;

#pragma pack(push, 1)

struct CameraFileHeader {
  char magic[4];          // "ALPR"
  uint8_t version;        // file format version (kCameraFileVersion)
  char state_code[2];     // e.g. "WA"
  uint32_t record_count;
  uint32_t dataset_version;  // monotonic DB release; compare for OTA camera-pack updates
};

static_assert(sizeof(CameraFileHeader) == 15, "CameraFileHeader must be 15 bytes packed");

struct AlertObject {
  int32_t lat_e7;
  int32_t lon_e7;
  uint16_t radius_ft;
  uint8_t kind;
  uint8_t type;
  uint8_t count;
  uint8_t coverage;
};

static_assert(sizeof(AlertObject) == 14, "AlertObject must be 14 bytes packed");

#pragma pack(pop)

constexpr char kCameraMagic[4] = {'A', 'L', 'P', 'R'};
constexpr uint8_t kCameraFileVersion = 1;
