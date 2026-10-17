/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v4)
 * Log archive implementation
 */

#include "log_archive.h"
#include "../debug/Debug.h"
#include "../roof/roof_controller.h"
#include "../sensors/gps_handler.h"
#include <LittleFS.h>
#include <esp_partition.h>
#include <vector>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Serialises flush / clear / delete between the logger task and web requests
static SemaphoreHandle_t archiveMutex = nullptr;   // created in logArchiveBegin()
struct ArchiveLock {
  ArchiveLock()  { if (archiveMutex) xSemaphoreTakeRecursive(archiveMutex, portMAX_DELAY); }
  ~ArchiveLock() { if (archiveMutex) xSemaphoreGiveRecursive(archiveMutex); }
};

static bool ready = false;
static bool flushRequested = false;
static unsigned long lastFlushMs = 0;
static uint32_t lastFlushUtc = 0;
static uint32_t lastFlushRecords = 0;
static uint16_t currentBoot = 0;

// Records per loop pass, so a large backlog never stalls the loop for long
static const int FLUSH_BATCH = 100;

bool logArchiveBegin() {
  if (!archiveMutex) archiveMutex = xSemaphoreCreateRecursiveMutex();
  if (!esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs")) {
    Debug.println("Log archive disabled: no 'spiffs' partition (wrong flash layout)");
    return false;
  }
  // formatOnFail: the first boot after the v4 migration finds an unformatted partition
  if (!LittleFS.begin(true, "/littlefs", 5, "spiffs")) {
    Debug.println("Log archive disabled: LittleFS mount failed");
    return false;
  }
  if (!LittleFS.exists(LOG_ARCHIVE_DIR)) {
    LittleFS.mkdir(LOG_ARCHIVE_DIR);
  }
  ready = true;
  lastFlushMs = millis();
  Debug.printf("Log archive mounted: %lu KB used of %lu KB\n",
               (unsigned long)(LittleFS.usedBytes() / 1024), (unsigned long)(LittleFS.totalBytes() / 1024));
  return true;
}

bool logArchiveReady() { return ready; }
fs::FS& logArchiveFS() { return LittleFS; }

bool logArchiveBusy() {
  return roofStatus == ROOF_OPENING || roofStatus == ROOF_CLOSING || roofOpState != OP_IDLE;
}

void logArchiveRequestFlush() { flushRequested = true; }

// ── Formatting ───────────────────────────────────────────────────────────────

static void formatUtc(uint32_t utc, char* buf, size_t len, char suffix) {
  time_t t = utc;
  struct tm tmUtc;
  gmtime_r(&t, &tmUtc);
  snprintf(buf, len, "%04d-%02d-%02d %02d:%02d:%02d%c",
           tmUtc.tm_year + 1900, tmUtc.tm_mon + 1, tmUtc.tm_mday,
           tmUtc.tm_hour, tmUtc.tm_min, tmUtc.tm_sec, suffix);
}

// Records logged before this boot got a time source can be dated from uptime
// once it has one. Returns 0 if the record can't be dated.
static uint32_t effectiveUtc(const FramLogRecord& rec, bool& estimated) {
  estimated = false;
  if (rec.utc != 0) return rec.utc;
  uint32_t now = isTimeSynced() ? getCurrentUnixTime() : 0;
  if (now == 0 || rec.boot != currentBoot) return 0;
  uint32_t ageS = (millis() - rec.ms) / 1000;
  estimated = true;
  return now > ageS ? now - ageS : 0;
}

String logFormatRecord(const FramLogRecord& rec) {
  bool estimated;
  uint32_t utc = effectiveUtc(rec, estimated);
  char ts[24];
  if (utc) formatUtc(utc, ts, sizeof(ts), estimated ? '~' : 'Z');
  else     strcpy(ts, "---------- --:--:-- ");
  uint32_t s = rec.ms / 1000;
  char prefix[64];
  snprintf(prefix, sizeof(prefix), "%s [b%u +%02lu:%02lu:%02lu.%03lu] ", ts, rec.boot,
           (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60), (unsigned long)(s % 60),
           (unsigned long)(rec.ms % 1000));
  return String(prefix) + rec.text;
}

