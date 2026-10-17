/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v4)
 * FRAM black-box log (MB85RC256V, 32 KB, I2C 0x50)
 *
 * Every persistent log line is written to FRAM the moment it's logged, so the
 * lines leading up to a brownout or crash survive the reset. FRAM has no erase
 * cycle and ~10^12 write endurance, so per-line writes cost nothing.
 *
 * Layout: two 48-byte header slots (written alternately, each with a CRC, so a
 * write torn by power loss leaves the other slot valid) followed by a byte ring
 * of records:
 *   [0xA5][len u8][utc u32][boot u16][uptime_ms u32][text, len bytes]
 * A record only "exists" once the header that covers it is committed.
 * log_archive.cpp copies unflushed records to LittleFS periodically.
 */

#ifndef FRAM_LOG_H
#define FRAM_LOG_H

#include <Arduino.h>

#define FRAM_I2C_ADDRESS      0x50
#define FRAM_LOG_TEXT_MAX     120

struct FramLogRecord {
  uint32_t utc;        // 0 = no time source when logged
  uint16_t boot;       // reboot counter when logged
  uint32_t ms;         // millis() when logged
  uint8_t  len;
  char     text[FRAM_LOG_TEXT_MAX + 1];
};

struct FramLogIter {
  uint32_t pos;        // ring offset of the next record
  uint32_t remaining;  // bytes left to read
  uint32_t consumed;   // bytes read so far
};

bool framLogBegin(uint16_t bootId);       // Wire must already be started
bool framLogPresent();

// Iteration (oldest first). unflushedOnly = only records not yet archived.
void framLogIterBegin(FramLogIter& it, bool unflushedOnly);
bool framLogIterNext(FramLogIter& it, FramLogRecord& rec);

void framLogMarkFlushed(const FramLogIter& it);   // everything before it.pos is archived
void framLogClear();

uint32_t framLogCapacity();
uint32_t framLogUsed();
uint32_t framLogUnflushed();
uint32_t framLogDropped();                // unflushed records overwritten before archiving
uint32_t framLogSize();                   // chip size in bytes (0 = not present)

#endif // FRAM_LOG_H
