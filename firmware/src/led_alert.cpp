#include "led_alert.h"

#if defined(FLOCKU_BOARD_HELTEC_TRACKER) && LED_ALERT_USE_WS2812
/// Lit count is always an even number: 2, 4, 6, or 8 (pairs only).
static uint8_t tracker_neo_lit_count(float d_ft) {
  if (d_ft > 400.f) return 2;
  if (d_ft > 250.f) return 4;
  if (d_ft > 120.f) return 6;
  return 8;
}

/// 8-LED bar (indices 0–7): center pair 3+4, expand outward in pairs (solid red, no flash).
static uint8_t tracker_neo_center_mask(uint8_t lit_count) {
  switch (lit_count) {
    case 2:
      return 0x18;  // ..XX.... pixels 3,4
    case 4:
      return 0x3C;  // .XXXX... pixels 2,3,4,5
    case 6:
      return 0x7E;  // .XXXXXX. pixels 1–6
    case 8:
      return 0xFF;
    default:
      return 0x18;
  }
}

/// Larson scanner: head sweeps 0→7→0 ping-pong with one dim trailing pixel.
static void tracker_draw_larson(Adafruit_NeoPixel* strip, uint32_t now_ms, uint32_t head_rgb, uint32_t tail_rgb,
                                uint8_t brightness) {
  strip->setBrightness(brightness);
  constexpr uint8_t k_pat_len = 14;
  const uint32_t step = (now_ms / NEOPIXEL_IDLE_LARSON_STEP_MS) % k_pat_len;
  uint8_t head = step < 8 ? static_cast<uint8_t>(step) : static_cast<uint8_t>(14 - step);

  strip->clear();
  strip->setPixelColor(head, head_rgb);

  int8_t tail = -1;
  if (step > 0 && step < 8) {
    tail = static_cast<int8_t>(head) - 1;
  } else if (step >= 8) {
    tail = static_cast<int8_t>(head) + 1;
  }
  if (tail >= 0 && tail < static_cast<int8_t>(NEOPIXEL_COUNT)) {
    strip->setPixelColor(static_cast<uint16_t>(tail), tail_rgb);
  }

  strip->show();
}

static void tracker_draw_solid(Adafruit_NeoPixel* strip, uint32_t rgb, uint8_t brightness) {
  strip->setBrightness(brightness);
  strip->clear();
  for (uint8_t i = 0; i < NEOPIXEL_COUNT; ++i) {
    strip->setPixelColor(i, rgb);
  }
  strip->show();
}
#endif

void LedAlert::boot_flash_red() {
#if defined(FLOCKU_BOARD_HELTEC_TRACKER) && LED_ALERT_USE_WS2812
  if (!rgb_strip_) return;
  rgb_strip_->setBrightness(NEOPIXEL_MAX_BRIGHTNESS);
  const uint32_t red = Adafruit_NeoPixel::Color(255, 0, 0);
  constexpr uint8_t k_flashes = 3;
  for (uint8_t i = 0; i < k_flashes; ++i) {
    for (uint8_t p = 0; p < NEOPIXEL_COUNT; ++p) {
      rgb_strip_->setPixelColor(p, red);
    }
    rgb_strip_->show();
    delay(120);
    rgb_strip_->clear();
    rgb_strip_->show();
    delay(120);
  }
#endif
}

void LedAlert::begin(uint8_t pin, bool active_high) {
  pin_ = pin;
  active_high_ = active_high;
  enabled_ = true;

#if LED_ALERT_USE_WS2812
  if (rgb_strip_) {
    delete rgb_strip_;
    rgb_strip_ = nullptr;
  }
  rgb_strip_ = new Adafruit_NeoPixel(NEOPIXEL_COUNT, pin_, NEO_GRB + NEO_KHZ800);
  rgb_strip_->begin();
  rgb_strip_->setBrightness(NEOPIXEL_MAX_BRIGHTNESS);
  rgb_strip_->clear();
  rgb_strip_->show();
#else
  pinMode(pin_, OUTPUT);
#endif
  apply_level(0);
}

void LedAlert::set_active(bool on) { enabled_ = on; }

void LedAlert::apply_level(uint8_t level) {
  level_ = level;
#if LED_ALERT_USE_WS2812
  if (!rgb_strip_) return;
  if (!enabled_ || level_ == 0) {
    rgb_strip_->clear();
    rgb_strip_->show();
    return;
  }
#if defined(FLOCKU_BOARD_HELTEC_TRACKER)
  const uint32_t r = static_cast<uint32_t>(level_) * 45u;
  const uint8_t red = r > 255u ? 255u : static_cast<uint8_t>(r);
  const uint32_t c = Adafruit_NeoPixel::Color(red, 0, 0);
  for (uint16_t i = 0; i < NEOPIXEL_COUNT; ++i) {
    rgb_strip_->setPixelColor(i, c);
  }
#else
  const uint32_t r = static_cast<uint32_t>(level_) * 45u;
  const uint8_t red = r > 255u ? 255u : static_cast<uint8_t>(r);
  rgb_strip_->setPixelColor(0, Adafruit_NeoPixel::Color(red, 0, 0));
#endif
  rgb_strip_->show();
#else
  const bool lit = enabled_ && level_ > 0;
  digitalWrite(pin_, (active_high_ ? lit : !lit) ? HIGH : LOW);
#endif
}

