/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v4)
 * Shared I2C bus (DS3231 RTC + FRAM log)
 *
 * The bus is used from more than one task (logger task writes FRAM, the loop
 * task reads the RTC and serves FRAM history), so every transaction sequence
 * holds this recursive lock. The roof task never touches I2C.
 */

#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

void i2cBusBegin(int sda, int scl, uint32_t hz);   // Start Wire and create the lock
SemaphoreHandle_t i2cBusMutex();

// RAII guard: holds the bus for the enclosing scope
class I2CLock {
public:
  I2CLock()  { if (i2cBusMutex()) xSemaphoreTakeRecursive(i2cBusMutex(), portMAX_DELAY); }
  ~I2CLock() { if (i2cBusMutex()) xSemaphoreGiveRecursive(i2cBusMutex()); }
  I2CLock(const I2CLock&) = delete;
  I2CLock& operator=(const I2CLock&) = delete;
};

#endif // I2C_BUS_H
