#include <Arduino.h>
#include <cctype>
#include <cmath>
#include <cstring>

#include "battery_monitor.h"
#include "camera_store.h"
#include "alert_ui.h"
#include "config.h"
#include "display_manager.h"
#include "geo_math.h"
#include "gps_manager.h"
#include "led_alert.h"
#include "risk_engine.h"
#include "simulation.h"
#include "state_locator.h"

struct CollectCtx {
  AlertObject buf[128];
  size_t n{0};
};

static void collect_cb(const AlertObject& obj, void* user) {
  auto* c = static_cast<CollectCtx*>(user);
  if (c->n < sizeof(c->buf) / sizeof(c->buf[0])) c->buf[c->n++] = obj;
}

static const char* camera_type_label(uint8_t t) {
  switch (t) {
    case TYPE_FLOCK:
      return "FLOCK";
    case TYPE_OTHER_ALPR:
      return "ALPR";
    case TYPE_MIXED:
      return "MIXED";
    default:
      return "ALPR?";
  }
}

static void scopy(char* dst, size_t dst_sz, const char* src) {
  if (!dst_sz) return;
  std::strncpy(dst, src, dst_sz - 1);
  dst[dst_sz - 1] = '\0';
}

static DisplayManager g_disp;
static LedAlert g_led;
static GpsManager g_gps;
static CameraStore g_store;
static RiskEngine g_risk;
#if SIMULATION_MODE
static Simulation g_sim;
#endif

static char g_state[3]{'-', '-', '\0'};
static bool g_have_file{false};

static bool state_code_valid(const char code[3]) {
  return std::isalpha(static_cast<unsigned char>(code[0])) &&
         std::isalpha(static_cast<unsigned char>(code[1]));
}

static void fill_vehicle_ui_bar(VehicleUiState* vu, bool gps_ok, float speed_mph,
                                uint8_t gps_sat_count = 0, bool gps_sat_count_valid = false) {
  vu->gps_ok = gps_ok;
  vu->speed_mph = speed_mph;
  vu->gps_sat_count = gps_sat_count;
  vu->gps_sat_count_valid = gps_sat_count_valid;
#if defined(FLOCKU_BOARD_TBEAM) || defined(FLOCKU_BOARD_LILYGO_PAGER)
  vu->battery_percent = battery_percent_read();
  vu->battery_charging = battery_is_charging();
  vu->battery_usb_present = battery_usb_present();
#else
  vu->battery_percent = -1;
  vu->battery_charging = false;
  vu->battery_usb_present = false;
#endif
}

static void build_alert_ui_v(const VehicleState& v, const AlertResult& r, AlertUiState* ui) {
  std::memset(ui, 0, sizeof(*ui));
  if (!r.active) return;
  ui->active = true;

  const double olat = static_cast<double>(r.object.lat_e7) / 1e7;
  const double olon = static_cast<double>(r.object.lon_e7) / 1e7;
  const float brg = static_cast<float>(bearingDeg(v.lat_deg, v.lon_deg, olat, olon));
  const float signed_a =
      std::isfinite(v.heading_deg) ? signedAngleDiffDeg(v.heading_deg, brg) : signedAngleDiffDeg(0.0f, brg);

  const int dft = static_cast<int>(r.distance_ft + 0.5f);
  ui->arrow = AlertArrow::None;

  if (r.classification == PathClass::Intersection || r.object.kind == KIND_INTERSECTION_ZONE) {
    scopy(ui->title, sizeof(ui->title), "AVOID INTERSECTION");
    snprintf(ui->line2, sizeof(ui->line2), "%u cams", r.object.count);
    snprintf(ui->line3, sizeof(ui->line3), "%d ft", dft);
  } else if (r.object.kind == KIND_CAMERA_CLUSTER) {
    scopy(ui->title, sizeof(ui->title), "ALPR ZONE");
    snprintf(ui->line2, sizeof(ui->line2), "%u cams", r.object.count);
    snprintf(ui->line3, sizeof(ui->line3), "%d ft", dft);
    if (v.speed_mph >= static_cast<float>(settings().heading_trust_min_mph)) {
      ui->arrow = alert_arrow_from_path(PathClass::Ahead, signed_a);
    }
  } else if (r.classification == PathClass::LeftTurn) {
    scopy(ui->title, sizeof(ui->title), "ALPR IF LEFT");
    snprintf(ui->line2, sizeof(ui->line2), "%d ft", dft);
    ui->arrow = AlertArrow::Left;
    scopy(ui->brand, sizeof(ui->brand), camera_type_label(r.object.type));
  } else if (r.classification == PathClass::RightTurn) {
    scopy(ui->title, sizeof(ui->title), "ALPR IF RIGHT");
    snprintf(ui->line2, sizeof(ui->line2), "%d ft", dft);
    ui->arrow = AlertArrow::Right;
    scopy(ui->brand, sizeof(ui->brand), camera_type_label(r.object.type));
  } else if (v.speed_mph < static_cast<float>(settings().heading_trust_min_mph)) {
    // Parked / slow: course-over-ground arrow is meaningless without reliable heading.
    scopy(ui->title, sizeof(ui->title), "ALPR NEAR");
    snprintf(ui->line2, sizeof(ui->line2), "%d ft", dft);
    scopy(ui->brand, sizeof(ui->brand), camera_type_label(r.object.type));
  } else {
    scopy(ui->title, sizeof(ui->title), "ALPR AHEAD");
    snprintf(ui->line2, sizeof(ui->line2), "%d ft", dft);
    ui->arrow = AlertArrow::Ahead;
    scopy(ui->brand, sizeof(ui->brand), camera_type_label(r.object.type));
  }
}

