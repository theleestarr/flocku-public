#include "display_manager.h"

#include <Adafruit_GFX.h>
#if defined(FLOCKU_BOARD_HELTEC_TRACKER)
#include <Adafruit_ST7735.h>
#include <SPI.h>
#elif defined(FLOCKU_BOARD_LILYGO_PAGER)
#include <TFT_eSPI.h>
#else
#include <Adafruit_SSD1306.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "config.h"

#include "battery_monitor.h"
#include "boot_logo_bitmap.h"
#if defined(FLOCKU_TFT_TRACKER_UI)
#include "boot_logo_tracker_bitmap.h"
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#endif
#if defined(FLOCKU_BOARD_LILYGO_PAGER)
#include "boot_logo_pager_bitmap.h"
#endif
#include "pager_board.h"
#include "satellite_icon.h"

#if defined(FLOCKU_BOARD_TBEAM)
#include "battery_icon_meshtastic.h"
#endif

#if defined(FLOCKU_BOARD_HELTEC_TRACKER)
// Hardware SPI (Meshtastic uses SPI3 @ 40 MHz). Bit-banged constructor was unreliable on real hardware.
static Adafruit_ST7735 display(&SPI, ST7735_CS, ST7735_DC, ST7735_RST);
static constexpr int16_t kScrW = 160;
static constexpr int16_t kScrH = 80;
#define MONO_HI ST77XX_WHITE
#define MONO_LO ST77XX_BLACK
#elif defined(FLOCKU_BOARD_LILYGO_PAGER)
static TFT_eSPI g_tft;
static TFT_eSprite g_pg(&g_tft);
static bool g_pg_ready{false};
struct PagerDisplay {
  void fillScreen(uint16_t c) { g_tft.fillScreen(c); }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) { g_tft.fillRect(x, y, w, h, c); }
  void drawPixel(int16_t x, int16_t y, uint16_t c) { g_tft.drawPixel(x, y, c); }
  void fillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t c) {
    g_tft.fillTriangle(x0, y0, x1, y1, x2, y2, c);
  }
  void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color) {
    g_tft.drawBitmap(x, y, bitmap, w, h, color, TFT_BLACK);
  }
  void setTextSize(uint8_t s) { g_tft.setTextSize(s); }
  void setTextColor(uint16_t c) { g_tft.setTextColor(c); }
  void setTextColor(uint16_t c, uint16_t bg) { g_tft.setTextColor(c, bg); }
  void setCursor(int16_t x, int16_t y) { g_tft.setCursor(x, y); }
  void print(const char* s) { g_tft.print(s); }
  void print(const __FlashStringHelper* s) { g_tft.print(s); }
  void print(uint32_t v) { g_tft.print(v); }
  void print(int v) { g_tft.print(v); }
  int16_t getCursorY() { return g_tft.getCursorY(); }
  void setFont(const GFXfont* font) {
    if (font) {
      g_tft.setFreeFont(font);
    } else {
      g_tft.setTextFont(1);
      g_tft.setTextSize(1);
    }
  }
  void getTextBounds(const char* text, int16_t, int16_t, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
    if (!text || !text[0]) {
      if (x1) *x1 = 0;
      if (y1) *y1 = 0;
      if (w) *w = 0;
      if (h) *h = 0;
      return;
    }
    if (w) *w = static_cast<uint16_t>(g_tft.textWidth(text));
    const int16_t fh = g_tft.fontHeight();
    if (h) *h = fh > 0 ? static_cast<uint16_t>(fh) : 16;
    if (x1) *x1 = 0;
    if (y1) *y1 = fh > 0 ? static_cast<int16_t>(-fh) : -16;
  }
} display;
static constexpr int16_t kScrW = 480;
static constexpr int16_t kScrH = 222;
#define MONO_HI TFT_WHITE
#define MONO_LO TFT_BLACK
#define ST77XX_WHITE TFT_WHITE
#define ST77XX_BLACK TFT_BLACK
#define ST77XX_RED TFT_RED
#define ST77XX_GREEN TFT_GREEN
#define ST77XX_YELLOW TFT_YELLOW
#define ST77XX_ORANGE TFT_ORANGE
#define ST77XX_CYAN TFT_CYAN
#if defined(FLOCKU_BOARD_LILYGO_PAGER)
static TFT_eSprite& pager_pg() {
  if (!g_pg_ready) {
    g_pg.setColorDepth(16);
    g_pg.createSprite(kScrW, kScrH);
    g_pg_ready = true;
  }
  return g_pg;
}

static void pager_flush() { pager_pg().pushSprite(0, 0); }

enum class PagerScr : uint8_t { Gps, NoData, Idle, Alert };

struct PagerSnap {
  PagerScr scr{PagerScr::Idle};
  uint8_t gps_ok{0};
  uint8_t gps_sat_valid{0};
  uint8_t gps_sat_count{0};
  int16_t speed_mph_i{0};
  int8_t battery_percent{-1};
  uint8_t battery_charging{0};
  uint8_t sim_mode{0};
  char state_code[3]{};
  uint32_t cam_count{0};
  char nodata_path[20]{};
  AlertUiState alert{};
};

static PagerSnap g_pager_last{};
static bool g_pager_has_last{false};
static uint32_t g_pager_last_flush_ms{0};

static bool pager_try_splash(uint32_t now_ms, const VehicleUiState& v);
static uint32_t g_pager_boot_ms{0};
static bool g_pager_initial_splash{false};
#endif
#else
static Adafruit_SSD1306 display(128, 64, &Wire, OLED_RST_PIN);
static constexpr int16_t kScrW = 128;
static constexpr int16_t kScrH = 64;
#define MONO_HI SSD1306_WHITE
#define MONO_LO SSD1306_BLACK
#endif

static inline void disp_clear() {
#if defined(FLOCKU_BOARD_LILYGO_PAGER)
  pager_pg().fillScreen(TFT_BLACK);
#elif defined(FLOCKU_TFT_TRACKER_UI)
  display.fillScreen(MONO_LO);
#else
  display.clearDisplay();
#endif
}

static inline void disp_show() {
#if !defined(FLOCKU_TFT_TRACKER_UI)
  display.display();
#endif
}

#if defined(FLOCKU_TFT_TRACKER_UI)
/// Full-screen ST7735 updates every loop() beat with the panel scan → visible bands; throttle redraws.
static bool tft_full_paint_skip(uint32_t& last_ms) {
#if defined(FLOCKU_DEMO_FIRMWARE)
  (void)last_ms;
  return false;
#else
  const uint32_t t = millis();
  if (last_ms != 0 && t - last_ms < TFT_FULL_REDRAW_MIN_MS) return true;
  last_ms = t;
  return false;
#endif
}

namespace {

struct TrackerIdleSnap {
  uint8_t gps_ok;
  uint8_t gps_sat_valid;
  uint8_t gps_sat_count;
  uint8_t sim_mode;
  uint8_t demo_mode;
  int16_t speed_mph_i;
  char state_code[3];
  char demo_caption[24];
  uint32_t cam_count;
};

TrackerIdleSnap g_last_idle_snap{};
bool g_idle_snap_valid{false};
bool g_tracker_idle_invalidate{true};

static void tracker_idle_mark_content_stale() { g_tracker_idle_invalidate = true; }

static TrackerIdleSnap tracker_idle_snap_from(const VehicleUiState& v) {
  TrackerIdleSnap s{};
  s.gps_ok = v.gps_ok ? 1u : 0u;
  s.gps_sat_valid = v.gps_sat_count_valid ? 1u : 0u;
  s.gps_sat_count = v.gps_sat_count;
  s.sim_mode = v.sim_mode ? 1u : 0u;
  s.demo_mode = v.demo_mode ? 1u : 0u;
  s.speed_mph_i = static_cast<int16_t>(v.speed_mph + 0.5f);
  s.state_code[0] = v.state_code[0];
  s.state_code[1] = v.state_code[1];
  s.state_code[2] = v.state_code[2];
  std::strncpy(s.demo_caption, v.demo_caption, sizeof(s.demo_caption) - 1);
  s.demo_caption[sizeof(s.demo_caption) - 1] = '\0';
  s.cam_count = v.cam_count;
  return s;
}

static bool tracker_idle_snap_equal(const TrackerIdleSnap& a, const TrackerIdleSnap& b) {
  return a.gps_ok == b.gps_ok && a.gps_sat_valid == b.gps_sat_valid &&
         a.gps_sat_count == b.gps_sat_count && a.sim_mode == b.sim_mode &&
         a.demo_mode == b.demo_mode && a.speed_mph_i == b.speed_mph_i &&
         a.cam_count == b.cam_count && a.state_code[0] == b.state_code[0] &&
         a.state_code[1] == b.state_code[1] && a.state_code[2] == b.state_code[2] &&
         std::strncmp(a.demo_caption, b.demo_caption, sizeof(a.demo_caption)) == 0;
}

static bool tracker_idle_should_repaint(const VehicleUiState& v) {
  if (g_tracker_idle_invalidate) return true;
  if (!g_idle_snap_valid) return true;
  const TrackerIdleSnap cur = tracker_idle_snap_from(v);
  return !tracker_idle_snap_equal(cur, g_last_idle_snap);
}

static void tracker_idle_note_repainted(const VehicleUiState& v) {
  g_last_idle_snap = tracker_idle_snap_from(v);
  g_idle_snap_valid = true;
  g_tracker_idle_invalidate = false;
}

}  // namespace
#endif

