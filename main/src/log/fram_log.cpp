/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v4)
 * FRAM black-box log implementation
 */

#include "fram_log.h"
#include "../debug/Debug.h"
#include <Wire.h>
#include <rom/crc.h>

static const uint32_t HDR_MAGIC     = 0x4C524F52;  // "RORL"
static const uint16_t HDR_VERSION   = 1;
static const uint32_t SLOT_SIZE     = 48;
static const uint32_t RING_START    = 2 * SLOT_SIZE;
static const uint8_t  REC_MARK      = 0xA5;
static const uint32_t REC_HDR_SIZE  = 12;
static const size_t   I2C_CHUNK     = 64;          // well under the 128-byte Wire buffer

struct __attribute__((packed)) FramHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t bootId;
  uint32_t head;        // ring offset where the next record goes
  uint32_t tail;        // ring offset of the oldest record
  uint32_t used;        // bytes in use
  uint32_t flushed;     // ring offset of the oldest unflushed record
  uint32_t unflushed;   // bytes not yet archived
  uint32_t seq;         // commit counter (picks the newer slot)
  uint32_t dropped;     // unflushed records evicted before archiving
  uint32_t crc;
};

static FramHeader hdr;
static bool present = false;
static uint32_t chipSize = 0;
static uint32_t ringSize = 0;
static TaskHandle_t ownerTask = nullptr;

// ── Raw I2C access ───────────────────────────────────────────────────────────

static bool framWrite(uint32_t addr, const uint8_t* data, size_t len) {
  while (len > 0) {
    size_t n = len > I2C_CHUNK ? I2C_CHUNK : len;
    Wire.beginTransmission(FRAM_I2C_ADDRESS);
    Wire.write((uint8_t)(addr >> 8));
    Wire.write((uint8_t)(addr & 0xFF));
    Wire.write(data, n);
    if (Wire.endTransmission() != 0) return false;
    addr += n; data += n; len -= n;
  }
  return true;
}

static bool framRead(uint32_t addr, uint8_t* data, size_t len) {
  while (len > 0) {
    size_t n = len > I2C_CHUNK ? I2C_CHUNK : len;
    Wire.beginTransmission(FRAM_I2C_ADDRESS);
    Wire.write((uint8_t)(addr >> 8));
    Wire.write((uint8_t)(addr & 0xFF));
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)FRAM_I2C_ADDRESS, (uint8_t)n) != n) return false;
    for (size_t i = 0; i < n; i++) data[i] = Wire.read();
    addr += n; data += n; len -= n;
  }
  return true;
}

// Ring helpers: offsets are relative to RING_START and wrap at ringSize
static bool ringWrite(uint32_t pos, const uint8_t* data, size_t len) {
  size_t first = min((size_t)(ringSize - pos), len);
  if (!framWrite(RING_START + pos, data, first)) return false;
  return first == len || framWrite(RING_START, data + first, len - first);
}

static bool ringRead(uint32_t pos, uint8_t* data, size_t len) {
  size_t first = min((size_t)(ringSize - pos), len);
  if (!framRead(RING_START + pos, data, first)) return false;
  return first == len || framRead(RING_START, data + first, len - first);
}

static inline uint32_t ringAdvance(uint32_t pos, uint32_t n) { return (pos + n) % ringSize; }

// ── Header ───────────────────────────────────────────────────────────────────

static uint32_t headerCrc(const FramHeader& h) {
  return crc32_le(0, (const uint8_t*)&h, offsetof(FramHeader, crc));
}

static bool headerValid(const FramHeader& h) {
  return h.magic == HDR_MAGIC && h.version == HDR_VERSION && h.crc == headerCrc(h) &&
         h.head < ringSize && h.tail < ringSize && h.flushed < ringSize &&
         h.used <= ringSize && h.unflushed <= h.used;
}

static void commitHeader() {
  hdr.seq++;
  hdr.crc = headerCrc(hdr);
  framWrite((hdr.seq & 1) * SLOT_SIZE, (const uint8_t*)&hdr, sizeof(hdr));
}

static void formatLog(uint16_t bootId) {
  memset(&hdr, 0, sizeof(hdr));
  hdr.magic = HDR_MAGIC;
  hdr.version = HDR_VERSION;
  hdr.bootId = bootId;
  commitHeader();
  commitHeader();  // both slots valid
}

// ── Public API ───────────────────────────────────────────────────────────────

// Debug sink: called for every completed persistent log line
static void framSink(uint32_t utc, uint32_t ms, const char* text);

