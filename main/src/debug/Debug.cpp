/*
 * ESP32 ASCOM Alpaca Roll-Off Roof Controller
 * Debug Utility Implementation
 *
 * Ring buffer: 50 entries × 120 chars ≈ 6 KB static RAM.
 * Consecutive identical lines collapse into a single entry with a repeat count.
 * logSeq increments on every change so the browser can poll cheaply.
 */

#include "Debug.h"

DebugClass Debug;

// ── Ring buffer ───────────────────────────────────────────────────────────────

#define LOG_ENTRIES  50
#define LOG_MSG_LEN  120

struct LogEntry {
  uint32_t ms;
  uint16_t count;
  char     msg[LOG_MSG_LEN];
};

static LogEntry  logRing[LOG_ENTRIES];
static int       logHead  = 0;
static int       logCount = 0;
static uint32_t  logSeq   = 0;

// Line accumulator — text is buffered until '\n' triggers a push
static char   lineAccum[LOG_MSG_LEN];
static size_t lineLen = 0;

static void pushLine(const char* line)
{
  uint32_t now = millis();

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

  // New entry — overwrite oldest slot when full
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
  logRing[idx].count = 1;
  logSeq++;
}

void logAppend(const char* text)
{
  for (const char* p = text; *p; p++) {
    if (*p == '\n') {
      if (lineLen > 0) {
        lineAccum[lineLen] = '\0';
        pushLine(lineAccum);
        lineLen = 0;
      }
    } else if (*p != '\r' && lineLen < LOG_MSG_LEN - 1) {
      lineAccum[lineLen++] = *p;
    }
  }
}

// ── JSON serialiser ───────────────────────────────────────────────────────────

static void jsonEscape(String& out, const char* s)
{
  for (; *s; s++) {
    if      (*s == '"')  { out += "\\\""; }
    else if (*s == '\\') { out += "\\\\"; }
    else if ((uint8_t)*s < 0x20) { /* skip control chars */ }
    else                 { out += *s; }
  }
}

void logGetJSON(String& out)
{
  out = "{\"seq\":";
  out += logSeq;
  out += ",\"entries\":[";
  for (int i = 0; i < logCount; i++) {
    int idx = (logHead + i) % LOG_ENTRIES;
    if (i > 0) out += ',';
    out += "{\"ms\":";
    out += logRing[idx].ms;
    out += ",\"n\":";
    out += logRing[idx].count;
    out += ",\"msg\":\"";
    jsonEscape(out, logRing[idx].msg);
    out += "\"}";
  }
  out += "]}";
}
