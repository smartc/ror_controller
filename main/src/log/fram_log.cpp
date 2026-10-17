/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v4)
 * FRAM black-box log implementation
 *
 * All access holds the shared I2C lock. Records are written by the logger task;
 * the loop task only reads (console history, downloads) and clears.
 * If a write fails (FRAM unplugged, wiring fault) the log goes RAM-only and the
 * chip is re-probed every minute, so a bad bus costs one timeout, not one per line.
 */

#include "fram_log.h"
#include "../debug/Debug.h"
#include "../hw/i2c_bus.h"
#include <Wire.h>
#include <rom/crc.h>

static const uint32_t HDR_MAGIC     = 0x4C524F52;  // "RORL"
static const uint16_t HDR_VERSION   = 1;
static const uint32_t SLOT_SIZE     = 48;
static const uint32_t RING_START    = 2 * SLOT_SIZE;
static const uint8_t  REC_MARK      = 0xA5;
static const uint32_t REC_HDR_SIZE  = 12;
static const size_t   I2C_CHUNK     = 64;          // well under the 128-byte Wire buffer
static const uint32_t REPROBE_MS    = 60000;

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
static bool present = false;       // detected at boot
static bool online = false;        // usable right now
static unsigned long lastProbeMs = 0;
static uint16_t bootIdSaved = 0;
static uint32_t chipSize = 0;
static uint32_t ringSize = 0;

// -- Raw I2C access (caller holds the I2C lock) ----------------------------------

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

static void goOffline(const char* what) {
  if (!online) return;
  online = false;
  lastProbeMs = millis();
  // Queued to the logger, which skips FRAM while offline - no recursion
  Debug.printf("FRAM %s failed - log is RAM-only until FRAM responds again\n", what);
}

// -- Header ---------------------------------------------------------------------

static uint32_t headerCrc(const FramHeader& h) {
  return crc32_le(0, (const uint8_t*)&h, offsetof(FramHeader, crc));
}

static bool headerValid(const FramHeader& h) {
  return h.magic == HDR_MAGIC && h.version == HDR_VERSION && h.crc == headerCrc(h) &&
         h.head < ringSize && h.tail < ringSize && h.flushed < ringSize &&
         h.used <= ringSize && h.unflushed <= h.used;
}

static bool commitHeader() {
  hdr.seq++;
  hdr.crc = headerCrc(hdr);
  if (!framWrite((hdr.seq & 1) * SLOT_SIZE, (const uint8_t*)&hdr, sizeof(hdr))) {
    goOffline("header write");
    return false;
  }
  return true;
}

static void formatLog(uint16_t bootId) {
  memset(&hdr, 0, sizeof(hdr));
  hdr.magic = HDR_MAGIC;
  hdr.version = HDR_VERSION;
  hdr.bootId = bootId;
  commitHeader();
  commitHeader();  // both slots valid
}

// Reads the newer valid header slot, formatting if neither is valid
static bool loadHeader(uint16_t bootId) {
  FramHeader a, b;
  bool aRead = framRead(0, (uint8_t*)&a, sizeof(a));
  bool bRead = framRead(SLOT_SIZE, (uint8_t*)&b, sizeof(b));
  if (!aRead && !bRead) return false;
  bool aOk = aRead && headerValid(a);
  bool bOk = bRead && headerValid(b);
  if (aOk && bOk)  hdr = (a.seq > b.seq) ? a : b;
  else if (aOk)    hdr = a;
  else if (bOk)    hdr = b;
  else {
    Debug.println("FRAM log header invalid or blank - formatting");
    formatLog(bootId);
  }
  hdr.bootId = bootId;
  return commitHeader();
}

// -- Public API -----------------------------------------------------------------

bool framLogBegin(uint16_t bootId) {
  I2CLock lock;
  bootIdSaved = bootId;
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

  present = true;
  online = true;
  if (!loadHeader(bootId)) {
    goOffline("header read");
    return false;
  }
  Debug.printf("FRAM log: %lu/%lu bytes used, %lu unflushed, %lu dropped\n",
               hdr.used, ringSize, hdr.unflushed, hdr.dropped);
  return true;
}

bool framLogPresent() { return present && online; }

void framLogHandle() {
  if (!present || online || millis() - lastProbeMs < REPROBE_MS) return;
  I2CLock lock;
  lastProbeMs = millis();
  Wire.beginTransmission(FRAM_I2C_ADDRESS);
  if (Wire.endTransmission() != 0) return;
  online = true;
  if (loadHeader(bootIdSaved)) {
    Debug.println("FRAM responding again - persistent logging resumed");
  }
}

void framLogWriteLine(uint32_t utc, uint32_t ms, const char* text) {
  if (!present || !online) return;
  I2CLock lock;

  size_t len = strnlen(text, FRAM_LOG_TEXT_MAX);
  uint32_t need = REC_HDR_SIZE + len;

  // Evict oldest records until the new one fits
  while (hdr.used + need > ringSize && hdr.used > 0) {
    uint8_t lenByte[2];
    if (!ringRead(hdr.tail, lenByte, 2)) { goOffline("read"); return; }
    if (lenByte[0] != REC_MARK) {
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
  if (!ringWrite(hdr.head, rec, need)) {   // header not committed: record never existed
    goOffline("write");
    return;
  }

  if (hdr.unflushed == 0) hdr.flushed = hdr.head;
  hdr.head = ringAdvance(hdr.head, need);
  hdr.used += need;
  hdr.unflushed += need;
  commitHeader();
}

void framLogIterBegin(FramLogIter& it, bool unflushedOnly) {
  I2CLock lock;
  it.pos = unflushedOnly ? hdr.flushed : hdr.tail;
  it.remaining = framLogPresent() ? (unflushedOnly ? hdr.unflushed : hdr.used) : 0;
  it.consumed = 0;
  it.droppedAtStart = hdr.dropped;
}

bool framLogIterNext(FramLogIter& it, FramLogRecord& rec) {
  if (it.remaining < REC_HDR_SIZE || !framLogPresent()) return false;
  I2CLock lock;
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

// The iterator must have been started with unflushedOnly = true. If records were
// evicted while iterating (ring overflow), the eviction already advanced
// "flushed", so leave it alone rather than double-count.
void framLogMarkFlushed(const FramLogIter& it) {
  if (!framLogPresent() || it.consumed == 0) return;
  I2CLock lock;
  if (hdr.dropped != it.droppedAtStart) return;
  hdr.flushed = it.pos;
  hdr.unflushed -= min(it.consumed, hdr.unflushed);
  commitHeader();
}

void framLogClear() {
  if (!framLogPresent()) return;
  {
    I2CLock lock;
    formatLog(hdr.bootId);
  }
  Debug.println("FRAM log cleared");
}

uint32_t framLogCapacity()  { return framLogPresent() ? ringSize : 0; }
uint32_t framLogUsed()      { return framLogPresent() ? hdr.used : 0; }
uint32_t framLogUnflushed() { return framLogPresent() ? hdr.unflushed : 0; }
uint32_t framLogDropped()   { return framLogPresent() ? hdr.dropped : 0; }
uint32_t framLogSize()      { return present ? chipSize : 0; }
bool framLogDetected()      { return present; }