bool framLogBegin(uint16_t bootId) {
  Wire.beginTransmission(FRAM_I2C_ADDRESS);
  if (Wire.endTransmission() != 0) {
    Debug.println("FRAM not detected at 0x50 - log is RAM-only (lost on reset)");
    return false;
  }

  // Device ID (reserved slave 0xF8>>1): density nibble 5 = 256 Kbit, 6 = 512 Kbit.
  // Parts without an ID (or unexpected density) are treated as 32 KB.
  chipSize = 32768;
  Wire.beginTransmission(0x7C);
  Wire.write(FRAM_I2C_ADDRESS << 1);
  if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)0x7C, (uint8_t)3) == 3) {
    uint8_t d0 = Wire.read(), d1 = Wire.read(), d2 = Wire.read();
    uint16_t mfr = (d0 << 4) | (d1 >> 4);
    uint8_t density = d1 & 0x0F;
    if (density == 6) chipSize = 65536;
    Debug.printf("FRAM detected: mfr 0x%03X, product 0x%X%02X, %lu KB\n", mfr, d1 & 0x0F, d2, chipSize / 1024);
  } else {
    Debug.println("FRAM detected (no device ID) - assuming 32 KB");
  }
  ringSize = chipSize - RING_START;

  // Pick the newer valid header slot
  FramHeader a, b;
  bool aOk = framRead(0, (uint8_t*)&a, sizeof(a)) && headerValid(a);
  bool bOk = framRead(SLOT_SIZE, (uint8_t*)&b, sizeof(b)) && headerValid(b);
  if (aOk && bOk)  hdr = (a.seq > b.seq) ? a : b;
  else if (aOk)    hdr = a;
  else if (bOk)    hdr = b;
  else {
    Debug.println("FRAM log header invalid or blank - formatting");
    formatLog(bootId);
  }
  hdr.bootId = bootId;
  commitHeader();

  present = true;
  ownerTask = xTaskGetCurrentTaskHandle();
  Debug.printf("FRAM log: %lu/%lu bytes used, %lu unflushed, %lu dropped\n",
               hdr.used, ringSize, hdr.unflushed, hdr.dropped);

  logSetPersistSink(framSink);  // replays lines logged before FRAM was ready
  return true;
}

bool framLogPresent() { return present; }

static void framSink(uint32_t utc, uint32_t ms, const char* text) {
  // Only the loop task touches the I2C bus; lines from other tasks stay RAM-only
  if (!present || xTaskGetCurrentTaskHandle() != ownerTask) return;

  size_t len = strnlen(text, FRAM_LOG_TEXT_MAX);
  uint32_t need = REC_HDR_SIZE + len;

  // Evict oldest records until the new one fits
  while (hdr.used + need > ringSize && hdr.used > 0) {
    uint8_t lenByte[2];
    if (!ringRead(hdr.tail, lenByte, 2) || lenByte[0] != REC_MARK) {
      // Ring damaged (e.g. torn record): start over rather than loop on garbage
      formatLog(hdr.bootId);
      break;
    }
    uint32_t recSize = REC_HDR_SIZE + lenByte[1];
    if (hdr.tail == hdr.flushed && hdr.unflushed > 0) {
      hdr.flushed = ringAdvance(hdr.flushed, recSize);
      hdr.unflushed -= min(hdr.unflushed, recSize);
      hdr.dropped++;
    }
    hdr.tail = ringAdvance(hdr.tail, recSize);
    hdr.used -= recSize;
  }

  uint8_t rec[REC_HDR_SIZE + FRAM_LOG_TEXT_MAX];
  rec[0] = REC_MARK;
  rec[1] = (uint8_t)len;
  memcpy(rec + 2, &utc, 4);
  memcpy(rec + 6, &hdr.bootId, 2);
  memcpy(rec + 8, &ms, 4);
  memcpy(rec + REC_HDR_SIZE, text, len);
  if (!ringWrite(hdr.head, rec, need)) return;  // header not committed: record never existed

  if (hdr.unflushed == 0) hdr.flushed = hdr.head;
  hdr.head = ringAdvance(hdr.head, need);
  hdr.used += need;
  hdr.unflushed += need;
  commitHeader();
}

void framLogIterBegin(FramLogIter& it, bool unflushedOnly) {
  it.pos = unflushedOnly ? hdr.flushed : hdr.tail;
  it.remaining = present ? (unflushedOnly ? hdr.unflushed : hdr.used) : 0;
  it.consumed = 0;
}

bool framLogIterNext(FramLogIter& it, FramLogRecord& rec) {
  if (it.remaining < REC_HDR_SIZE) return false;
  uint8_t h[REC_HDR_SIZE];
  if (!ringRead(it.pos, h, REC_HDR_SIZE) || h[0] != REC_MARK) return false;
  rec.len = h[1];
  if (rec.len > FRAM_LOG_TEXT_MAX || REC_HDR_SIZE + rec.len > it.remaining) return false;
  memcpy(&rec.utc, h + 2, 4);
  memcpy(&rec.boot, h + 6, 2);
  memcpy(&rec.ms, h + 8, 4);
  if (!ringRead(ringAdvance(it.pos, REC_HDR_SIZE), (uint8_t*)rec.text, rec.len)) return false;
  rec.text[rec.len] = '\0';
  it.pos = ringAdvance(it.pos, REC_HDR_SIZE + rec.len);
  it.remaining -= REC_HDR_SIZE + rec.len;
  it.consumed += REC_HDR_SIZE + rec.len;
  return true;
}

// The iterator must have been started with unflushedOnly = true and nothing may
// be logged between iterating and calling this (log_archive guarantees both).
void framLogMarkFlushed(const FramLogIter& it) {
  if (!present || it.consumed == 0) return;
  hdr.flushed = it.pos;
  hdr.unflushed -= min(it.consumed, hdr.unflushed);
  commitHeader();
}

void framLogClear() {
  if (!present) return;
  uint16_t boot = hdr.bootId;
  formatLog(boot);
  Debug.println("FRAM log cleared");
}

uint32_t framLogCapacity()  { return present ? ringSize : 0; }
uint32_t framLogUsed()      { return present ? hdr.used : 0; }
uint32_t framLogUnflushed() { return present ? hdr.unflushed : 0; }
uint32_t framLogDropped()   { return present ? hdr.dropped : 0; }
uint32_t framLogSize()      { return present ? chipSize : 0; }
