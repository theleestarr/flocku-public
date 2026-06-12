#include "alert_ui.h"

#include <cmath>

AlertArrow alert_arrow_from_path(PathClass cls, float signed_angle_deg) {
  if (cls == PathClass::LeftTurn) return AlertArrow::Left;
  if (cls == PathClass::RightTurn) return AlertArrow::Right;
  if (cls != PathClass::Ahead) return AlertArrow::None;

  const float a = std::fabs(signed_angle_deg);
  if (a < 15.0f) return AlertArrow::Ahead;
  if (signed_angle_deg > 0.0f) {
    if (a < 45.0f) return AlertArrow::BearRight;
    if (a < 90.0f) return AlertArrow::SideRight;
  } else {
    if (a < 45.0f) return AlertArrow::BearLeft;
    if (a < 90.0f) return AlertArrow::SideLeft;
  }
  return AlertArrow::None;
}