void setup() {
  Serial.begin(115200);

  g_disp.begin();
#if SIMULATION_MODE
  {
    VehicleUiState boot{};
    fill_vehicle_ui_bar(&boot, true, 0.f);
    boot.sim_mode = true;
    g_disp.show_boot_acquiring(millis(), boot);
  }
#else
  {
    VehicleUiState boot{};
    fill_vehicle_ui_bar(&boot, false, 0.f);
    g_disp.show_boot_acquiring(millis(), boot);
  }
#endif

#if LED_ALERT_PIN >= 0
  g_led.begin(static_cast<uint8_t>(LED_ALERT_PIN), true);
#endif
#if defined(FLOCKU_BOARD_HELTEC_TRACKER) && LED_ALERT_USE_WS2812
  g_led.boot_flash_red();
#endif

  g_store.mountLittleFs();
  state_locator_init();

#if SIMULATION_MODE
  g_sim.begin();
  g_have_file = true;
#else
  g_gps.begin();
  const uint32_t boot_gps_start = millis();
  while (!g_gps.has_fix()) {
    if (millis() - boot_gps_start >= GPS_BOOT_FIX_TIMEOUT_MS) break;
    g_gps.update();
#if !defined(FLOCKU_TFT_TRACKER_UI)
    // OLED / T-Beam: refresh boot for GPS blink; keep polling fast on Tracker (splash is static).
    VehicleUiState boot{};
    fill_vehicle_ui_bar(&boot, g_gps.has_fix(), g_gps.speed_mph(), g_gps.satellites_used(),
                          g_gps.satellites_valid());
    g_disp.show_boot_acquiring(millis(), boot);
#endif
#if defined(FLOCKU_TFT_TRACKER_UI)
    delay(100);
#else
    delay(50);
#endif
  }
#if !defined(FLOCKU_TFT_TRACKER_UI)
  {
    VehicleUiState boot{};
    fill_vehicle_ui_bar(&boot, g_gps.has_fix(), g_gps.speed_mph(), g_gps.satellites_used(),
                        g_gps.satellites_valid());
    g_disp.show_boot_acquiring(millis(), boot);
  }
#endif
#endif
}

