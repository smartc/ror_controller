/*
 * ESP32 ASCOM Alpaca Roll-Off Roof Controller
 * Debug Utility Implementation
 *
 * Ring buffer: 200 entries x 120 chars = ~26 KB static RAM.
 * Consecutive identical lines collapse into a single entry with a repeat count.
 * logSeq increments on every change so the browser can poll cheaply.
 *
 * Thread safety (v4): Debug is called from the roof task, the loop task and
 * the logger task. The ring is guarded by a mutex that is only ever held for a
 * memcpy-sized moment, and each task has its own partial-line buffer so
 * print()+println() pairs from different tasks can't interleave. Persisting a
 * line is a non-blocking queue send; the logger task does the FRAM write.
 */

#include "Debug.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

DebugClass Debug;

// -- Ring buffer ----------------------------------------------------------------

#define LOG_ENTRIES  200
#define LOG_MSG_LEN  LOG_LINE_MAX

struct LogEntry {
  uint32_t ms;
  uint32_t utc;      // 0 = no time source when logged
  uint16_t count;
  char     msg[LOG_MSG_LEN];
};

static LogTimeSource  timeSource  = nullptr;

static LogEntry  logRing[LOG_ENTRIES];
static int       logHead  = 0;
static int       logCount = 0;
static uint32_t  logSeq   = 0;

static SemaphoreHandle_t ringMutex = nullptr;
static QueueHandle_t     persistQueue = nullptr;
static volatile uint32_t persistDropped = 0;

// Per-task line accumulators - text is buffered until '\n' triggers a push
#define LOG_ACCUMS 4
struct LineAccum {
  TaskHandle_t task;
  size_t len;
  char buf[LOG_MSG_LEN];
};
static LineAccum accums[LOG_ACCUMS];

static void lockRing()   { if (ringMutex) xSemaphoreTake(ringMutex, portMAX_DELAY); }
static void unlockRing() { if (ringMutex) xSemaphoreGive(ringMutex); }

void logInit()
{
  if (!ringMutex) ringMutex = xSemaphoreCreateMutex();
}

// Caller holds the ring lock
static void pushLine(const char* line)
{
  uint32_t now = millis();
  uint32_t utc = timeSource ? timeSource() : 0;

  // Dedup: if last entry is identical, just bump its count
  if (logCount > 0) {
    int last = (logHead + logCount - 1) % LOG_ENTRIES;
    if (strncmp(logRing[last].msg, line, LOG_MSG_LEN) == 0) {
      logRing[last].count++;
      logRing[last].ms = now;
      logSeq++;
      return;
    }
  }

  // New entry - overwrite oldest slot when full
  int idx;
  if (logCount < LOG_ENTRIES) {
    idx = (logHead + logCount) % LOG_ENTRIES;
    logCount++;
  } else {
    idx = logHead;
    logHead = (logHead + 1) % LOG_ENTRIES;
  }
  strncpy(logRing[idx].msg, line, LOG_MSG_LEN - 1);
  logRing[idx].msg[LOG_MSG_LEN - 1] = '\0';
  logRing[idx].ms    = now;
  logRing[idx].utc   = utc;
  logRing[idx].count = 1;
  logSeq++;

  // Repeats (deduped above) are not re-persisted; the first occurrence is.
  // Never blocks: if the logger is behind, the line stays RAM-only.
  if (persistQueue) {
    LogLine item;
    item.utc = utc;
    item.ms = now;
    memcpy(item.msg, logRing[idx].msg, LOG_MSG_LEN);
    if (xQueueSend(persistQueue, &item, 0) != pdTRUE) persistDropped++;
  }
}

void logEnablePersistQueue(size_t depth)
{
  lockRing();
  if (!persistQueue) {
    persistQueue = xQueueCreate(depth, sizeof(LogLine));
    // Replay what's already in RAM so early boot lines are persisted too
    for (int i = 0; i < logCount && persistQueue; i++) {
      const LogEntry& e = logRing[(logHead + i) % LOG_ENTRIES];
      LogLine item;
      item.utc = e.utc;
      item.ms = e.ms;
      memcpy(item.msg, e.msg, LOG_MSG_LEN);
      if (xQueueSend(persistQueue, &item, 0) != pdTRUE) persistDropped++;
    }
  }
  unlockRing();
}

bool logPersistReceive(LogLine& out, uint32_t waitMs)
{
  if (!persistQueue) return false;
  return xQueueReceive(persistQueue, &out, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}

uint32_t logPersistPending()
{
  return persistQueue ? uxQueueMessagesWaiting(persistQueue) : 0;
}

uint32_t logPersistDropped()
{
  return persistDropped;
}

void logSetTimeSource(LogTimeSource fn)
{
  timeSource = fn;
}

static LineAccum& accumForCurrentTask()
{
  TaskHandle_t me = xTaskGetCurrentTaskHandle();
  for (auto& a : accums) if (a.task == me) return a;
  for (auto& a : accums) if (a.task == nullptr) { a.task = me; a.len = 0; return a; }
  return accums[LOG_ACCUMS - 1];  // more tasks than slots: share the last one
}

void logAppend(const char* text)
{
  lockRing();
  LineAccum& acc = accumForCurrentTask();
  for (const char* p = text; *p; p++) {
    if (*p == '\n') {
      if (acc.len > 0) {
        acc.buf[acc.len] = '\0';
        pushLine(acc.buf);
        acc.len = 0;
      }
    } else if (*p != '\r' && acc.len < LOG_MSG_LEN - 1) {
      acc.buf[acc.len++] = *p;
    }
  }
  unlockRing();
}

// -- JSON serialiser ------------------------------------------------------------

static void jsonEscape(String& out, const char* s)
{
  for (; *s; s++) {
    if      (*s == '"')  { out += "\\\""; }
    else if (*s == '\\') { out += "\\\\"; }
    else if ((uint8_t)*s < 0x20) { /* skip control chars */ }
    else                 { out += *s; }
  }
}

// Copies one entry at a time under the lock, so building the JSON (slow) never
// holds the ring while another task wants to log
void logGetJSON(String& out)
{
  lockRing();
  uint32_t seq = logSeq;
  int count = logCount;
  unlockRing();

  out = "{\"seq\":";
  out += seq;
  out += ",\"entries\":[";
  LogEntry e;
  for (int i = 0; i < count; i++) {
    lockRing();
    if (i >= logCount) { unlockRing(); break; }
    e = logRing[(logHead + i) % LOG_ENTRIES];
    unlockRing();
    if (i > 0) out += ',';
    out += "{\"ms\":";
    out += e.ms;
    out += ",\"t\":";
    out += e.utc;
    out += ",\"n\":";
    out += e.count;
    out += ",\"msg\":\"";
    jsonEscape(out, e.msg);
    out += "\"}";
  }
  out += "]}";
}