#if defined(FLOCKU_BOARD_TBEAM)
/// Meshtastic-style inverted status strip (white bar, black text). Fits 13px-tall battery icon.
static constexpr int16_t kTbeamBarH = 14;
#endif

static void draw_satellite_glyph_at(int16_t ox, int16_t oy, uint16_t color) {
  for (int16_t row = 0; row < kSatelliteIconH; ++row) {
    const uint8_t b = pgm_read_byte(&kMeshtasticSatellite8[row]);
    for (int16_t col = 0; col < kSatelliteIconW; ++col) {
      if (((b >> col) & 1) != 0) {
        const int16_t px = ox + col * kSatelliteDisplayScale;
        const int16_t py = oy + row * kSatelliteDisplayScale;
        display.fillRect(px, py, kSatelliteDisplayScale, kSatelliteDisplayScale, color);
      }
    }
  }
}

static void draw_satellite_glyph() { draw_satellite_glyph_at(0, 0, MONO_HI); }

#if defined(FLOCKU_BOARD_TBEAM)
/// ThingPulse-style XBM: LSB within each byte row is the leftmost pixel (matches Meshtastic `drawXbm`).
static void draw_xbm_lsb(const uint8_t* xbm, int16_t w, int16_t h, int16_t ox, int16_t oy, uint16_t color) {
  const int16_t width_in_xbm = (w + 7) / 8;
  for (int16_t row = 0; row < h; ++row) {
    uint8_t data = 0;
    for (int16_t col = 0; col < w; ++col) {
      if (col & 7) {
        data >>= 1;
      } else {
        data = pgm_read_byte(xbm + (col / 8) + row * width_in_xbm);
      }
      if ((data & 0x01) != 0) {
        display.drawPixel(ox + col, oy + row, color);
      }
    }
  }
}

static void draw_satellite_glyph_1x_color(int16_t ox, int16_t oy, uint16_t color) {
  for (int16_t row = 0; row < kSatelliteIconH; ++row) {
    const uint8_t b = pgm_read_byte(&kMeshtasticSatellite8[row]);
    for (int16_t col = 0; col < kSatelliteIconW; ++col) {
      if (((b >> col) & 1) != 0) {
        display.drawPixel(ox + col, oy + row, color);
      }
    }
  }
}

static void draw_tbeam_top_bar(uint32_t now_ms, const VehicleUiState& v) {
  display.fillRect(0, 0, kScrW, kTbeamBarH, MONO_HI);
  display.setTextSize(1);

  constexpr int16_t k_sat_left = 2;
  const int16_t sy = (kTbeamBarH - kSatelliteIconH) / 2;
  if (!v.gps_ok && ((now_ms / 450) & 1u) == 0) {
    display.fillRect(k_sat_left - 1, sy - 1, kSatelliteIconW + 2, kSatelliteIconH + 2, MONO_HI);
  } else {
    draw_satellite_glyph_1x_color(k_sat_left, sy, MONO_LO);
  }

  int16_t xa = 0, ya = 0;
  uint16_t wa = 0, ha = 0;
  display.setTextColor(MONO_LO);

  char spd[16]{};
  std::snprintf(spd, sizeof(spd), "%d mph", static_cast<int>(v.speed_mph + 0.5f));
  display.getTextBounds(spd, 0, 0, &xa, &ya, &wa, &ha);
  const int16_t rx = kScrW - static_cast<int16_t>(wa) - 2;
  const int16_t ry = (kTbeamBarH - static_cast<int16_t>(ha)) / 2 - ya;
  display.setCursor(rx, ry);
  display.print(spd);

  const bool usb_only = v.battery_usb_present && !v.battery_charging;
  char pct_str[8]{};
  uint16_t pct_w = 0;
  uint16_t pct_h = 0;
  if (!usb_only) {
    if (v.battery_percent >= 0 && v.battery_percent <= 100) {
      std::snprintf(pct_str, sizeof(pct_str), "%d%%", static_cast<int>(v.battery_percent));
    } else {
      std::strncpy(pct_str, "--%", sizeof(pct_str) - 1);
    }
    display.getTextBounds(pct_str, 0, 0, &xa, &ya, &pct_w, &pct_h);
  }

  constexpr int16_t k_gap = 2;
  constexpr int16_t k_left_end = k_sat_left + kSatelliteIconW + 4;
  const int16_t icon_w = usb_only ? kMeshtasticUsbIconW : kMeshtasticBatteryIconTotalW;
  const int16_t cluster_w =
      usb_only ? icon_w : static_cast<int16_t>(icon_w + k_gap + pct_w);
  const int16_t mid_right = rx - 4;
  const int16_t cluster_x =
      static_cast<int16_t>(k_left_end + std::max(0, (mid_right - k_left_end - cluster_w) / 2));

  if (usb_only) {
    const int16_t uy = (kTbeamBarH - kMeshtasticUsbIconH) / 2;
    draw_xbm_lsb(kMeshtasticUsb10x8, kMeshtasticUsbIconW, kMeshtasticUsbIconH, cluster_x, uy, MONO_LO);
  } else {
    const int16_t bat_y = (kTbeamBarH - kMeshtasticBatteryIconH) / 2;
    draw_xbm_lsb(kMeshtasticBatteryBitmapHBottom, kMeshtasticBatteryHalfW, kMeshtasticBatteryIconH, cluster_x, bat_y,
                 MONO_LO);
    draw_xbm_lsb(kMeshtasticBatteryBitmapHTop, kMeshtasticBatteryHalfW, kMeshtasticBatteryIconH,
                 cluster_x + kMeshtasticBatteryHalfW, bat_y, MONO_LO);

    bool charging = v.battery_charging;
    if (v.battery_percent >= 100) {
      charging = false;
    }
    const bool bolt_on = charging && ((now_ms / 500) & 1u);
    int pct_fill = static_cast<int>(v.battery_percent);
    if (pct_fill < 0) {
      pct_fill = 0;
    }
    if (pct_fill > 100) {
      pct_fill = 100;
    }

    if (charging && bolt_on) {
      draw_xbm_lsb(kMeshtasticLightningBoltH, kMeshtasticBatteryHalfW, kMeshtasticBatteryIconH, cluster_x + 4, bat_y,
                   MONO_LO);
    } else {
      display.drawLine(cluster_x + 5, bat_y, cluster_x + 10, bat_y, MONO_LO);
      display.drawLine(cluster_x + 5, bat_y + 12, cluster_x + 10, bat_y + 12, MONO_LO);
      const int fill_w = 14 * pct_fill / 100;
      display.fillRect(cluster_x + 1, bat_y + 1, fill_w, 11, MONO_LO);
    }

    const int16_t tx = cluster_x + icon_w + k_gap;
    const int16_t ty = (kTbeamBarH - static_cast<int16_t>(pct_h)) / 2 - ya;
    display.setCursor(tx, ty);
    display.print(pct_str);
  }

  display.setTextColor(MONO_HI);
}
#endif

// Pending-GPS blink under glyph; sat_origin_y matches draw_satellite_glyph_at.
static void draw_gps_pending_blink(uint32_t t_ms, int16_t sat_origin_y = 0, uint16_t hi = MONO_HI) {
  if (((t_ms / 450) & 1u) == 0) return;
  display.fillRect(0, sat_origin_y + kSatelliteDisplayH, kSatelliteDisplayW, 2, hi);
}

#if defined(FLOCKU_TFT_TRACKER_UI)

static void tracker_use_classic_font() {
  display.setFont(nullptr);
  display.setTextSize(1);
}

/// Draw with a GFX proportional font; y_top is the top edge of the glyph box.
static int16_t tracker_draw_top(const GFXfont* font, int16_t x, int16_t y_top, const char* text,
                                uint16_t color) {
  if (!text || !text[0]) return y_top;
  display.setFont(font);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setTextColor(color, MONO_LO);
  display.setCursor(x, y_top - y1);
  display.print(text);
  return y_top + static_cast<int16_t>(h) + 2;
}

static int16_t tracker_text_width(const GFXfont* font, const char* text) {
  if (!text || !text[0]) return 0;
  display.setFont(font);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  return static_cast<int16_t>(w);
}

static const GFXfont* tracker_headline_font() { return &FreeSansBold12pt7b; }

/// Narrower bold for single-line status headlines that do not fit at 12pt on 160px.
static const GFXfont* tracker_headline_compact_font() { return &FreeSansBold9pt7b; }

static const GFXfont* tracker_body_font() { return &FreeSans9pt7b; }

/// Body region below dashboard on color TFT (reserve classic footer).
#if defined(FLOCKU_BOARD_LILYGO_PAGER)
static constexpr int16_t kTrackerDashH = 32;
static constexpr int16_t kTrackerBodyBottom = kScrH - 16;
static constexpr int16_t kTrackerFooterY = kScrH - 12;
static constexpr const char* kTrackerBoardTitle = "Flocku";
#else
static constexpr int16_t kTrackerDashH = 18;
static constexpr int16_t kTrackerBodyBottom = kScrH - 10;
static constexpr int16_t kTrackerFooterY = kScrH - 8;
static constexpr const char* kTrackerBoardTitle = "Wireless Tracker";
#endif