void loop() {
  const uint32_t now = millis();

  VehicleState vs{};

#if SIMULATION_MODE
  g_sim.next_vehicle(&vs);
  if (!std::isfinite(vs.heading_deg)) vs.heading_deg = 90.0f;
#else
  g_gps.update();
  vs.lat_deg = g_gps.lat_deg();
  vs.lon_deg = g_gps.lon_deg();
  vs.speed_mph = g_gps.speed_mph();
  vs.heading_deg = g_gps.smoothed_heading_deg();
  if (!g_gps.has_fix()) {
    VehicleUiState vu{};
    fill_vehicle_ui_bar(&vu, false, g_gps.speed_mph(), g_gps.satellites_used(), g_gps.satellites_valid());
    g_disp.show_gps_searching(vu);
#if defined(FLOCKU_BOARD_HELTEC_TRACKER) && LED_ALERT_USE_WS2812
    g_led.update(now, false, 9999.0f, KIND_CAMERA_POINT, LedBarMode::GpsLarson);
#else
    g_led.update(now, false, 9999.0f, KIND_CAMERA_POINT);
#endif
#if defined(FLOCKU_TFT_TRACKER_UI)
    static uint32_t s_last_gps_log;
    if (now - s_last_gps_log >= 5000) {
      s_last_gps_log = now;
      Serial.printf("gps: sats=%u valid=%d age=%ums\n", static_cast<unsigned>(g_gps.satellites_used()),
                      g_gps.satellites_valid() ? 1 : 0, g_gps.fix_age_ms());
    }
#endif
#if defined(FLOCKU_BOARD_LILYGO_PAGER)
    delay(50);
#endif
    return;
  }
#endif

  char st[3]{};
  const bool resolved = state_locator_resolve(vs.lat_deg, vs.lon_deg, st);
  if (resolved) {
    g_state[0] = st[0];
    g_state[1] = st[1];
    g_state[2] = '\0';
  } else {
    g_state[0] = '-';
    g_state[1] = '-';
    g_state[2] = '\0';
  }

#if !SIMULATION_MODE
  static char g_open_state[3]{""};
  if (resolved) {
    if (g_open_state[0] != g_state[0] || g_open_state[1] != g_state[1]) {
      g_have_file = g_store.open_state_file(g_state);
      g_open_state[0] = g_state[0];
      g_open_state[1] = g_state[1];
      g_open_state[2] = '\0';
    }
  } else {
    g_have_file = false;
    g_open_state[0] = '\0';
  }
#endif

  CollectCtx ctx{};

#if SIMULATION_MODE
  for (size_t i = 0; i < g_sim.mock_object_count(); ++i) {
    collect_cb(g_sim.mock_objects()[i], &ctx);
  }
#else
  if (!resolved) {
    VehicleUiState vu{};
    fill_vehicle_ui_bar(&vu, true, vs.speed_mph, g_gps.satellites_used(), g_gps.satellites_valid());
    scopy(vu.state_code, sizeof(vu.state_code), "--");
    g_disp.show_no_data_file(vu, "--");
#if defined(FLOCKU_BOARD_HELTEC_TRACKER) && LED_ALERT_USE_WS2812
    g_led.update(now, false, 9999.0f, KIND_CAMERA_POINT, LedBarMode::NoDataYellow);
#else
    g_led.update(now, false, 9999.0f, KIND_CAMERA_POINT);
#endif
#if defined(FLOCKU_BOARD_LILYGO_PAGER)
    delay(50);
#endif
    return;
  }
  if (!g_have_file) {
    VehicleUiState vu{};
    fill_vehicle_ui_bar(&vu, true, vs.speed_mph, g_gps.satellites_used(), g_gps.satellites_valid());
    g_disp.show_no_data_file(vu, g_state);
#if defined(FLOCKU_BOARD_HELTEC_TRACKER) && LED_ALERT_USE_WS2812
    g_led.update(now, false, 9999.0f, KIND_CAMERA_POINT, LedBarMode::NoDataYellow);
#else
    g_led.update(now, false, 9999.0f, KIND_CAMERA_POINT);
#endif
#if defined(FLOCKU_BOARD_LILYGO_PAGER)
    delay(50);
#endif
    return;
  }
  g_store.scan_near(vs.lat_deg, vs.lon_deg, static_cast<float>(settings().forward_lookahead_ft + 400.0f),
                    collect_cb, &ctx);
#endif

  const AlertResult ar = g_risk.evaluate(vs, ctx.buf, ctx.n);

  VehicleUiState vu{};
#if SIMULATION_MODE
  fill_vehicle_ui_bar(&vu, true, vs.speed_mph, 9, true);
#else
  fill_vehicle_ui_bar(&vu, true, vs.speed_mph, g_gps.satellites_used(), g_gps.satellites_valid());
#endif
  scopy(vu.state_code, sizeof(vu.state_code), state_code_valid(g_state) ? g_state : "--");
  vu.cam_count = g_store.header_valid() ? g_store.record_count() : static_cast<uint32_t>(ctx.n);
#if SIMULATION_MODE
  vu.sim_mode = true;
#endif

  AlertUiState au{};
  build_alert_ui_v(vs, ar, &au);

  if (!ar.active) {
    g_disp.show_idle(vu);
#if defined(FLOCKU_BOARD_HELTEC_TRACKER) && LED_ALERT_USE_WS2812
    g_led.update(now, false, 9999.0f, KIND_CAMERA_POINT, LedBarMode::IdleLarson);
#else
    g_led.update(now, false, 9999.0f, KIND_CAMERA_POINT);
#endif
  } else {
    g_disp.show_alert(vu, au);
    g_led.update(now, true, ar.distance_ft, ar.object.kind);
  }

#if defined(FLOCKU_BOARD_LILYGO_PAGER)
  delay(50);
#endif

  (void)now;
}
