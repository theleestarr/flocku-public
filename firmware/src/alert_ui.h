#pragma once

#include <cstdint>

#include "risk_engine.h"

/// Heading-relative direction glyph for Tracker alert body (ST7735).
enum class AlertArrow : uint8_t {
  None = 0,
  Ahead,
  Left,
  Right,
  BearRight,
  BearLeft,
  SideRight,
  SideLeft,
};

AlertArrow alert_arrow_from_path(PathClass cls, float signed_angle_deg);
