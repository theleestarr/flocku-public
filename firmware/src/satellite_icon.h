#pragma once

#include <Arduino.h>

// 8×8 satellite XBM from Meshtastic `src/graphics/images.h` (`imgSatellite`); LSB-first per row (same as Adafruit `drawXBitmap`).
// Meshtastic firmware is GPL-3.0; this 8×8 excerpt is copied for interoperability (same glyph as MT UI).
// Source: https://github.com/meshtastic/firmware/blob/master/src/graphics/images.h
static constexpr int kSatelliteIconW = 8;
static constexpr int kSatelliteIconH = 8;
// Integer scale for OLED (source bitmap is 8×8 XBM / LSB-first rows).
static constexpr int kSatelliteDisplayScale = 2;
static constexpr int kSatelliteDisplayW = kSatelliteIconW * kSatelliteDisplayScale;
static constexpr int kSatelliteDisplayH = kSatelliteIconH * kSatelliteDisplayScale;

static const uint8_t kMeshtasticSatellite8[] PROGMEM = {
    0x00, 0x00, 0x00, 0x18, 0xDB, 0xFF, 0xDB, 0x18,
};