static void tracker_draw_version_footer() {
  char ver[24]{};
  std::snprintf(ver, sizeof(ver), "v%s", FIRMWARE_VERSION_STRING);
  tracker_use_classic_font();
  display.setTextSize(1);
  int16_t vbx = 0, vby = 0;
  uint16_t vw = 0, vh = 0;
  display.getTextBounds(ver, 0, 0, &vbx, &vby, &vw, &vh);
  display.setTextColor(ST77XX_WHITE, MONO_LO);
  display.setCursor(kScrW - static_cast<int16_t>(vw), kTrackerFooterY);
  display.print(ver);
}

/// Idle footer: state + loaded cam count left, version right.
static void tracker_draw_footer(const VehicleUiState& v, bool state_and_count) {
  tracker_use_classic_font();
  display.setTextSize(1);
  if (state_and_count && v.state_code[0]) {
    display.setTextColor(ST77XX_WHITE, MONO_LO);
    display.setCursor(0, kTrackerFooterY);
    display.print(v.state_code);
    display.setTextColor(ST77XX_CYAN, MONO_LO);
    display.print(F(" "));
    display.print(v.cam_count);
  }
  tracker_draw_version_footer();
}

static bool tracker_remainder_single_word(const char* p) {
  while (*p == ' ') ++p;
  if (*p == '\0') return false;
  return std::strchr(p, ' ') == nullptr;
}

/// Word wrap: break at spaces only; never split characters inside a word.
static size_t tracker_wrap_take(const char* p, const GFXfont* font, int16_t max_w) {
  if (!p || !p[0]) return 0;
  const size_t full_len = std::strlen(p);
  if (tracker_text_width(font, p) <= max_w) return full_len;

  size_t best = 0;
  size_t best_space = 0;
  for (size_t i = 0; p[i] != '\0'; ++i) {
    char probe[48]{};
    const size_t n = i + 1;
    if (n >= sizeof(probe)) break;
    std::memcpy(probe, p, n);
    probe[n] = '\0';
    if (tracker_text_width(font, probe) <= max_w) {
      best = n;
      if (p[i] == ' ') best_space = n;
    } else {
      break;
    }
  }

  if (best_space > 0) {
    if (p[best] != '\0' && tracker_remainder_single_word(p + best_space)) {
      size_t earlier_space = 0;
      for (size_t i = 0; i < best_space; ++i) {
        if (p[i] == ' ') earlier_space = i + 1;
      }
      if (earlier_space > 0) return earlier_space;
    }
    return best_space;
  }

  // Never split mid-word: emit the full first word even if wider than the line.
  size_t word_len = 0;
  while (p[word_len] != '\0' && p[word_len] != ' ') ++word_len;
  if (word_len > 0) return word_len;
  return best > 0 ? best : 1;
}

static int16_t tracker_draw_text_wrapped(int16_t y, const char* text, const GFXfont* font, uint16_t color,
                                         uint8_t max_lines) {
  const int16_t max_w = kScrW - 4;
  if (!text || !text[0] || max_lines == 0) return y;

  const char* p = text;
  uint8_t lines = 0;
  while (*p && lines < max_lines && y < kTrackerBodyBottom) {
    const size_t take = tracker_wrap_take(p, font, max_w);

    char line[48]{};
    std::strncpy(line, p, take);
    line[sizeof(line) - 1] = '\0';
    size_t trim = 0;
    while (line[trim] == ' ') ++trim;
    if (line[trim] == '\0') {
      p += take;
      while (*p == ' ') ++p;
      continue;
    }

    y = tracker_draw_top(font, 0, y, line + trim, color);
    p += take;
    while (*p == ' ') ++p;
    ++lines;
  }
  return y;
}

/// Draw headline text, wrapping to at most `max_lines` (same font as idle "No ALPR nearby").
static int16_t tracker_draw_headline_wrapped(int16_t y, const char* text, uint16_t color,
                                             uint8_t max_lines = 2) {
  return tracker_draw_text_wrapped(y, text, tracker_headline_font(), color, max_lines);
}

/// Body text (9pt); use for alert distance / cam count under the red headline.
static int16_t tracker_draw_body_wrapped(int16_t y, const char* text, uint16_t color, uint8_t max_lines = 2) {
  return tracker_draw_text_wrapped(y, text, tracker_body_font(), color, max_lines);
}

/// Fixed line breaks for long alert titles (12pt bold on 160×80).
static int16_t tracker_draw_alert_title(int16_t y, const char* title, uint16_t color) {
  const GFXfont* font = tracker_headline_font();
  if (title && std::strcmp(title, "ALPR IF LEFT") == 0) {
    y = tracker_draw_top(font, 0, y, "ALPR IF", color);
    return tracker_draw_top(font, 0, y, "LEFT", color);
  }
  if (title && std::strcmp(title, "ALPR IF RIGHT") == 0) {
    y = tracker_draw_top(font, 0, y, "ALPR IF", color);
    return tracker_draw_top(font, 0, y, "RIGHT", color);
  }
  if (title && std::strcmp(title, "AVOID INTERSECTION") == 0) {
    y = tracker_draw_top(font, 0, y, "AVOID", color);
    return tracker_draw_top(tracker_headline_compact_font(), 0, y, "INTERSECTION", color);
  }
  return tracker_draw_headline_wrapped(y, title, color, 2);
}

/// Filled direction arrow for ST7735 alert body (right margin).
static void tracker_draw_arrow_glyph(int16_t cx, int16_t cy, AlertArrow dir, uint16_t color) {
  constexpr int16_t L = 8;
  constexpr int16_t W = 6;
  switch (dir) {
    case AlertArrow::Ahead:
      display.fillTriangle(cx, cy - L, cx - W, cy + 1, cx + W, cy + 1, color);
      display.fillRect(cx - 2, cy + 1, 5, L - 3, color);
      break;
    case AlertArrow::Left:
      display.fillTriangle(cx - L, cy, cx + 1, cy - W, cx + 1, cy + W, color);
      display.fillRect(cx + 1, cy - 2, L - 3, 5, color);
      break;
    case AlertArrow::Right:
      display.fillTriangle(cx + L, cy, cx - 1, cy - W, cx - 1, cy + W, color);
      display.fillRect(cx - L + 3, cy - 2, L - 3, 5, color);
      break;
    case AlertArrow::BearRight:
      display.fillTriangle(cx + L, cy - L, cx - 4, cy + 1, cx + 2, cy + L, color);
      break;
    case AlertArrow::BearLeft:
      display.fillTriangle(cx - L, cy - L, cx + 4, cy + 1, cx - 2, cy + L, color);
      break;
    case AlertArrow::SideRight:
      display.fillTriangle(cx + L, cy, cx - W, cy - W, cx - W, cy + W, color);
      break;
    case AlertArrow::SideLeft:
      display.fillTriangle(cx - L, cy, cx + W, cy - W, cx + W, cy + W, color);
      break;
    default:
      break;
  }
}

/// RGB565 dark grey (~#333); readable white-on-grey for the status bar.
static constexpr uint16_t kTrackerDashBg = 0x3186;

