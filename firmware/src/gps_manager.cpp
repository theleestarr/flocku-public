#include "gps_manager.h"

#include <TinyGPSPlus.h>

#include <cmath>

#include "config.h"
#include "geo_math.h"
#if defined(FLOCKU_BOARD_LILYGO_PAGER)
#include "pager_board.h"
#endif

static TinyGPSPlus gps;

#if defined(FLOCKU_BOARD_TBEAM) || defined(FLOCKU_BOARD_ESP32_WROVER_OLED) || defined(FLOCKU_BOARD_LILYGO_PAGER)
#if GPS_UBLOX_USE_PMTK
// Trim NMEA to RMC+GGA, optional 5 Hz, optional 38400 baud (checksums verified).
static void ublox_pmtk_apply(HardwareSerial& ser) {
  ser.print(F("$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28\r\n"));
  delay(80);
#if GPS_UBLOX_USE_5HZ
  ser.print(F("$PMTK220,200*2C\r\n"));
  delay(80);
#endif
#if GPS_UBLOX_UPGRADE_BAUD
  ser.print(F("$PMTK251,38400*27\r\n"));
  delay(250);  // Allow module to switch baud before we close/reopen UART.
  ser.flush();
  while (ser.available() > 0) {
    (void)ser.read();
  }
  ser.end();
  delay(80);
  ser.begin(GPS_UBLOX_RUNTIME_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
#ifdef ARDUINO_ARCH_ESP32
  ser.setRxBufferSize(2048);
#endif
#endif
  delay(50);
  while (ser.available() > 0) {
    (void)ser.read();
  }
}
#endif
#endif

// Same hardware serial the rest of the file uses (matches GPS_UART_NUM).
static HardwareSerial& gps_port() {
#if GPS_UART_NUM == 1
  return Serial1;
#else
  return Serial2;
#endif
}

// Quectel L76K: Meshtastic `src/gps/GPS.cpp` GNSS_MODEL_MTK init (PCAS, not PMTK).
// https://github.com/meshtastic/firmware/blob/develop/src/gps/GPS.cpp
// Unicore UC6580 (Heltec Wireless Tracker) — Meshtastic GNSS_MODEL_UC6580 setup.
// Without CFGSYS many modules never track satellites (NMEA may still flow).
static void uc6580_init_meshtastic_style(HardwareSerial& ser) {
  ser.print(F("$CFGSYS,h35155\r\n"));  // GPS L1/L5 + BDS + GLONASS + GALILEO + SBAS + QZSS
  delay(750);                          // receiver resets after CFGSYS
  ser.print(F("$CFGMSG,0,3,0\r\n"));   // GSV off
  delay(250);
  ser.print(F("$CFGMSG,0,2,0\r\n"));   // GSA off
  delay(250);
  ser.print(F("$CFGMSG,6,0,0\r\n"));  // NOTICE TXT off
  delay(250);
  ser.print(F("$CFGMSG,6,1,0\r\n"));
  delay(250);
  while (ser.available() > 0) {
    (void)ser.read();
  }
}

static void l76k_init_meshtastic_style(HardwareSerial& ser) {
  const char* const cmds[] = {
      // Multi-constellation
      "$PCAS04,7*1E\r\n",
      // RMC + GGA only (TinyGPS++ needs these for fix / speed / course)
      "$PCAS03,1,0,0,0,1,0,0,0,0,0,,,0,0*02\r\n",
      // Vehicle mode (vs aviation)
      "$PCAS11,3*1E\r\n",
  };
  for (const char* c : cmds) {
    ser.print(c);
    delay(250);
  }
}

void GpsManager::begin() {
#if defined(FLOCKU_BOARD_LILYGO_PAGER)
  (void)pager_board_begin();
#endif
#if defined(GPS_POWER_EN_PIN)
  pinMode(GPS_POWER_EN_PIN, OUTPUT);
#if defined(GPS_POWER_EN_ACTIVE_HIGH)
  digitalWrite(GPS_POWER_EN_PIN, HIGH);
#else
  digitalWrite(GPS_POWER_EN_PIN, LOW);  // GPS_EN active low (Heltec Tracker v1.0 GPIO 37, L76K)
#endif
  delay(100);
#endif
#if defined(GPS_STANDBY_PIN)
  pinMode(GPS_STANDBY_PIN, OUTPUT);
  digitalWrite(GPS_STANDBY_PIN, HIGH);  // HIGH = force wake (Meshtastic heltec_v4)
#endif

  delay(GPS_WARMUP_MS);

#if defined(GPS_RESET_PIN)
  pinMode(GPS_RESET_PIN, OUTPUT);
  // Idle: not in reset (GPS_RESET_MODE from variant is LOW = reset asserted)
  digitalWrite(GPS_RESET_PIN, HIGH);
  delay(10);
  digitalWrite(GPS_RESET_PIN, LOW);   // assert reset
  delay(20);
  digitalWrite(GPS_RESET_PIN, HIGH);  // release
  delay(100);
#endif

  HardwareSerial& ser = gps_port();
#ifdef ARDUINO_ARCH_ESP32
  ser.setRxBufferSize(2048);
#endif
#if defined(FLOCKU_BOARD_TBEAM) || defined(FLOCKU_BOARD_ESP32_WROVER_OLED) || defined(FLOCKU_BOARD_LILYGO_PAGER)
#if GPS_UBLOX_USE_PMTK
  ser.begin(GPS_UBLOX_INITIAL_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  delay(150);
  ublox_pmtk_apply(ser);
#else
  ser.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  delay(150);
#endif
#elif defined(FLOCKU_BOARD_HELTEC_TRACKER)
  ser.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  delay(150);
  uc6580_init_meshtastic_style(ser);
#else
  ser.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  delay(150);
  // Quectel L76K (Heltec) — MTK PCAS init after UART up.
  l76k_init_meshtastic_style(ser);
#endif

  heading_count_ = 0;
  heading_pos_ = 0;
  smoothed_heading_deg_ = NAN;
}

void GpsManager::ingest_char(int c) { gps.encode(static_cast<char>(c)); }

void GpsManager::recompute_heading_smooth() {
  const auto& s = settings();
  if (speed_mph_ <= static_cast<float>(s.heading_trust_min_mph) || !std::isfinite(course_deg_)) {
    return;
  }
  heading_ring_[heading_pos_ % kHeadingBuf] = static_cast<float>(gps.course.deg());
  heading_pos_++;
  if (heading_count_ < kHeadingBuf) heading_count_++;

  const size_t n = heading_count_;
  float buf[kHeadingBuf];
  for (size_t i = 0; i < n; ++i) {
    const size_t idx = (heading_pos_ - n + i) % kHeadingBuf;
    buf[i] = heading_ring_[idx];
  }
  smoothed_heading_deg_ = circularMeanDeg(buf, n);
}

bool GpsManager::satellites_valid() const {
  return gps.satellites.isValid() && gps.satellites.age() < 3000;
}

uint8_t GpsManager::satellites_used() const {
  if (!satellites_valid()) return 0;
  const uint32_t v = gps.satellites.value();
  return v > 99u ? 99u : static_cast<uint8_t>(v);
}

void GpsManager::update() {
  HardwareSerial& ser = gps_port();
  while (ser.available() > 0) {
    ingest_char(ser.read());
  }

  const bool loc_ok = gps.location.isValid() && gps.location.age() < 2000;
  bool quality_ok = true;
  if (GPS_FIX_MAX_HDOP > 0.0f) {
    if (!gps.hdop.isValid() || gps.hdop.age() > 2000 ||
        static_cast<float>(gps.hdop.hdop()) > GPS_FIX_MAX_HDOP) {
      quality_ok = false;
    }
  }
  if (GPS_FIX_MIN_SATELLITES > 0u) {
    if (!gps.satellites.isValid() || gps.satellites.age() > 2000 ||
        gps.satellites.value() < GPS_FIX_MIN_SATELLITES) {
      quality_ok = false;
    }
  }
  has_fix_ = loc_ok && quality_ok;
  if (gps.location.isValid()) {
    lat_ = gps.location.lat();
    lon_ = gps.location.lng();
    fix_age_ms_ = gps.location.age();
  }
  if (gps.speed.isValid()) {
    speed_mph_ = static_cast<float>(gps.speed.mph());
  } else {
    speed_mph_ = 0;
  }
  if (gps.course.isValid() && speed_mph_ > 0.5f) {
    course_deg_ = static_cast<float>(gps.course.deg());
  }
  recompute_heading_smooth();
}
