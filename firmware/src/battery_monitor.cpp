#include "battery_monitor.h"

#include "config.h"
#include <Wire.h>

#if defined(FLOCKU_BOARD_LILYGO_PAGER)
#include "BQ27220.h"

static BQ27220 g_bq;
static bool g_bq_ok{false};

bool battery_monitor_begin() {
  Wire.begin(PAGER_I2C_SDA, PAGER_I2C_SCL);
  if (!g_bq.begin()) {
    g_bq_ok = false;
    return false;
  }
  if (!g_bq.init()) {
    g_bq_ok = false;
    return false;
  }
  (void)g_bq.setDefaultCapacity(BQ27220_DESIGN_CAPACITY);
  g_bq_ok = true;
  return true;
}

int8_t battery_percent_read() {
  if (!g_bq_ok) return -1;
  const int p = static_cast<int>(g_bq.getChargePercent());
  if (p < 0 || p == 65535) return -1;
  return static_cast<int8_t>(p > 100 ? 100 : p);
}

bool battery_is_charging() {
  if (!g_bq_ok) return false;
  return g_bq.getIsCharging();
}

bool battery_usb_present() {
  if (!g_bq_ok) return false;
  BQ27220BatteryStatus st{};
  if (!g_bq.getBatteryStatus(&st)) return false;
  return st.reg.BATTPRES != 0;
}

uint16_t battery_voltage_mv() {
  if (!g_bq_ok) return 0;
  return static_cast<uint16_t>(g_bq.getVoltage());
}

#elif defined(FLOCKU_BOARD_TBEAM)
#define XPOWERS_CHIP_AXP192
#include "XPowersLib.h"

static XPowersPMU g_pmu;
static bool g_pmu_ok;

bool battery_monitor_begin() {
  const bool ok =
      g_pmu.begin(Wire, AXP192_SLAVE_ADDRESS, OLED_SDA_PIN, OLED_SCL_PIN);
  if (!ok) {
    g_pmu_ok = false;
    return false;
  }
  // LilyGO / Meshtastic: boards without a TS thermistor must disable TS pin measure.
  g_pmu.disableTSPinMeasure();
  g_pmu.enableBattDetection();
  g_pmu.enableBattVoltageMeasure();
  g_pmu.enableVbusVoltageMeasure();
  g_pmu_ok = true;
  return true;
}

int8_t battery_percent_read() {
  if (!g_pmu_ok) return -1;
  if (!g_pmu.isBatteryConnect()) return -1;
  const int p = g_pmu.getBatteryPercent();
  if (p < 0) return -1;
  if (p > 100) return 100;
  return static_cast<int8_t>(p);
}

bool battery_is_charging() {
  if (!g_pmu_ok) return false;
  return g_pmu.isCharging();
}

bool battery_usb_present() {
  if (!g_pmu_ok) return false;
  return g_pmu.isVbusIn();
}

uint16_t battery_voltage_mv() {
  if (!g_pmu_ok || !g_pmu.isBatteryConnect()) return 0;
  return g_pmu.getBattVoltage();
}

#else

bool battery_monitor_begin() { return false; }

int8_t battery_percent_read() { return -1; }

bool battery_is_charging() { return false; }

bool battery_usb_present() { return false; }

uint16_t battery_voltage_mv() { return 0; }

#endif