/// Dark grey dashboard strip: sat icon + in-use count, optional title, SIM + mph (not on boot splash).
static void tracker_draw_dashboard_bar(uint32_t now_ms, const VehicleUiState& v) {
  display.fillRect(0, 0, kScrW, kTrackerDashH, kTrackerDashBg);
  const int16_t icon_y = (kTrackerDashH - kSatelliteDisplayH) / 2;
  const uint16_t glyph_color = v.gps_ok ? ST77XX_WHITE : ST77XX_YELLOW;
  draw_satellite_glyph_at(2, icon_y, glyph_color);
  if (!v.gps_ok) {
    draw_gps_pending_blink(now_ms, icon_y, ST77XX_ORANGE);
  }

  const GFXfont* dash_font = &FreeSans9pt7b;
#if defined(FLOCKU_BOARD_LILYGO_PAGER)
  const int16_t ty_txt = 8;
#else
  const int16_t ty_txt = 4;
#endif
  char satbuf[6]{};
  if (v.gps_sat_count_valid) {
    std::snprintf(satbuf, sizeof(satbuf), "%u", static_cast<unsigned>(v.gps_sat_count));
  } else {
    std::strncpy(satbuf, "--", sizeof(satbuf) - 1);
  }
  satbuf[sizeof(satbuf) - 1] = '\0';

  const int16_t sat_text_x = 2 + kSatelliteDisplayW + 3;
  tracker_draw_top(dash_font, sat_text_x, ty_txt, satbuf, ST77XX_WHITE);
  const int16_t cluster_right = sat_text_x + tracker_text_width(dash_font, satbuf);

  char spd[20]{};
  std::snprintf(spd, sizeof(spd), "%d mph", static_cast<int>(v.speed_mph + 0.5f));
  const int16_t mph_w = tracker_text_width(dash_font, spd);
  int16_t badge_w = 0;
  if (v.sim_mode) {
    badge_w = tracker_text_width(dash_font, "SIM");
  }
#if defined(FLOCKU_BOARD_LILYGO_PAGER)
  char bat[12]{};
  int16_t bat_w = 0;
  if (v.battery_percent >= 0) {
    std::snprintf(bat, sizeof(bat), "%d%%", static_cast<int>(v.battery_percent));
    bat_w = tracker_text_width(dash_font, bat);
  }
  const int16_t mph_x = kScrW - 3 - mph_w;
  const int16_t bat_x = mph_x - (bat_w > 0 ? (8 + bat_w) : 0);
  const int16_t badge_x = bat_x - (v.sim_mode ? (5 + badge_w) : 0);
#else
  const int16_t mph_x = kScrW - 3 - mph_w;
  const int16_t badge_x = mph_x - (v.sim_mode ? (5 + badge_w) : 0);
#endif

  const int16_t title_gap_right = (v.sim_mode ? badge_x : mph_x) - 4;
  const int16_t title_w = tracker_text_width(dash_font, kTrackerBoardTitle);
  const int16_t title_gap_left = cluster_right + 4;
  if (title_gap_right > title_gap_left && title_gap_right - title_gap_left >= title_w) {
    tracker_draw_top(dash_font, title_gap_left + (title_gap_right - title_gap_left - title_w) / 2, ty_txt,
                     kTrackerBoardTitle, ST77XX_WHITE);
  }

  if (v.sim_mode) {
    tracker_draw_top(dash_font, badge_x, ty_txt, "SIM", ST77XX_WHITE);
  }
#if defined(FLOCKU_BOARD_LILYGO_PAGER)
  if (bat_w > 0) {
    const uint16_t bat_color = v.battery_charging ? ST77XX_YELLOW : ST77XX_WHITE;
    tracker_draw_top(dash_font, bat_x, ty_txt, bat, bat_color);
  }
#endif

  tracker_draw_top(dash_font, mph_x, ty_txt, spd, ST77XX_WHITE);
  tracker_use_classic_font();
}

#if defined(FLOCKU_TFT_TRACKER_UI) && defined(FLOCKU_BOARD_HELTEC_TRACKER)

enum class HeltecScreen : uint8_t { None, Boot, Gps, NoData, Idle, Alert };

struct HeltecDashSnap {
  uint8_t gps_ok{0};
  uint8_t gps_sat_valid{0};
  uint8_t gps_sat_count{0};
  int16_t speed_mph_i{0};
  uint8_t sim_mode{0};
};

struct HeltecFooterSnap {
  uint8_t has_state{0};
  char state_code[3]{};
  uint32_t cam_count{0};
};

struct HeltecAlertSnap {
  char title[22]{};
  char line2[22]{};
  char line3[22]{};
  uint8_t arrow{0};
};

static HeltecScreen g_ht_screen{HeltecScreen::None};
static HeltecDashSnap g_ht_last_dash{};
static HeltecFooterSnap g_ht_last_footer{};
static HeltecAlertSnap g_ht_last_alert{};
static char g_ht_nodata_state[3]{};
static uint8_t g_ht_last_blink{0xff};

static HeltecDashSnap heltec_dash_snap(const VehicleUiState& v) {
  HeltecDashSnap s{};
  s.gps_ok = v.gps_ok ? 1u : 0u;
  s.gps_sat_valid = v.gps_sat_count_valid ? 1u : 0u;
  s.gps_sat_count = v.gps_sat_count;
  s.speed_mph_i = static_cast<int16_t>(v.speed_mph + 0.5f);
  s.sim_mode = v.sim_mode ? 1u : 0u;
  return s;
}

static bool heltec_dash_equal(const HeltecDashSnap& a, const HeltecDashSnap& b) {
  return a.gps_ok == b.gps_ok && a.gps_sat_valid == b.gps_sat_valid &&
         a.gps_sat_count == b.gps_sat_count && a.speed_mph_i == b.speed_mph_i &&
         a.sim_mode == b.sim_mode;
}

static HeltecFooterSnap heltec_footer_snap(const VehicleUiState& v, bool state_and_count) {
  HeltecFooterSnap s{};
  s.has_state = state_and_count ? 1u : 0u;
  if (state_and_count && v.state_code[0]) {
    s.state_code[0] = v.state_code[0];
    s.state_code[1] = v.state_code[1];
    s.cam_count = v.cam_count;
  }
  return s;
}

static bool heltec_footer_equal(const HeltecFooterSnap& a, const HeltecFooterSnap& b) {
  return a.has_state == b.has_state && a.cam_count == b.cam_count &&
         a.state_code[0] == b.state_code[0] && a.state_code[1] == b.state_code[1];
}

static HeltecAlertSnap heltec_alert_snap(const AlertUiState& a) {
  HeltecAlertSnap s{};
  std::strncpy(s.title, a.title, sizeof(s.title) - 1);
  std::strncpy(s.line2, a.line2, sizeof(s.line2) - 1);
  std::strncpy(s.line3, a.line3, sizeof(s.line3) - 1);
  s.arrow = static_cast<uint8_t>(a.arrow);
  return s;
}

static bool heltec_alert_equal(const HeltecAlertSnap& a, const HeltecAlertSnap& b) {
  return a.arrow == b.arrow && std::strcmp(a.title, b.title) == 0 &&
         std::strcmp(a.line2, b.line2) == 0 && std::strcmp(a.line3, b.line3) == 0;
}

static uint8_t heltec_blink_phase(uint32_t now_ms, const VehicleUiState& v) {
  if (v.gps_ok) return 0;
  return static_cast<uint8_t>((now_ms / 450) & 1u);
}

static void heltec_clear_body_region() {
  display.fillRect(0, kTrackerDashH, kScrW, kTrackerFooterY - kTrackerDashH, MONO_LO);
}

static void heltec_clear_footer_region() {
  display.fillRect(0, kTrackerFooterY, kScrW, kScrH - kTrackerFooterY, MONO_LO);
}

static void heltec_update_blink_only(uint32_t now_ms, const VehicleUiState& v) {
  const int16_t icon_y = (kTrackerDashH - kSatelliteDisplayH) / 2;
  display.fillRect(0, icon_y + kSatelliteDisplayH, kSatelliteDisplayW + 4, 3, kTrackerDashBg);
  if (!v.gps_ok) draw_gps_pending_blink(now_ms, icon_y, ST77XX_ORANGE);
}

static void heltec_paint_dashboard(uint32_t now_ms, const VehicleUiState& v) {
  tracker_draw_dashboard_bar(now_ms, v);
  g_ht_last_dash = heltec_dash_snap(v);
  g_ht_last_blink = heltec_blink_phase(now_ms, v);
}

/// Dashboard strip or GPS blink bar only — skip when unchanged.
static void heltec_update_dashboard(uint32_t now_ms, const VehicleUiState& v) {
  const HeltecDashSnap cur = heltec_dash_snap(v);
  const uint8_t blink = heltec_blink_phase(now_ms, v);
  if (!heltec_dash_equal(cur, g_ht_last_dash)) {
    heltec_paint_dashboard(now_ms, v);
    return;
  }
  if (!v.gps_ok && blink != g_ht_last_blink) {
    heltec_update_blink_only(now_ms, v);
    g_ht_last_blink = blink;
  }
}

static void heltec_paint_version_footer() {
  heltec_clear_footer_region();
  tracker_draw_version_footer();
  g_ht_last_footer = {};
}

static void heltec_paint_state_footer(const VehicleUiState& v) {
  heltec_clear_footer_region();
  tracker_draw_footer(v, true);
  g_ht_last_footer = heltec_footer_snap(v, true);
}

static void heltec_paint_gps_body() {
  const int16_t body_top = kTrackerDashH + 2;
  int16_t y = body_top;
  y = tracker_draw_top(tracker_headline_font(), 0, y, "GPS", ST77XX_WHITE);
  y = tracker_draw_top(tracker_body_font(), 0, y, "SEARCHING", ST77XX_WHITE);
  tracker_draw_top(tracker_body_font(), 0, y, "Waiting for lock...", ST77XX_WHITE);
  tracker_use_classic_font();
}

static void heltec_paint_nodata_body(const char* state_code) {
  const int16_t body_top = kTrackerDashH + 2;
  int16_t y = body_top;
  y = tracker_draw_top(tracker_headline_compact_font(), 0, y, "NO DATA FILE", ST77XX_WHITE);
  char path_line[20]{"/cams/??.bin"};
  if (state_code && std::strlen(state_code) >= 2) {
    std::snprintf(path_line, sizeof(path_line), "/cams/%c%c.bin",
                  static_cast<char>(std::toupper(static_cast<unsigned char>(state_code[0]))),
                  static_cast<char>(std::toupper(static_cast<unsigned char>(state_code[1]))));
  }
  y = tracker_draw_top(tracker_body_font(), 0, y, path_line, ST77XX_WHITE);
  tracker_draw_top(tracker_body_font(), 0, y, "missing", ST77XX_WHITE);
  tracker_use_classic_font();
}

