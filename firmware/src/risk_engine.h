#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "types.h"

struct VehicleState {
  double lat_deg{0};
  double lon_deg{0};
  float speed_mph{0};
  float heading_deg{NAN};  // smoothed COG when trusted; else NAN
};

enum class PathClass : uint8_t { None = 0, Ahead, LeftTurn, RightTurn, Intersection };

struct AlertResult {
  bool active{false};
  PathClass classification{PathClass::None};
  AlertObject object{};
  float distance_ft{0};
  float score{0};
};

class RiskEngine {
 public:
  AlertResult evaluate(const VehicleState& vehicle, const AlertObject* objects, size_t count) const;

 private:
  bool turn_fan_active(const VehicleState& v, const AlertObject& obj, float distance_ft) const;
  float compute_score(const AlertObject& obj, float distance_ft, PathClass cls) const;
};
