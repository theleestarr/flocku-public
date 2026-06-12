#pragma once

#include <cstddef>
#include <cstdint>

#include "risk_engine.h"
#include "types.h"

// Feeds a scripted GPS route and optional in-RAM alert objects for desk testing (no UART / files).
class Simulation {
 public:
  void begin();
  bool next_vehicle(VehicleState* out);

  const AlertObject* mock_objects() const { return objs_; }
  size_t mock_object_count() const { return n_objs_; }

 private:
  size_t idx_{0};
  static const AlertObject objs_[];
  static const size_t n_objs_;
};
