/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v4)
 * Shared I2C bus implementation
 */

#include "i2c_bus.h"
#include <Wire.h>

static SemaphoreHandle_t busMutex = nullptr;

void i2cBusBegin(int sda, int scl, uint32_t hz) {
  if (!busMutex) busMutex = xSemaphoreCreateRecursiveMutex();
  Wire.begin(sda, scl);
  Wire.setClock(hz);
  // A missing or wedged device should cost milliseconds, not the 50 ms default
  Wire.setTimeOut(20);
}

SemaphoreHandle_t i2cBusMutex() { return busMutex; }