static void heltec_paint_idle_body() {
  const int16_t body_y = kTrackerDashH + 4;
  int16_t y = body_y;
  y = tracker_draw_top(tracker_headline_font(), 0, y, "No ALPR", ST77XX_GREEN);
  tracker_draw_top(tracker_headline_font(), 0, y, "nearby", ST77XX_GREEN);
  tracker_use_classic_font();
}

static void heltec_paint_alert_body(const AlertUiState& a) {
  const int16_t y = kTrackerDashH + 4;
  int16_t yy = y;
  yy = tracker_draw_alert_title(yy, a.title, ST77XX_RED);
  if (a.line2[0]) yy = tracker_draw_body_wrapped(yy, a.line2, ST77XX_WHITE, 1);
  if (a.line3[0]) yy = tracker_draw_body_wrapped(yy, a.line3, ST77XX_WHITE, 1);
  if (a.arrow != AlertArrow::None) {
    const int16_t arrow_cy = (y + std::min<int16_t>(yy, kTrackerBodyBottom)) / 2;
    tracker_draw_arrow_glyph(kScrW - 14, arrow_cy, a.arrow, ST77XX_RED);
  }
  tracker_use_classic_font();
}

static void heltec_reset_paint_state() {
  g_ht_screen = HeltecScreen::None;
  g_ht_last_blink = 0xff;
}

static void heltec_show_gps(const VehicleUiState& v) {
  const uint32_t now = millis();
  if (g_ht_screen != HeltecScreen::Gps) {
    g_ht_screen = HeltecScreen::Gps;
    disp_clear();
    heltec_paint_dashboard(now, v);
    heltec_paint_gps_body();
    heltec_paint_version_footer();
    tracker_idle_mark_content_stale();
    return;
  }
  heltec_update_dashboard(now, v);
}

static void heltec_show_nodata(const VehicleUiState& v, const char* state_code) {
  const uint32_t now = millis();
  char st[3]{"--"};
  if (state_code && state_code[0]) {
    st[0] = state_code[0];
    st[1] = state_code[1];
  }
  const bool state_changed = st[0] != g_ht_nodata_state[0] || st[1] != g_ht_nodata_state[1];
  if (g_ht_screen != HeltecScreen::NoData) {
    g_ht_screen = HeltecScreen::NoData;
    g_ht_nodata_state[0] = st[0];
    g_ht_nodata_state[1] = st[1];
    g_ht_nodata_state[2] = '\0';
    disp_clear();
    heltec_paint_dashboard(now, v);
    heltec_paint_nodata_body(state_code);
    heltec_paint_version_footer();
    tracker_idle_mark_content_stale();
    return;
  }
  if (state_changed) {
    g_ht_nodata_state[0] = st[0];
    g_ht_nodata_state[1] = st[1];
    heltec_clear_body_region();
    heltec_paint_nodata_body(state_code);
  }
  heltec_update_dashboard(now, v);
}

static void heltec_show_idle(const VehicleUiState& v) {
  const uint32_t now = millis();
  if (g_ht_screen != HeltecScreen::Idle) {
    g_ht_screen = HeltecScreen::Idle;
    disp_clear();
    heltec_paint_dashboard(now, v);
    heltec_paint_idle_body();
    heltec_paint_state_footer(v);
    tracker_idle_note_repainted(v);
    return;
  }
  const HeltecFooterSnap fcur = heltec_footer_snap(v, true);
  if (!heltec_footer_equal(fcur, g_ht_last_footer)) {
    heltec_paint_state_footer(v);
  }
  heltec_update_dashboard(now, v);
}

static void heltec_show_alert(const VehicleUiState& v, const AlertUiState& a) {
  const uint32_t now = millis();
  const HeltecAlertSnap cur = heltec_alert_snap(a);
  if (g_ht_screen != HeltecScreen::Alert) {
    g_ht_screen = HeltecScreen::Alert;
    disp_clear();
    heltec_paint_dashboard(now, v);
    heltec_paint_alert_body(a);
    heltec_paint_version_footer();
    g_ht_last_alert = cur;
    tracker_idle_mark_content_stale();
    return;
  }
  if (!heltec_alert_equal(cur, g_ht_last_alert)) {
    heltec_clear_body_region();
    heltec_paint_alert_body(a);
    g_ht_last_alert = cur;
  }
  heltec_update_dashboard(now, v);
}

#endif  // FLOCKU_TFT_TRACKER_UI && FLOCKU_BOARD_HELTEC_TRACKER

#if defined(FLOCKU_BOARD_LILYGO_PAGER)
// Bruce boot_screen_anim: drawPixel(0,0) forces TFT comms and clears ghosting from prior firmware.
static void pager_force_tft_sync() { g_tft.drawPixel(0, 0, TFT_BLACK); }

/// Boot/splash only — paints directly to the panel (not the ops sprite).
static void pager_panel_fill_black() {
  pager_force_tft_sync();
  const int16_t pw = g_tft.width();
  const int16_t ph = g_tft.height();
  g_tft.fillScreen(TFT_BLACK);
  g_tft.fillRect(0, 0, pw, ph, TFT_BLACK);
}

/// ST7796 222x480 panel: rotation-3 viewport is 480x222 with rowstart=49 (needs CGRAM_OFFSET).
/// Also black the native 222x480 GRAM so dead rows outside the viewport lose prior firmware text.
static void pager_scrub_entire_panel() {
  const uint8_t rot = g_tft.getRotation();
  pager_panel_fill_black();
  g_tft.setRotation(0);
  g_tft.fillScreen(TFT_BLACK);
  g_tft.setRotation(rot);
  pager_panel_fill_black();
  pager_force_tft_sync();
}

static bool pager_in_splash_phase(uint32_t now_ms, const VehicleUiState& v) {
  if (g_pager_boot_ms == 0) g_pager_boot_ms = now_ms;
  if (v.gps_ok) return false;
  return (now_ms - g_pager_boot_ms) < PAGER_SPLASH_MAX_MS;
}

/// Splash until GPS lock or PAGER_SPLASH_MAX_MS; then return false so ops UI runs.
static bool pager_try_splash(uint32_t now_ms, const VehicleUiState& v) {
  static bool splash_painted;
  static bool was_in_splash{true};
  const bool in_splash = pager_in_splash_phase(now_ms, v);
  if (in_splash) {
    if (!splash_painted) {
      if (!g_pager_initial_splash) {
        pager_scrub_entire_panel();
        g_tft.drawBitmap(0, 0, kPagerBoot, kPagerBootW, kPagerBootH, TFT_RED, TFT_BLACK);
        pager_force_tft_sync();
      }
      splash_painted = true;
    }
    was_in_splash = true;
    return true;
  }
  if (was_in_splash) {
    was_in_splash = false;
    splash_painted = false;
    g_pager_has_last = false;
    tracker_idle_mark_content_stale();
  }
  return false;
}

static constexpr int16_t kPagerBarH = 36;
static constexpr uint16_t kPagerDashBg = 0x3186;

static PagerSnap pager_snap_from(PagerScr scr, const VehicleUiState& v, const AlertUiState* alert,
                                 const char* state_code) {
  PagerSnap s{};
  s.scr = scr;
  s.gps_ok = v.gps_ok ? 1u : 0u;
  s.gps_sat_valid = v.gps_sat_count_valid ? 1u : 0u;
  s.gps_sat_count = v.gps_sat_count;
  s.speed_mph_i = static_cast<int16_t>(v.speed_mph + 0.5f);
  s.battery_percent = v.battery_percent;
  s.battery_charging = v.battery_charging ? 1u : 0u;
  s.sim_mode = v.sim_mode ? 1u : 0u;
  s.state_code[0] = v.state_code[0];
  s.state_code[1] = v.state_code[1];
  s.state_code[2] = v.state_code[2];
  s.cam_count = v.cam_count;
  if (state_code) {
    std::strncpy(s.nodata_path, state_code, sizeof(s.nodata_path) - 1);
    s.nodata_path[sizeof(s.nodata_path) - 1] = '\0';
  }
  if (alert) s.alert = *alert;
  return s;
}

static bool pager_snaps_equal(const PagerSnap& a, const PagerSnap& b) {
  if (a.scr != b.scr) return false;
  if (a.gps_ok != b.gps_ok || a.gps_sat_valid != b.gps_sat_valid || a.gps_sat_count != b.gps_sat_count ||
      a.speed_mph_i != b.speed_mph_i || a.battery_percent != b.battery_percent ||
      a.battery_charging != b.battery_charging || a.sim_mode != b.sim_mode ||
      a.state_code[0] != b.state_code[0] || a.state_code[1] != b.state_code[1] ||
      a.cam_count != b.cam_count) {
    return false;
  }
  if (a.scr == PagerScr::NoData) {
    return std::strncmp(a.nodata_path, b.nodata_path, sizeof(a.nodata_path)) == 0;
  }
  if (a.scr == PagerScr::Alert) {
    return a.alert.active == b.alert.active && a.alert.arrow == b.alert.arrow &&
           std::strncmp(a.alert.title, b.alert.title, sizeof(a.alert.title)) == 0 &&
           std::strncmp(a.alert.line2, b.alert.line2, sizeof(a.alert.line2)) == 0 &&
           std::strncmp(a.alert.line3, b.alert.line3, sizeof(a.alert.line3)) == 0;
  }
  return true;
}

