#pragma once

#include <Arduino.h>

/// Call once after `Wire.begin(OLED_SDA, OLED_SCL)` (e.g. from `DisplayManager::begin`).
bool battery_monitor_begin();

/// Percent from AXP192 via XPowersLib (`getBatteryPercent`): voltage→lookup table, not a fuel gauge — reads high while charging.
/// Returns 0–100, or -1 if unknown / no battery detected.
int8_t battery_percent_read();

/// True when AXP192 reports active charging (battery path). Use with USB present to verify charge circuit.
bool battery_is_charging();

/// True when VBUS (USB) is present.
bool battery_usb_present();

/// Battery terminal voltage in mV from PMU ADC, or 0 if unavailable.
uint16_t battery_voltage_mv();