uint8_t LedAlert::distance_band(float distance_ft) const {
  if (distance_ft > 800.0f) return 0;
  if (distance_ft > 500.0f) return 1;
  if (distance_ft > 300.0f) return 2;
  if (distance_ft > 150.0f) return 3;
  if (distance_ft > 75.0f) return 4;
  return 5;
}

void LedAlert::update(uint32_t now_ms, bool alert_active, float distance_ft, uint8_t object_kind,
                       LedBarMode bar_mode) {
#if defined(FLOCKU_BOARD_HELTEC_TRACKER) && LED_ALERT_USE_WS2812
  if (!rgb_strip_) return;
  if (!enabled_) {
    pattern_ = LedPatternKind::None;
    rgb_strip_->clear();
    rgb_strip_->show();
    return;
  }

  if (!alert_active) {
    pattern_ = LedPatternKind::None;
    switch (bar_mode) {
      case LedBarMode::IdleLarson:
        tracker_draw_larson(rgb_strip_, now_ms, Adafruit_NeoPixel::Color(255, 255, 255),
                            Adafruit_NeoPixel::Color(70, 110, 220), NEOPIXEL_IDLE_BRIGHTNESS);
        return;
      case LedBarMode::GpsLarson:
        tracker_draw_larson(rgb_strip_, now_ms, Adafruit_NeoPixel::Color(255, 220, 0),
                            Adafruit_NeoPixel::Color(140, 100, 0), NEOPIXEL_IDLE_BRIGHTNESS);
        return;
      case LedBarMode::NoDataYellow:
        tracker_draw_solid(rgb_strip_, Adafruit_NeoPixel::Color(255, 220, 0), NEOPIXEL_MAX_BRIGHTNESS);
        return;
      default:
        rgb_strip_->clear();
        rgb_strip_->show();
        return;
    }
  }

  rgb_strip_->setBrightness(NEOPIXEL_MAX_BRIGHTNESS);

  if (object_kind == KIND_INTERSECTION_ZONE) {
    pattern_ = LedPatternKind::Intersection;
  } else if (object_kind == KIND_CAMERA_CLUSTER) {
    pattern_ = LedPatternKind::Cluster;
  } else {
    pattern_ = LedPatternKind::Camera;
  }

  (void)now_ms;
  const uint8_t n = tracker_neo_lit_count(distance_ft);
  const uint8_t mask = tracker_neo_center_mask(n);
  const uint32_t red = Adafruit_NeoPixel::Color(255, 0, 0);

  rgb_strip_->clear();
  for (uint8_t i = 0; i < NEOPIXEL_COUNT; ++i) {
    rgb_strip_->setPixelColor(i, (mask & (1u << i)) ? red : 0u);
  }
  rgb_strip_->show();
  return;
#endif

  (void)bar_mode;

  if (!alert_active) {
    pattern_ = LedPatternKind::None;
    apply_level(0);
    return;
  }

  if (object_kind == KIND_INTERSECTION_ZONE) {
    pattern_ = LedPatternKind::Intersection;
  } else if (object_kind == KIND_CAMERA_CLUSTER) {
    pattern_ = LedPatternKind::Cluster;
  } else {
    pattern_ = LedPatternKind::Camera;
  }

  const uint8_t band = distance_band(distance_ft);
  if (band == 0) {
    const uint32_t period = 1200;
    apply_level(((now_ms / (period / 2)) % 2) ? 1 : 0);
    return;
  }

  static const uint16_t kHalfMs[] = {0, 400, 280, 180, 90, 0};
  uint16_t half = kHalfMs[band];
  if (band == 5) {
    apply_level(5);
    return;
  }

  if (pattern_ == LedPatternKind::Intersection && band <= 3) {
    const uint16_t half2 = static_cast<uint16_t>(half / 2);
    half = half2 < 60 ? 60 : half2;
  }

  const uint32_t cycle = static_cast<uint32_t>(half) * 2;
  if (cycle == 0) {
    apply_level(0);
    return;
  }

  const uint32_t pos = now_ms % cycle;
  bool on = pos < half;

  if (pattern_ == LedPatternKind::Cluster) {
    const uint32_t dcycle = cycle * 2;
    const uint32_t p2 = now_ms % dcycle;
    on = (p2 < half) || (p2 > cycle && p2 < cycle + half);
  } else if (pattern_ == LedPatternKind::Intersection) {
    const uint32_t tri = cycle * 3;
    const uint32_t p3 = now_ms % tri;
    on = (p3 < half) || (p3 > cycle && p3 < cycle + half) || (p3 > 2 * cycle && p3 < 2 * cycle + half);
  }

  apply_level(on ? band : 0);
}