static bool pager_should_redraw(PagerScr scr, const VehicleUiState& v, const AlertUiState* alert,
                                const char* state_code, uint32_t now_ms) {
  const PagerSnap cur = pager_snap_from(scr, v, alert, state_code);
  if (!g_pager_has_last) return true;
  if (!pager_snaps_equal(cur, g_pager_last)) return true;
  if (now_ms - g_pager_last_flush_ms < PAGER_TFT_FULL_REDRAW_MIN_MS) return false;
  return false;
}

static void pager_note_redraw(PagerScr scr, const VehicleUiState& v, const AlertUiState* alert,
                              const char* state_code, uint32_t now_ms) {
  g_pager_last = pager_snap_from(scr, v, alert, state_code);
  g_pager_has_last = true;
  g_pager_last_flush_ms = now_ms;
}

static void pager_draw_sat(TFT_eSprite& s, int16_t ox, int16_t oy, uint16_t color) {
  for (int16_t row = 0; row < kSatelliteIconH; ++row) {
    const uint8_t b = pgm_read_byte(&kMeshtasticSatellite8[row]);
    for (int16_t col = 0; col < kSatelliteIconW; ++col) {
      if (((b >> col) & 1) != 0) {
        s.fillRect(ox + col * kSatelliteDisplayScale, oy + row * kSatelliteDisplayScale,
                   kSatelliteDisplayScale, kSatelliteDisplayScale, color);
      }
    }
  }
}

static void pager_reset_text(TFT_eSprite& s) {
  s.setFreeFont(nullptr);
  s.setTextFont(1);
  s.setTextSize(1);
  s.setTextDatum(TL_DATUM);
}

static void pager_draw_text(TFT_eSprite& s, int16_t x, int16_t y, const char* text, uint16_t color,
                            uint8_t size) {
  if (!text || !text[0]) return;
  pager_reset_text(s);
  s.setTextSize(size);
  s.setTextColor(color);
  s.setTextDatum(TL_DATUM);
  s.drawString(text, x, y);
}

static void pager_draw_status_bar(TFT_eSprite& s, const VehicleUiState& v) {
  s.fillRect(0, 0, kScrW, kPagerBarH, kPagerDashBg);
  const int16_t icon_y = (kPagerBarH - kSatelliteDisplayH) / 2;
  const uint16_t glyph_color = v.gps_ok ? TFT_WHITE : TFT_YELLOW;
  pager_draw_sat(s, 8, icon_y, glyph_color);

  pager_reset_text(s);
  s.setTextSize(2);
  s.setTextColor(TFT_WHITE);

  char satbuf[8]{};
  if (v.gps_sat_count_valid) {
    std::snprintf(satbuf, sizeof(satbuf), "%u", static_cast<unsigned>(v.gps_sat_count));
  } else {
    std::strncpy(satbuf, "--", sizeof(satbuf) - 1);
  }
  s.setTextDatum(ML_DATUM);
  s.drawString(satbuf, 8 + kSatelliteDisplayW + 6, kPagerBarH / 2);

  s.setTextDatum(MC_DATUM);
  s.drawString("Flocku", kScrW / 2, kPagerBarH / 2);

  char spd[20]{};
  std::snprintf(spd, sizeof(spd), "%d mph", static_cast<int>(v.speed_mph + 0.5f));
  int16_t rx = kScrW - 8;
  if (v.sim_mode) {
    s.setTextDatum(MR_DATUM);
    s.drawString("SIM", rx, kPagerBarH / 2);
    rx -= s.textWidth("SIM") + 10;
  }
  if (v.battery_percent >= 0) {
    char bat[8]{};
    std::snprintf(bat, sizeof(bat), "%d%%", static_cast<int>(v.battery_percent));
    s.setTextColor(v.battery_charging ? TFT_YELLOW : TFT_WHITE);
    s.setTextDatum(MR_DATUM);
    s.drawString(bat, rx, kPagerBarH / 2);
    rx -= s.textWidth(bat) + 10;
    s.setTextColor(TFT_WHITE);
  }
  s.setTextDatum(MR_DATUM);
  s.drawString(spd, rx, kPagerBarH / 2);
  pager_reset_text(s);
}

static void pager_draw_footer(TFT_eSprite& s, const VehicleUiState& v, bool state_and_count) {
  constexpr int16_t kFootY = kScrH - 6;
  pager_reset_text(s);
  s.setTextColor(TFT_WHITE);
  if (state_and_count && v.state_code[0]) {
    char left[24]{};
    std::snprintf(left, sizeof(left), "%s %lu", v.state_code, static_cast<unsigned long>(v.cam_count));
    s.setTextDatum(BL_DATUM);
    s.drawString(left, 8, kFootY);
  }
  char ver[24]{};
  std::snprintf(ver, sizeof(ver), "v%s", FIRMWARE_VERSION_STRING);
  s.setTextDatum(BR_DATUM);
  s.drawString(ver, kScrW - 8, kFootY);
  pager_reset_text(s);
}

static void pager_draw_headline(TFT_eSprite& s, int16_t y, const char* line1, const char* line2, uint16_t color) {
  if (line1 && line1[0]) {
    pager_draw_text(s, 16, y, line1, color, 3);
    y += 30;
  }
  if (line2 && line2[0]) {
    pager_draw_text(s, 16, y, line2, color, 3);
  }
}

static void pager_draw_body_lines(TFT_eSprite& s, int16_t y, const char* line1, const char* line2, uint16_t color) {
  if (line1 && line1[0]) {
    pager_draw_text(s, 16, y, line1, color, 2);
    y += 20;
  }
  if (line2 && line2[0]) {
    pager_draw_text(s, 16, y, line2, color, 2);
  }
}

static void pager_draw_arrow(TFT_eSprite& s, int16_t cx, int16_t cy, AlertArrow dir, uint16_t color) {
  constexpr int16_t L = 16;
  constexpr int16_t W = 12;
  switch (dir) {
    case AlertArrow::Ahead:
      s.fillTriangle(cx, cy - L, cx - W, cy + 2, cx + W, cy + 2, color);
      s.fillRect(cx - 4, cy + 2, 9, L - 4, color);
      break;
    case AlertArrow::Left:
      s.fillTriangle(cx - L, cy, cx + 2, cy - W, cx + 2, cy + W, color);
      s.fillRect(cx + 2, cy - 4, L - 4, 9, color);
      break;
    case AlertArrow::Right:
      s.fillTriangle(cx + L, cy, cx - 2, cy - W, cx - 2, cy + W, color);
      s.fillRect(cx - L + 4, cy - 4, L - 4, 9, color);
      break;
    default:
      break;
  }
}

static void pager_render_and_flush(PagerScr scr, const VehicleUiState& v, const AlertUiState* alert,
                                   const char* state_code, uint32_t now_ms) {
  auto& s = pager_pg();
  s.fillScreen(TFT_BLACK);
  pager_draw_status_bar(s, v);
  switch (scr) {
    case PagerScr::Gps:
      pager_draw_headline(s, kPagerBarH + 20, "GPS", "SEARCHING", TFT_WHITE);
      pager_draw_body_lines(s, kPagerBarH + 88, "Waiting for lock...", nullptr, TFT_WHITE);
      pager_draw_footer(s, v, false);
      break;
    case PagerScr::NoData: {
      pager_draw_headline(s, kPagerBarH + 20, "NO DATA", "FILE", TFT_WHITE);
      char path_line[20]{"/cams/??.bin"};
      if (state_code && std::strlen(state_code) >= 2) {
        std::snprintf(path_line, sizeof(path_line), "/cams/%c%c.bin",
                      static_cast<char>(std::toupper(static_cast<unsigned char>(state_code[0]))),
                      static_cast<char>(std::toupper(static_cast<unsigned char>(state_code[1]))));
      }
      pager_draw_body_lines(s, kPagerBarH + 88, path_line, "missing", TFT_WHITE);
      pager_draw_footer(s, v, false);
      break;
    }
    case PagerScr::Idle:
      pager_draw_headline(s, kPagerBarH + 20, "No ALPR", "nearby", TFT_GREEN);
      pager_draw_footer(s, v, true);
      break;
    case PagerScr::Alert:
      if (alert) {
        int16_t y = kPagerBarH + 16;
        if (alert->title[0]) {
          pager_draw_text(s, 16, y, alert->title, TFT_RED, 3);
          y += 30;
        }
        pager_draw_body_lines(s, y, alert->line2[0] ? alert->line2 : nullptr,
                              alert->line3[0] ? alert->line3 : nullptr, TFT_WHITE);
        if (alert->arrow != AlertArrow::None) {
          pager_draw_arrow(s, kScrW - 28, kPagerBarH + 80, alert->arrow, TFT_RED);
        }
      }
      pager_draw_footer(s, v, false);
      break;
  }
  pager_flush();
  pager_note_redraw(scr, v, alert, state_code, now_ms);
}
#endif