static String fileForRecord(const FramLogRecord& rec) {
  bool estimated;
  uint32_t utc = effectiveUtc(rec, estimated);
  char name[32];
  if (utc) {
    time_t t = utc;
    struct tm tmUtc;
    gmtime_r(&t, &tmUtc);
    snprintf(name, sizeof(name), LOG_ARCHIVE_DIR "/%04d-%02d-%02d.log",
             tmUtc.tm_year + 1900, tmUtc.tm_mon + 1, tmUtc.tm_mday);
  } else {
    snprintf(name, sizeof(name), LOG_ARCHIVE_DIR "/boot-%05u.log", rec.boot);
  }
  return String(name);
}

// ── Retention ────────────────────────────────────────────────────────────────

struct LogFileInfo { String name; size_t size; };

static std::vector<LogFileInfo> listLogFiles() {
  std::vector<LogFileInfo> files;
  File dir = LittleFS.open(LOG_ARCHIVE_DIR);
  if (!dir || !dir.isDirectory()) return files;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    if (!f.isDirectory()) files.push_back({String(f.name()), f.size()});
  }
  return files;
}

// Undated boot-*.log files sort before dated files, so they're the first to
// go when over the size cap; dated files sort chronologically by name.
static bool olderFirst(const LogFileInfo& a, const LogFileInfo& b) {
  bool aBoot = a.name.startsWith("boot-"), bBoot = b.name.startsWith("boot-");
  if (aBoot != bBoot) return aBoot;
  return a.name < b.name;
}

static void applyRetention() {
  std::vector<LogFileInfo> files = listLogFiles();
  std::sort(files.begin(), files.end(), olderFirst);

  // Age limit (dated files only, and only with a trusted clock)
  if (isTimeSynced()) {
    uint32_t cutoff = getCurrentUnixTime() - LOG_ARCHIVE_MAX_DAYS * 86400UL;
    char cutoffName[24];
    time_t t = cutoff;
    struct tm tmUtc;
    gmtime_r(&t, &tmUtc);
    snprintf(cutoffName, sizeof(cutoffName), "%04d-%02d-%02d.log",
             tmUtc.tm_year + 1900, tmUtc.tm_mon + 1, tmUtc.tm_mday);
    for (auto it = files.begin(); it != files.end();) {
      if (!it->name.startsWith("boot-") && it->name < String(cutoffName)) {
        LittleFS.remove(String(LOG_ARCHIVE_DIR "/") + it->name);
        it = files.erase(it);
      } else {
        ++it;
      }
    }
  }

  // Size limit
  size_t total = 0;
  for (auto& f : files) total += f.size;
  while (total > LOG_ARCHIVE_MAX_BYTES && files.size() > 1) {
    total -= files.front().size;
    LittleFS.remove(String(LOG_ARCHIVE_DIR "/") + files.front().name);
    files.erase(files.begin());
  }
}

// ── Flushing ─────────────────────────────────────────────────────────────────

// Copies up to maxRecords unflushed FRAM records into the archive.
// Returns the number copied; nothing may be logged while this runs.
static int flushBatch(int maxRecords) {
  FramLogIter it;
  framLogIterBegin(it, true);
  FramLogRecord rec;
  File out;
  String openName;
  int count = 0;

  while (count < maxRecords && framLogIterNext(it, rec)) {
    String name = fileForRecord(rec);
    if (name != openName) {
      if (out) out.close();
      out = LittleFS.open(name, FILE_APPEND);
      openName = name;
      if (!out) break;
    }
    out.print(logFormatRecord(rec));
    out.print('\n');
    count++;
  }
  if (out) out.close();
  framLogMarkFlushed(it);
  return count;
}

// Writes any queued log lines to FRAM right now (they normally go via the logger task)
static void drainPersistQueue() {
  LogLine line;
  while (logPersistReceive(line, 0)) framLogWriteLine(line.utc, line.ms, line.msg);
}

bool logArchiveFlushNow() {
  ArchiveLock lock;
  drainPersistQueue();
  if (!ready || !framLogPresent()) return false;
  if (logArchiveBusy()) return false;
  uint32_t total = 0;
  int n;
  while ((n = flushBatch(FLUSH_BATCH)) > 0) total += n;
  applyRetention();
  lastFlushMs = millis();
  lastFlushUtc = isTimeSynced() ? getCurrentUnixTime() : 0;
  lastFlushRecords = total;
  flushRequested = false;
  if (total > 0) Debug.printf(2, "Log archive: flushed %lu records\n", total);
  return true;
}

