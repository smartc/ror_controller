/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v4)
 * Log archive - copies the FRAM log to LittleFS (the 9.9 MB "spiffs" partition)
 *
 * Files: /logs/YYYY-MM-DD.log (UTC day). Lines from a boot that never got a time
 * source go to /logs/boot-NNNNN.log. Flushes run every 15 minutes, when half
 * the FRAM ring is unflushed, or on request - but never while the roof is
 * moving, because a flash erase can stall the main loop for tens of ms.
 * Retention: 90 days and 2 MB total, oldest deleted first.
 */

#ifndef LOG_ARCHIVE_H
#define LOG_ARCHIVE_H

#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>
#include "fram_log.h"

#define LOG_ARCHIVE_DIR            "/logs"
#define LOG_ARCHIVE_MAX_BYTES      (2UL * 1024 * 1024)
#define LOG_ARCHIVE_MAX_DAYS       90
#define LOG_FLUSH_INTERVAL_MS      (15UL * 60 * 1000)

bool logArchiveBegin();                   // Mount LittleFS (formats on first use)
void logArchiveSetBoot(uint16_t boot);    // Current reboot counter (for dating early-boot lines)
bool logArchiveReady();
void logArchiveHandle();                  // Call every loop()
void logArchiveRequestFlush();            // Flush at the next safe moment
bool logArchiveFlushNow();                // Flush everything now; false if the roof is busy
bool logArchiveBusy();                    // Roof moving/relay sequence active: no flash I/O

fs::FS& logArchiveFS();
bool logArchiveValidName(const String& name);  // Plain "*.log" name inside /logs
bool logArchiveDelete(const String& name);
void logArchiveClearAll();                // FRAM + all archive files

void logArchiveStatus(JsonObject obj);    // Usage, last flush, FRAM stats
void logArchiveListFiles(JsonArray arr);  // [{name,size}] newest first

// One formatted line for a FRAM record (no trailing newline)
String logFormatRecord(const FramLogRecord& rec);

#endif // LOG_ARCHIVE_H