/// Boot splash: red fox on black from `boot_logo_tracker_bitmap.h` (Heltec 160x80).
static void tracker_boot_screen() {
  display.fillScreen(ST77XX_BLACK);
  const int x0 = (kScrW - kTrackerBootW) / 2;
  const int y0 = (kScrH - kTrackerBootH) / 2;
  display.drawBitmap(x0, y0, kTrackerBoot, kTrackerBootW, kTrackerBootH, ST77XX_RED);
}
#endif

bool DisplayManager::ensure_display() { return inited_; }

bool DisplayManager::begin() {
#if defined(HELTEC_VEXT_PIN)
  pinMode(HELTEC_VEXT_PIN, OUTPUT);
#if defined(HELTEC_VEXT_ACTIVE_HIGH)
  digitalWrite(HELTEC_VEXT_PIN, HIGH);
#else
  digitalWrite(HELTEC_VEXT_PIN, LOW);
#endif
  delay(20);
#endif
#if defined(FLOCKU_BOARD_HELTEC_TRACKER)
  // Vext enables LDO for TFT + GNSS path; wait before SPI (Heltec Wireless Tracker v1.1).
  delay(120);
  pinMode(ST7735_BL_PIN, OUTPUT);
  digitalWrite(ST7735_BL_PIN, HIGH);
  SPI.begin(ST7735_SCK, /*miso=*/-1, ST7735_MOSI, /*ss=*/-1);
#if defined(ARDUINO_ARCH_ESP32)
  SPI.setFrequency(ST7735_SPI_MAX_HZ);
#endif
  display.initR(INITR_MINI160x80_PLUGIN);
  display.setRotation(1);
#elif defined(FLOCKU_BOARD_LILYGO_PAGER)
  (void)pager_board_begin();
  pinMode(PAGER_TFT_BL_PIN, OUTPUT);
  digitalWrite(PAGER_TFT_BL_PIN, HIGH);
  g_tft.init();
  g_tft.setRotation(PAGER_TFT_ROTATION);
  g_pager_boot_ms = millis();
  // Panel requires TFT_INVERSION_ON + CGRAM_OFFSET (platformio.ini). Do not call invertDisplay(false).
  pager_scrub_entire_panel();
  g_tft.drawBitmap(0, 0, kPagerBoot, kPagerBootW, kPagerBootH, TFT_RED, TFT_BLACK);
  pager_force_tft_sync();
  g_pager_initial_splash = true;
  (void)battery_monitor_begin();
#elif !defined(FLOCKU_TFT_TRACKER_UI)
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
#endif
#if defined(FLOCKU_BOARD_TBEAM)
  (void)battery_monitor_begin();
#endif

#if defined(FLOCKU_TFT_TRACKER_UI)
#if !defined(FLOCKU_BOARD_LILYGO_PAGER)
  disp_clear();
#endif
  display.setTextColor(MONO_HI);
  display.setTextSize(1);
#else
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    inited_ = false;
    return false;
  }
  disp_clear();
  display.setTextColor(MONO_HI);
  display.setTextSize(1);
  disp_show();  // Push cleared buffer (DISPLAYON before first transfer shows garbage).
#endif
  inited_ = true;
  return true;
}

void DisplayManager::invalidate_screen() {
#if defined(FLOCKU_TFT_TRACKER_UI)
  tracker_idle_mark_content_stale();
#if defined(FLOCKU_BOARD_HELTEC_TRACKER)
  heltec_reset_paint_state();
#endif
#endif
}

void DisplayManager::show_boot_acquiring(uint32_t now_ms, const VehicleUiState& v) {
  if (!inited_) return;
#if defined(FLOCKU_BOARD_LILYGO_PAGER)
  if (pager_try_splash(now_ms, v)) return;
#endif
#if defined(FLOCKU_TFT_TRACKER_UI)
#if defined(FLOCKU_BOARD_HELTEC_TRACKER)
  static bool s_boot_done;
  if (s_boot_done) return;
  s_boot_done = true;
  g_ht_screen = HeltecScreen::Boot;
#else
  static uint32_t s_last_boot_paint;
  if (tft_full_paint_skip(s_last_boot_paint)) return;
#endif
#endif
  disp_clear();

#if defined(FLOCKU_TFT_TRACKER_UI)
  tracker_boot_screen();
  disp_show();
  tracker_idle_mark_content_stale();
  return;
#endif

#if defined(FLOCKU_BOARD_TBEAM)
  draw_tbeam_top_bar(now_ms, v);
  constexpr int k_fox_y = 22;
#else
  constexpr int k_fox_y = 10;
#endif

  const int x0 = (kScrW - kBootLogoW) / 2;
  display.drawBitmap(x0, k_fox_y, kBootLogo, kBootLogoW, kBootLogoH, MONO_HI);

#if !defined(FLOCKU_BOARD_TBEAM)
  draw_satellite_glyph();
  if (!v.gps_ok) {
    draw_gps_pending_blink(now_ms);
  }
#endif

  display.setTextSize(1);
  display.setTextColor(MONO_HI);

  int16_t bx = 0, by = 0;
  uint16_t tw = 0, th = 0;
  constexpr int k_bottom_y = kScrH - 8;
  char ver[20]{};
  std::snprintf(ver, sizeof(ver), "v%s", FIRMWARE_VERSION_STRING);
  display.getTextBounds(ver, 0, 0, &bx, &by, &tw, &th);
  int vx = kScrW - static_cast<int>(tw);
  int vy = k_bottom_y;
  if (vy + static_cast<int>(th) > kScrH - 1) {
    vy = (kScrH - 1) - static_cast<int>(th);
  }
  if (vx < 0) vx = 0;
  display.setCursor(vx, vy);
  display.print(ver);

  disp_show();
}

void DisplayManager::show_gps_searching(const VehicleUiState& v) {
  if (!inited_) return;
#if defined(FLOCKU_BOARD_LILYGO_PAGER)
  const uint32_t now = millis();
  if (pager_try_splash(now, v)) return;
  if (!pager_should_redraw(PagerScr::Gps, v, nullptr, nullptr, now)) return;
  pager_render_and_flush(PagerScr::Gps, v, nullptr, nullptr, now);
  return;
#elif defined(FLOCKU_TFT_TRACKER_UI)
#if defined(FLOCKU_BOARD_HELTEC_TRACKER)
  heltec_show_gps(v);
  return;
#else
  static uint32_t s_last_gps_paint;
  if (tft_full_paint_skip(s_last_gps_paint)) return;
#endif
#endif
  disp_clear();
#if defined(FLOCKU_BOARD_TBEAM)
  draw_tbeam_top_bar(millis(), v);
  const int16_t body_top = kTbeamBarH + 2;
#elif defined(FLOCKU_TFT_TRACKER_UI)
  tracker_draw_dashboard_bar(millis(), v);
  const int16_t body_top = kTrackerDashH + 2;
#else
  const int16_t body_top = 0;
  const uint32_t t = millis();
  draw_satellite_glyph();
  draw_gps_pending_blink(t);
#endif
  display.setTextColor(MONO_HI);
#if defined(FLOCKU_TFT_TRACKER_UI)
  {
    int16_t y = body_top + 2;
    y = tracker_draw_top(tracker_headline_font(), 0, y, "GPS", ST77XX_WHITE);
    y = tracker_draw_top(tracker_body_font(), 0, y, "SEARCHING", ST77XX_WHITE);
    tracker_draw_top(tracker_body_font(), 0, y, "Waiting for lock...", ST77XX_WHITE);
    tracker_use_classic_font();
    tracker_draw_footer(v, false);
  }
#else
  display.setTextSize(2);
#if defined(FLOCKU_BOARD_TBEAM)
  display.setCursor(0, body_top + 2);
#else
  display.setCursor(0, body_top + kSatelliteDisplayH + 2);
#endif
  display.println(F("GPS"));
  display.setTextSize(1);
  display.setCursor(0, display.getCursorY() + 2);
  display.println(F("SEARCHING"));
  display.println(F("Waiting for lock..."));
#endif
  disp_show();
#if defined(FLOCKU_TFT_TRACKER_UI)
  tracker_idle_mark_content_stale();
#endif
}