void logArchiveHandle() {
  static bool draining = false;
  if (!ready || !framLogPresent()) return;
  ArchiveLock lock;

  if (!draining) {
    bool due = flushRequested ||
               millis() - lastFlushMs >= LOG_FLUSH_INTERVAL_MS ||
               framLogUnflushed() * 2 >= framLogCapacity();
    if (!due || framLogUnflushed() == 0) {
      if (due) { lastFlushMs = millis(); flushRequested = false; }
      return;
    }
    draining = true;
    lastFlushRecords = 0;
  }
  if (logArchiveBusy()) return;  // resume once the roof is idle

  int n = flushBatch(FLUSH_BATCH);
  lastFlushRecords += n;
  if (n < FLUSH_BATCH) {
    draining = false;
    applyRetention();
    lastFlushMs = millis();
    lastFlushUtc = isTimeSynced() ? getCurrentUnixTime() : 0;
    flushRequested = false;
  }
}

// ── Management ───────────────────────────────────────────────────────────────

bool logArchiveValidName(const String& name) {
  if (name.length() == 0 || name.length() > 24 || !name.endsWith(".log")) return false;
  for (size_t i = 0; i < name.length(); i++) {
    char c = name[i];
    if (!(isalnum(c) || c == '-' || c == '.')) return false;
  }
  return name.indexOf("..") < 0;
}

bool logArchiveDelete(const String& name) {
  if (!ready || !logArchiveValidName(name)) return false;
  bool ok;
  {
    ArchiveLock lock;
    ok = LittleFS.remove(String(LOG_ARCHIVE_DIR "/") + name);
  }
  if (ok) Debug.println("Log file deleted: " + name);
  return ok;
}

void logArchiveClearAll() {
  {
    ArchiveLock lock;
    drainPersistQueue();
    if (ready) {
      for (auto& f : listLogFiles()) LittleFS.remove(String(LOG_ARCHIVE_DIR "/") + f.name);
    }
    framLogClear();
  }
  Debug.println("All logs cleared (FRAM + archive)");
}

// -- Logger task ------------------------------------------------------------------
// Owns the slow side of logging: FRAM writes (I2C) and archive flushes (flash).
// Runs at the lowest priority so it can never delay the roof task.

static void loggerTask(void*) {
  LogLine line;
  for (;;) {
    if (logPersistReceive(line, 50)) {
      do {
        framLogWriteLine(line.utc, line.ms, line.msg);
      } while (logPersistReceive(line, 0));
    }
    framLogHandle();
    logArchiveHandle();
  }
}

void logArchiveStartTask() {
  static bool started = false;
  if (started) return;
  started = true;
  xTaskCreatePinnedToCore(loggerTask, "logger", 6144, nullptr, 1, nullptr, 0);
}

void logArchiveStatus(JsonObject obj) {
  obj["archive_ready"] = ready;
  obj["fram_present"] = framLogPresent();
  obj["fram_detected"] = framLogDetected();
  obj["queue_pending"] = logPersistPending();
  obj["queue_dropped"] = logPersistDropped();
  obj["fram_capacity"] = framLogCapacity();
  obj["fram_used"] = framLogUsed();
  obj["fram_unflushed"] = framLogUnflushed();
  obj["fram_dropped"] = framLogDropped();
  size_t total = 0, count = 0;
  if (ready) {
    for (auto& f : listLogFiles()) { total += f.size; count++; }
  }
  obj["archive_bytes"] = total;
  obj["archive_files"] = count;
  obj["archive_cap"] = LOG_ARCHIVE_MAX_BYTES;
  obj["archive_days"] = LOG_ARCHIVE_MAX_DAYS;
  obj["fs_used"] = ready ? (uint32_t)LittleFS.usedBytes() : 0;
  obj["fs_total"] = ready ? (uint32_t)LittleFS.totalBytes() : 0;
  obj["last_flush_utc"] = lastFlushUtc;
  obj["last_flush_age_s"] = (millis() - lastFlushMs) / 1000;
  obj["last_flush_records"] = lastFlushRecords;
  obj["busy"] = logArchiveBusy();
}

void logArchiveListFiles(JsonArray arr) {
  if (!ready) return;
  std::vector<LogFileInfo> files = listLogFiles();
  std::sort(files.begin(), files.end(), olderFirst);
  for (auto it = files.rbegin(); it != files.rend(); ++it) {
    JsonObject f = arr.createNestedObject();
    f["name"] = it->name;
    f["size"] = it->size;
  }
}

// Called from main.ino once the boot counter is known
void logArchiveSetBoot(uint16_t boot) { currentBoot = boot; }
