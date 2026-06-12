#include "risk_engine.h"

#include <cmath>

#include "config.h"
#include "geo_math.h"

bool RiskEngine::turn_fan_active(const VehicleState& v, const AlertObject& obj,
                                 float distance_ft) const {
  const auto& s = settings();
  if (obj.kind == KIND_INTERSECTION_ZONE || obj.kind == KIND_TURN_RISK_ZONE) return true;
  if (distance_ft <= 500.0f) return true;
  if (v.speed_mph < 15.0f && distance_ft <= 800.0f) return true;
  (void)s;
  return false;
}

float RiskEngine::compute_score(const AlertObject& obj, float distance_ft, PathClass cls) const {
  float score = 100000.0f / (distance_ft + 25.0f);
  switch (obj.kind) {
    case KIND_INTERSECTION_ZONE:
      score += 50.0f;
      break;
    case KIND_CAMERA_CLUSTER:
      score += 30.0f;
      break;
    case KIND_TURN_RISK_ZONE:
      score += 25.0f;
      break;
    case KIND_CAMERA_POINT:
    default:
      score += 10.0f;
      break;
  }
  if (cls == PathClass::Ahead) score += 25.0f;
  if (cls == PathClass::Intersection) score += 40.0f;
  if (cls == PathClass::LeftTurn || cls == PathClass::RightTurn) score += 15.0f;
  return score;
}

AlertResult RiskEngine::evaluate(const VehicleState& vehicle, const AlertObject* objects,
                                 size_t count) const {
  const auto& s = settings();
  AlertResult best{};

  const bool heading_ok =
      std::isfinite(vehicle.heading_deg) && vehicle.speed_mph > static_cast<float>(s.heading_trust_min_mph);
  const float heading_use = heading_ok ? vehicle.heading_deg : 0.0f;

  const float max_lookahead = static_cast<float>(s.forward_lookahead_ft > s.intersection_lookahead_ft
                                                     ? s.forward_lookahead_ft
                                                     : s.intersection_lookahead_ft);

  for (size_t i = 0; i < count; ++i) {
    const AlertObject& obj = objects[i];
    const double olat = static_cast<double>(obj.lat_e7) / 1e7;
    const double olon = static_cast<double>(obj.lon_e7) / 1e7;
    const float distance_ft =
        static_cast<float>(distanceFeet(vehicle.lat_deg, vehicle.lon_deg, olat, olon));

    if (distance_ft > max_lookahead + static_cast<float>(obj.radius_ft)) continue;

    const float bearing = static_cast<float>(bearingDeg(vehicle.lat_deg, vehicle.lon_deg, olat, olon));
    const float signed_angle =
        heading_ok ? signedAngleDiffDeg(heading_use, bearing) : signedAngleDiffDeg(0.0f, bearing);
    const float abs_angle = std::fabs(signed_angle);

    PathClass classification = PathClass::None;

    if (obj.kind == KIND_INTERSECTION_ZONE && distance_ft <= static_cast<float>(s.intersection_lookahead_ft)) {
      if (abs_angle <= 110.0f || vehicle.speed_mph < 15.0f) {
        classification = PathClass::Intersection;
      }
    } else if (heading_ok && abs_angle <= static_cast<float>(s.forward_cone_deg)) {
      classification = PathClass::Ahead;
    } else if (!heading_ok &&
               distance_ft <= static_cast<float>(s.stopped_proximity_ft) &&
               (obj.kind == KIND_CAMERA_POINT || obj.kind == KIND_CAMERA_CLUSTER)) {
      // Parked / creeping: GNSS course is unreliable — radius-only proximity alert.
      classification = PathClass::Ahead;
    } else if (turn_fan_active(vehicle, obj, distance_ft) && heading_ok) {
      if (signed_angle > static_cast<float>(s.turn_fan_min_deg) &&
          signed_angle < static_cast<float>(s.turn_fan_max_deg)) {
        classification = PathClass::RightTurn;
      } else if (signed_angle < -static_cast<float>(s.turn_fan_min_deg) &&
                 signed_angle > -static_cast<float>(s.turn_fan_max_deg)) {
        classification = PathClass::LeftTurn;
      }
    } else if (!heading_ok && distance_ft <= static_cast<float>(s.intersection_lookahead_ft) &&
               (obj.kind == KIND_INTERSECTION_ZONE || obj.kind == KIND_TURN_RISK_ZONE)) {
      classification = PathClass::Intersection;
    }

    if (classification == PathClass::None) continue;

    const float sc = compute_score(obj, distance_ft, classification);
    if (!best.active || sc > best.score) {
      best.active = true;
      best.classification = classification;
      best.object = obj;
      best.distance_ft = distance_ft;
      best.score = sc;
    }
  }

  return best;
}
