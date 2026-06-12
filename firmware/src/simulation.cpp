#include "simulation.h"

// Short synthetic route near Seattle-ish coordinates with eastbound heading.
static const struct {
  double lat, lon;
  float speed_mph;
  float heading;
} kRoute[] = {
    {47.620000, -122.350000, 35.0f, 90.0f},
    {47.620000, -122.349500, 35.0f, 90.0f},
    {47.620000, -122.349000, 35.0f, 90.0f},
    {47.620000, -122.348500, 35.0f, 90.0f},
    {47.620000, -122.348000, 35.0f, 90.0f},
};

// One point camera "ahead" of the route, one intersection-style cluster centroid.
static const AlertObject kObjs[] = {
    {static_cast<int32_t>(47.620100 * 1e7), static_cast<int32_t>(-122.347800 * 1e7), 120, KIND_CAMERA_POINT,
     TYPE_FLOCK, 1, COVER_AHEAD},
    {static_cast<int32_t>(47.620050 * 1e7), static_cast<int32_t>(-122.347600 * 1e7), 300, KIND_INTERSECTION_ZONE,
     TYPE_MIXED, 4, COVER_NORTH | COVER_EAST | COVER_SOUTH},
};

const AlertObject Simulation::objs_[] = {kObjs[0], kObjs[1]};
const size_t Simulation::n_objs_ = sizeof(kObjs) / sizeof(kObjs[0]);

void Simulation::begin() { idx_ = 0; }

bool Simulation::next_vehicle(VehicleState* out) {
  if (!out) return false;
  const auto& p = kRoute[idx_ % (sizeof(kRoute) / sizeof(kRoute[0]))];
  out->lat_deg = p.lat;
  out->lon_deg = p.lon;
  out->speed_mph = p.speed_mph;
  out->heading_deg = p.heading;
  ++idx_;
  return true;
}
