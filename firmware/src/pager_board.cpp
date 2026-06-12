#include "pager_board.h"

#if defined(FLOCKU_BOARD_LILYGO_PAGER)

#include "config.h"

#include <Wire.h>

#include "ExtensionIOXL9555.hpp"

static ExtensionIOXL9555 g_io;
static bool g_io_ok{false};

bool pager_board_begin() {
  pinMode(PAGER_POWER_ON_PIN, OUTPUT);
  digitalWrite(PAGER_POWER_ON_PIN, HIGH);
  delay(20);

  pinMode(PAGER_TFT_CS_PIN, OUTPUT);
  digitalWrite(PAGER_TFT_CS_PIN, HIGH);
  pinMode(PAGER_SDCARD_CS_PIN, OUTPUT);
  digitalWrite(PAGER_SDCARD_CS_PIN, HIGH);
  pinMode(PAGER_LORA_CS_PIN, OUTPUT);
  digitalWrite(PAGER_LORA_CS_PIN, HIGH);

  Wire.begin(PAGER_I2C_SDA, PAGER_I2C_SCL);
  delay(10);

  g_io_ok = g_io.begin(Wire, PAGER_IO_EXPANDER_ADDR, PAGER_I2C_SDA, PAGER_I2C_SCL);
  if (!g_io_ok) return false;

  const uint8_t outputs[] = {PAGER_EXPANDS_DRV_EN, PAGER_EXPANDS_LORA_EN, PAGER_EXPANDS_GPS_EN,
                             PAGER_EXPANDS_KB_EN,   PAGER_EXPANDS_SD_EN,   PAGER_EXPANDS_GPIO_EN};
  for (uint8_t pin : outputs) {
    g_io.pinMode(pin, OUTPUT);
    g_io.digitalWrite(pin, HIGH);
    delay(1);
  }

  g_io.pinMode(PAGER_EXPANDS_AMP_EN, OUTPUT);
  g_io.digitalWrite(PAGER_EXPANDS_AMP_EN, LOW);

  g_io.pinMode(PAGER_EXPANDS_SD_PULLEN, INPUT);
  delay(50);
  return true;
}

#else

bool pager_board_begin() { return true; }

#endif