void DisplayManager::show_no_data_file(const VehicleUiState& v, const char* state_code) {
  if (!inited_) return;
#if defined(FLOCKU_BOARD_LILYGO_PAGER)
  const uint32_t now = millis();
  if (pager_try_splash(now, v)) return;
  if (!pager_should_redraw(PagerScr::NoData, v, nullptr, state_code, now)) return;
  pager_render_and_flush(PagerScr::NoData, v, nullptr, state_code, now);
  return;
#elif defined(FLOCKU_TFT_TRACKER_UI)
#if defined(FLOCKU_BOARD_HELTEC_TRACKER)
  heltec_show_nodata(v, state_code);
  return;
#else
  static uint32_t s_last_nodata_paint;
  if (tft_full_paint_skip(s_last_nodata_paint)) return;
#endif
#endif
  disp_clear();
#if defined(FLOCKU_BOARD_TBEAM)
  draw_tbeam_top_bar(millis(), v);
  const int16_t body_top = kTbeamBarH + 2;
#elif defined(FLOCKU_TFT_TRACKER_UI)
  tracker_draw_dashboard_bar(millis(), v);
  const int16_t body_top = kTrackerDashH + 2;
#else
  const int16_t body_top = 0;
  draw_satellite_glyph();
#endif
  display.setTextColor(MONO_HI);
#if defined(FLOCKU_TFT_TRACKER_UI)
  {
    int16_t y = body_top + 2;
    y = tracker_draw_top(tracker_headline_compact_font(), 0, y, "NO DATA FILE", ST77XX_WHITE);
    char path_line[20]{"/cams/??.bin"};
    if (state_code && std::strlen(state_code) >= 2) {
      std::snprintf(path_line, sizeof(path_line), "/cams/%c%c.bin",
                    static_cast<char>(std::toupper(static_cast<unsigned char>(state_code[0]))),
                    static_cast<char>(std::toupper(static_cast<unsigned char>(state_code[1]))));
    }
    y = tracker_draw_top(tracker_body_font(), 0, y, path_line, ST77XX_WHITE);
    tracker_draw_top(tracker_body_font(), 0, y, "missing", ST77XX_WHITE);
    tracker_use_classic_font();
    tracker_draw_footer(v, false);
  }
#else
#if defined(FLOCKU_BOARD_TBEAM)
  display.setCursor(0, body_top + 2);
#else
  display.setCursor(0, body_top + kSatelliteDisplayH + 2);
#endif
  display.setTextSize(2);
  display.println(F("NO DATA"));
  display.setTextSize(1);
  display.println(F("FILE"));
  display.print(F("State: "));
  display.println(state_code ? state_code : "--");
  char path_line[20]{"/cams/??.bin"};
  if (state_code && std::strlen(state_code) >= 2) {
    std::snprintf(path_line, sizeof(path_line), "/cams/%c%c.bin",
                  static_cast<char>(std::toupper(static_cast<unsigned char>(state_code[0]))),
                  static_cast<char>(std::toupper(static_cast<unsigned char>(state_code[1]))));
  }
  display.println(path_line);
  display.println(F("missing"));
#endif
  disp_show();
#if defined(FLOCKU_TFT_TRACKER_UI)
  tracker_idle_mark_content_stale();
#endif
}

void DisplayManager::show_idle(const VehicleUiState& v) {
  if (!inited_) return;
#if defined(FLOCKU_BOARD_LILYGO_PAGER)
  const uint32_t now = millis();
  if (pager_try_splash(now, v)) return;
  if (!pager_should_redraw(PagerScr::Idle, v, nullptr, nullptr, now)) return;
  pager_render_and_flush(PagerScr::Idle, v, nullptr, nullptr, now);
  return;
#else
#if defined(FLOCKU_TFT_TRACKER_UI)
#if defined(FLOCKU_BOARD_HELTEC_TRACKER)
  heltec_show_idle(v);
  return;
#else
  if (!tracker_idle_should_repaint(v)) return;
#endif
#else
  const uint32_t now_throttle = millis();
  if (now_throttle - last_idle_draw_ < min_idle_redraw_ms_) return;
  last_idle_draw_ = now_throttle;
#endif

  const uint32_t now = millis();

  disp_clear();

#if defined(FLOCKU_BOARD_TBEAM)
  draw_tbeam_top_bar(now, v);
  const int16_t body_y = kTbeamBarH + 2;
  display.setTextSize(1);
#else
  display.setTextSize(1);
#if defined(FLOCKU_TFT_TRACKER_UI)
  tracker_draw_dashboard_bar(now, v);
  constexpr int16_t body_y = kTrackerDashH + 4;
#else
  const uint32_t t = millis();
  static constexpr int16_t k_idle_sat_y = -6;
  draw_satellite_glyph_at(0, k_idle_sat_y, MONO_HI);
  if (!v.gps_ok) {
    draw_gps_pending_blink(t, k_idle_sat_y);
  }

  char speed_right[16]{};
  std::snprintf(speed_right, sizeof(speed_right), "%d mph",
                static_cast<int>(v.speed_mph + 0.5f));

  int16_t x1 = 0, y1 = 0;
  uint16_t w_right = 0, h_right = 0;
  display.getTextBounds(speed_right, 0, 0, &x1, &y1, &w_right, &h_right);
  constexpr int k_classic_font_h = 8;
  const int16_t ty_speed = std::max<int16_t>(
      0, static_cast<int16_t>(k_idle_sat_y + (kSatelliteDisplayH - k_classic_font_h) / 2));
  display.setTextColor(MONO_HI);
  display.setCursor(kScrW - static_cast<int>(w_right), ty_speed);
  display.print(speed_right);

  if (v.sim_mode) {
    display.setTextColor(MONO_HI);
    display.setCursor(kSatelliteDisplayW + 2, ty_speed);
    display.print(F("SIM"));
  }

  const int16_t body_y = k_idle_sat_y + kSatelliteDisplayH + 2;
#endif
#endif

#if defined(FLOCKU_TFT_TRACKER_UI)
  {
    int16_t y = body_y;
    y = tracker_draw_top(tracker_headline_font(), 0, y, "No ALPR", ST77XX_GREEN);
    tracker_draw_top(tracker_headline_font(), 0, y, "nearby", ST77XX_GREEN);
    tracker_use_classic_font();
    tracker_draw_footer(v, true);
  }
#else
  display.setCursor(0, body_y);
  display.setTextSize(2);
  display.println(F("No ALPR"));
  display.println(F("nearby"));
#endif

#if !defined(FLOCKU_TFT_TRACKER_UI)
  constexpr int k_bottom_y = kScrH - 8;
  display.setTextSize(1);
  display.setCursor(0, k_bottom_y);
  if (v.state_code[0]) {
    display.print(v.state_code);
  } else {
    display.print(F("--"));
  }
  display.print(F(" "));
  display.print(v.cam_count);
#endif
  disp_show();
#if defined(FLOCKU_TFT_TRACKER_UI)
  tracker_idle_note_repainted(v);
#endif
#endif  // !FLOCKU_BOARD_LILYGO_PAGER
}

void DisplayManager::show_alert(const VehicleUiState& v, const AlertUiState& a) {
  if (!inited_) return;
#if defined(FLOCKU_BOARD_LILYGO_PAGER)
  const uint32_t now = millis();
  if (pager_try_splash(now, v)) return;
  if (!pager_should_redraw(PagerScr::Alert, v, &a, nullptr, now)) return;
  pager_render_and_flush(PagerScr::Alert, v, &a, nullptr, now);
  return;
#elif defined(FLOCKU_TFT_TRACKER_UI)
#if defined(FLOCKU_BOARD_HELTEC_TRACKER)
  heltec_show_alert(v, a);
  return;
#else
  static uint32_t s_last_alert_paint;
  if (tft_full_paint_skip(s_last_alert_paint)) return;
#endif
#endif
  disp_clear();
#if defined(FLOCKU_BOARD_TBEAM)
  draw_tbeam_top_bar(millis(), v);
  const int16_t y = kTbeamBarH + 2;
#elif defined(FLOCKU_TFT_TRACKER_UI)
  tracker_draw_dashboard_bar(millis(), v);
  const int16_t y = kTrackerDashH + 4;
#else
  draw_satellite_glyph();
  const int16_t y = kSatelliteDisplayH + 2;
#endif
  display.setTextColor(MONO_HI);
  display.setTextSize(1);
#if !defined(FLOCKU_BOARD_TBEAM)
  if (v.sim_mode) {
#if !defined(FLOCKU_TFT_TRACKER_UI)
    display.setCursor(kSatelliteDisplayW + 2, 0);
    display.println(F("SIM"));
#endif
  }
#endif
#if defined(FLOCKU_TFT_TRACKER_UI)
  {
    int16_t yy = y;
    yy = tracker_draw_alert_title(yy, a.title, ST77XX_RED);
    if (a.line2[0]) {
      yy = tracker_draw_body_wrapped(yy, a.line2, ST77XX_WHITE, 1);
    }
    if (a.line3[0]) {
      yy = tracker_draw_body_wrapped(yy, a.line3, ST77XX_WHITE, 1);
    }
    if (a.arrow != AlertArrow::None) {
      const int16_t arrow_cy = (y + std::min<int16_t>(yy, kTrackerBodyBottom)) / 2;
      tracker_draw_arrow_glyph(kScrW - 14, arrow_cy, a.arrow, ST77XX_RED);
    }
    tracker_use_classic_font();
    tracker_draw_version_footer();
  }
#else
  display.setCursor(0, y);
  display.setTextSize(2);
  display.println(a.title);
  if (a.line2[0]) display.println(a.line2);
  if (a.line3[0]) display.println(a.line3);
  display.setTextSize(1);
  if (a.brand[0] || v.demo_caption[0]) {
    display.setCursor(0, kScrH - 8);
    if (a.brand[0]) display.print(a.brand);
    if (a.brand[0] && v.demo_caption[0]) display.print(F("  "));
    if (v.demo_caption[0]) display.print(v.demo_caption);
  }
#endif
  disp_show();
#if defined(FLOCKU_TFT_TRACKER_UI)
  tracker_idle_mark_content_stale();
#endif
}
