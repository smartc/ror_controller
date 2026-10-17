/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v4)
 * Network Manager Implementation
 */

#include "network_manager.h"
#include "../debug/Debug.h"
#include "../sensors/gps_handler.h"
#include <WiFi.h>
#include <ETH.h>
#include <SPI.h>
#include <Preferences.h>
#include <esp_mac.h>

// WiFi credentials and AP state
char ssid[SSID_SIZE] = DEFAULT_WIFI_SSID;
char password[PASSWORD_SIZE] = DEFAULT_WIFI_PASSWORD;
bool apMode = false;
static unsigned long apStartTime = 0;

// Ethernet / routing settings
int netMode = NET_MODE_WIFI_ONLY;
bool ethEnabled = false;
int ethCsPin = DEFAULT_ETH_CS_PIN;
int ethSckPin = DEFAULT_ETH_SCK_PIN;
int ethMosiPin = DEFAULT_ETH_MOSI_PIN;
int ethMisoPin = DEFAULT_ETH_MISO_PIN;
int ethIrqPin = DEFAULT_ETH_IRQ_PIN;
int ethRstPin = DEFAULT_ETH_RST_PIN;
int netPreferred = NET_IF_WIFI;
char netHostname[NET_HOSTNAME_SIZE] = DEFAULT_NET_HOSTNAME;

static SPIClass ethSPI(FSPI);
static bool ethStarted = false;   // W5500 driver installed
static bool ethActive = false;    // Driver running (false = failover standby)
static bool wifiActive = false;   // STA wanted (false = radio off / standby)

// Mode actually running: netMode, or WiFi-only if the W5500 failed to start
static int runMode = NET_MODE_WIFI_ONLY;

// WiFi reconnection state (non-blocking)
static bool wifiReconnecting = false;
static unsigned long wifiReconnectStartTime = 0;
static const unsigned long WIFI_RECONNECT_TIMEOUT = 30000;  // 30 seconds max

// Failover timing
static const unsigned long FAILOVER_DELAY_MS = 15000;   // primary down this long -> start backup
static const unsigned long FAILBACK_HOLD_MS = 60000;    // primary back this long -> stop backup
static const unsigned long ETH_ONLY_AP_DELAY_MS = 60000; // Ethernet-only: no IP this long -> setup AP

// Route priorities: the netif with the highest priority that has an IP is the
// default route. Core defaults are WiFi STA 100, Ethernet 50.
static const int ROUTE_PRIO_PREFERRED = 150;
static const int ROUTE_PRIO_ETH_DEFAULT = 50;

static void startAPMode();

// ── Modes ────────────────────────────────────────────────────────────────────

static const char* const MODE_KEYS[NET_MODE_COUNT]   = {"wifi", "eth", "dual", "wifi_eth", "eth_wifi"};
static const char* const MODE_LABELS[NET_MODE_COUNT] = {
  "WiFi only", "Ethernet only", "Dual (both up)", "WiFi, Ethernet failover", "Ethernet, WiFi failover"};

const char* netModeKey(int mode)   { return (mode >= 0 && mode < NET_MODE_COUNT) ? MODE_KEYS[mode] : "wifi"; }
const char* netModeLabel(int mode) { return (mode >= 0 && mode < NET_MODE_COUNT) ? MODE_LABELS[mode] : "?"; }

int netModeFromKey(const String& key) {
  for (int i = 0; i < NET_MODE_COUNT; i++) if (key == MODE_KEYS[i]) return i;
  return -1;
}

static bool isFailoverMode(int mode) { return mode == NET_MODE_WIFI_FAILOVER || mode == NET_MODE_ETH_FAILOVER; }

// Which interface gets the default route when both have an IP
static NetInterface routePreferred() {
  switch (runMode) {
    case NET_MODE_DUAL:          return (NetInterface)netPreferred;
    case NET_MODE_ETH_ONLY:
    case NET_MODE_ETH_FAILOVER:  return NET_IF_ETH;
    default:                     return NET_IF_WIFI;
  }
}

// ── Configuration ────────────────────────────────────────────────────────────

void loadNetworkConfiguration() {
  Preferences prefs;
  prefs.begin(PREFERENCES_NAMESPACE, true);
  int mode     = prefs.getInt(PREF_NET_MODE, -1);
  bool ethOn   = prefs.getBool(PREF_ETH_ENABLED, false);
  ethCsPin     = prefs.getInt(PREF_ETH_CS_PIN, DEFAULT_ETH_CS_PIN);
  ethSckPin    = prefs.getInt(PREF_ETH_SCK_PIN, DEFAULT_ETH_SCK_PIN);
  ethMosiPin   = prefs.getInt(PREF_ETH_MOSI_PIN, DEFAULT_ETH_MOSI_PIN);
  ethMisoPin   = prefs.getInt(PREF_ETH_MISO_PIN, DEFAULT_ETH_MISO_PIN);
  ethIrqPin    = prefs.getInt(PREF_ETH_IRQ_PIN, DEFAULT_ETH_IRQ_PIN);
  ethRstPin    = prefs.getInt(PREF_ETH_RST_PIN, DEFAULT_ETH_RST_PIN);
  netPreferred = prefs.getInt(PREF_NET_PREFERRED, NET_IF_WIFI);
  String host  = prefs.getString(PREF_NET_HOSTNAME, DEFAULT_NET_HOSTNAME);
  prefs.end();

  // Settings saved before network modes existed only had the Ethernet toggle
  if (mode < 0 || mode >= NET_MODE_COUNT) mode = ethOn ? NET_MODE_DUAL : NET_MODE_WIFI_ONLY;
  netMode = mode;
  ethEnabled = netMode != NET_MODE_WIFI_ONLY;

  if (host.length() == 0) host = DEFAULT_NET_HOSTNAME;
  strncpy(netHostname, host.c_str(), sizeof(netHostname) - 1);
  netHostname[sizeof(netHostname) - 1] = '\0';

  Debug.printf("Network config: mode=%s, hostname=%s, preferred=%s (CS=%d SCK=%d MOSI=%d MISO=%d INT=%d RST=%d)\n",
               netModeLabel(netMode), netHostname, netPreferred == NET_IF_ETH ? "Ethernet" : "WiFi",
               ethCsPin, ethSckPin, ethMosiPin, ethMisoPin, ethIrqPin, ethRstPin);
}

void saveNetworkConfiguration() {
  ethEnabled = netMode != NET_MODE_WIFI_ONLY;
  Preferences prefs;
  prefs.begin(PREFERENCES_NAMESPACE, false);
  prefs.putInt(PREF_NET_MODE, netMode);
  prefs.putBool(PREF_ETH_ENABLED, ethEnabled);  // kept for older firmware
  prefs.putInt(PREF_ETH_CS_PIN, ethCsPin);
  prefs.putInt(PREF_ETH_SCK_PIN, ethSckPin);
  prefs.putInt(PREF_ETH_MOSI_PIN, ethMosiPin);
  prefs.putInt(PREF_ETH_MISO_PIN, ethMisoPin);
  prefs.putInt(PREF_ETH_IRQ_PIN, ethIrqPin);
  prefs.putInt(PREF_ETH_RST_PIN, ethRstPin);
  prefs.putInt(PREF_NET_PREFERRED, netPreferred);
  prefs.putString(PREF_NET_HOSTNAME, netHostname);
  prefs.end();
  Debug.println("Network configuration saved");
}

String ethPinConflict(int cs, int sck, int mosi, int miso, int irq, int rst) {
  const int pins[] = {cs, sck, mosi, miso, irq, rst};
  const char* names[] = {"CS", "SCK", "MOSI", "MISO", "INT", "RST"};
  for (int i = 0; i < 6; i++) {
    if (i < 4 && pins[i] < 0) return String(names[i]) + " pin must be set";
    if (pins[i] < 0) continue;
    if (pins[i] > 48 || (pins[i] >= 22 && pins[i] <= 25)) return String(names[i]) + " is not a valid GPIO";
    for (int j = i + 1; j < 6; j++) {
      if (pins[i] == pins[j]) return String(names[i]) + " and " + names[j] + " share GPIO" + String(pins[i]);
    }
    if (gpsEnabled && (pins[i] == gpsTxPin || pins[i] == gpsRxPin || pins[i] == gpsPpsPin)) {
      return String(names[i]) + " (GPIO" + String(pins[i]) + ") is also used by the GPS";
    }
  }
  return "";
}

// ── Ethernet ─────────────────────────────────────────────────────────────────

static void initEthernet() {
  if (runMode == NET_MODE_WIFI_ONLY) {
    Debug.println("Ethernet disabled (WiFi only)");
    return;
  }
  String conflict = ethPinConflict(ethCsPin, ethSckPin, ethMosiPin, ethMisoPin, ethIrqPin, ethRstPin);
  if (conflict.length() > 0) {
    Debug.println("Ethernet NOT started - pin conflict: " + conflict + "; falling back to WiFi only");
    runMode = NET_MODE_WIFI_ONLY;
    return;
  }

  Debug.printf("Starting W5500 Ethernet (CS=%d SCK=%d MOSI=%d MISO=%d INT=%d RST=%d, %d MHz)\n",
               ethCsPin, ethSckPin, ethMosiPin, ethMisoPin, ethIrqPin, ethRstPin, ETH_SPI_MHZ);
  ethSPI.begin(ethSckPin, ethMisoPin, ethMosiPin);
  ethStarted = ETH.begin(ETH_PHY_W5500, 1, ethCsPin, ethIrqPin, ethRstPin, ethSPI, ETH_SPI_MHZ);
  if (!ethStarted) {
    // Never leave the controller with no usable interface
    Debug.println("Ethernet start FAILED - check W5500 wiring and power; falling back to WiFi only");
    runMode = NET_MODE_WIFI_ONLY;
    return;
  }
  ethActive = true;
  ETH.setRoutePrio(routePreferred() == NET_IF_ETH ? ROUTE_PRIO_PREFERRED : ROUTE_PRIO_ETH_DEFAULT);
  Debug.println("Ethernet MAC: " + ETH.macAddress());

  if (runMode == NET_MODE_WIFI_FAILOVER) {
    esp_eth_stop(ETH.handle());
    ethActive = false;
    Debug.println("Ethernet on standby (WiFi primary)");
  }
}

static void ethBackupUp() {
  if (!ethStarted || ethActive) return;
  if (esp_eth_start(ETH.handle()) == ESP_OK) ethActive = true;
  else Debug.println("Failover: Ethernet failed to start");
}

static void ethBackupDown() {
  if (!ethStarted || !ethActive) return;
  esp_eth_stop(ETH.handle());
  ethActive = false;
}

bool ethIsStarted()     { return ethStarted; }
bool ethHasIP()         { return ethStarted && ethActive && ETH.hasIP(); }
bool ethLinkUp()        { return ethStarted && ethActive && ETH.linkUp(); }
IPAddress ethLocalIP()  { return ethHasIP() ? ETH.localIP() : IPAddress(); }
String ethMacAddress()  { return ethStarted ? ETH.macAddress() : String(""); }
uint16_t ethLinkSpeed() { return ethLinkUp() ? ETH.linkSpeed() : 0; }

// ── WiFi ─────────────────────────────────────────────────────────────────────

bool wifiIsConnected() { return !apMode && wifiActive && WiFi.status() == WL_CONNECTED; }

String wifiMacAddress() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

static bool wifiCredentialsSet() {
  return strlen(ssid) > 0 && strcmp(ssid, "YOUR_SSID") != 0;
}

static void applyWiFiRoutePrio() {
  // STA keeps its default priority (100) unless it is the preferred interface
  if (routePreferred() == NET_IF_WIFI) WiFi.STA.setRoutePrio(ROUTE_PRIO_PREFERRED);
}

// Start a STA connection attempt without waiting for it
static void beginWiFiSTA() {
  WiFi.setSleep(WIFI_PS_NONE);  // Power saving caused disconnects
  WiFi.mode(WIFI_STA);
  delay(100);
  applyWiFiRoutePrio();
  WiFi.begin(ssid, password);
  wifiActive = true;
  wifiReconnecting = true;
  wifiReconnectStartTime = millis();
}

static void wifiRadioOff() {
  if (apMode) WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  apMode = false;
  wifiActive = false;
  wifiReconnecting = false;
}

static void initWiFi() {
  if (runMode == NET_MODE_ETH_ONLY || runMode == NET_MODE_ETH_FAILOVER) {
    WiFi.mode(WIFI_OFF);
    wifiActive = false;
    Debug.println(runMode == NET_MODE_ETH_ONLY ? "WiFi off (Ethernet only)" : "WiFi on standby (Ethernet primary)");
    return;
  }

  Debug.println("Initializing WiFi...");
  wifiActive = true;

  // Clean up any existing WiFi state
  WiFi.disconnect(true);  // Disconnect and clear stored credentials
  WiFi.softAPdisconnect(true);  // Stop any running AP
  delay(100);  // Allow cleanup to complete

  // Check if we have credentials to connect with
  if (wifiCredentialsSet()) {
    Debug.printf("Connecting to WiFi network: %s\n", ssid);
    beginWiFiSTA();

    // Wait up to 30 seconds for connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 60) {
      delay(500);
      Debug.print(".");
      attempts++;
    }
    Debug.println("");

    if (WiFi.status() == WL_CONNECTED) {
      Debug.println("WiFi connected successfully!");
      Debug.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
      Debug.printf("Signal strength: %d dBm\n", WiFi.RSSI());
      wifiReconnecting = false;
      apMode = false;
    } else if (runMode == NET_MODE_WIFI_FAILOVER) {
      // Don't wait for the failover delay at boot: bring Ethernet up now and
      // keep retrying WiFi. AP mode follows only if Ethernet gets no IP either.
      Debug.printf("Failed to connect to WiFi (status: %d); starting Ethernet backup\n", WiFi.status());
      ethBackupUp();
      wifiReconnectStartTime = millis();
    } else if (ethHasIP()) {
      // Reachable over Ethernet, so stay in STA mode and keep retrying WiFi
      // instead of dropping into AP mode.
      Debug.printf("Failed to connect to WiFi (status: %d); Ethernet is up, so not starting AP mode\n", WiFi.status());
      wifiReconnectStartTime = millis();
    } else {
      Debug.printf("Failed to connect to WiFi (status: %d), starting AP mode\n", WiFi.status());
      wifiReconnecting = false;
      startAPMode();
    }
  } else if (ethHasIP()) {
    Debug.println("No WiFi credentials configured; Ethernet is up, so not starting AP mode");
  } else {
    Debug.println("No valid WiFi credentials configured, starting AP mode");
    startAPMode();
  }
}

// Start Access Point mode for configuration
static void startAPMode() {
  Debug.println("Starting AP mode...");

  // Clean up WiFi state before starting AP
  WiFi.disconnect(true);
  delay(100);

  // Set WiFi mode to AP
  WiFi.mode(WIFI_AP);
  delay(100);

  // The AP password is printed on the setup banner; keep it out of the logs
  if (WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Debug.printf("AP mode started - SSID: %s\n", AP_SSID);
    Debug.printf("AP IP address: %s\n", WiFi.softAPIP().toString().c_str());
    apMode = true;
    apStartTime = millis();
  } else {
    Debug.println("Failed to start AP mode!");
    // Try one more time after a longer delay
    delay(1000);
    if (WiFi.softAP(AP_SSID, AP_PASSWORD)) {
      Debug.println("AP mode started on retry");
      Debug.printf("AP IP address: %s\n", WiFi.softAPIP().toString().c_str());
      apMode = true;
      apStartTime = millis();
    } else {
      Debug.println("AP mode failed to start after retry!");
    }
  }
}

// Handle WiFi connection in main loop (non-blocking apart from short settle delays)
static void handleWiFi() {
  if (apMode) {
    if (millis() - apStartTime <= AP_TIMEOUT) return;

    if (!wifiActive) {
      // Ethernet-only setup AP: close it once Ethernet is back, else keep it
      if (ethHasIP()) {
        Debug.println("AP mode timeout; Ethernet is up, turning WiFi off");
        wifiRadioOff();
      } else {
        apStartTime = millis();
      }
      return;
    }
    Debug.println("AP mode timeout, attempting to reconnect to WiFi");
    WiFi.softAPdisconnect(true);
    apMode = false;
    delay(100);
    beginWiFiSTA();
    return;
  }

  if (!wifiActive || !wifiCredentialsSet()) return;

  if (WiFi.status() != WL_CONNECTED) {
    if (!wifiReconnecting) {
      // Start reconnection attempt
      Debug.printf("WiFi connection lost (status: %d), attempting to reconnect...\n", WiFi.status());

      // Use disconnect/begin instead of just reconnect()
      WiFi.disconnect();
      delay(100);
      WiFi.begin(ssid, password);

      wifiReconnecting = true;
      wifiReconnectStartTime = millis();
    } else if (millis() - wifiReconnectStartTime > WIFI_RECONNECT_TIMEOUT) {
      if (ethHasIP()) {
        // Still reachable over Ethernet: no AP, just start another attempt
        Debug.printf(2, "WiFi still down after %lu s (status: %d); retrying (Ethernet is up)\n",
                     WIFI_RECONNECT_TIMEOUT / 1000, WiFi.status());
        WiFi.disconnect();
        delay(100);
        WiFi.begin(ssid, password);
        wifiReconnectStartTime = millis();
      } else {
        Debug.printf("Failed to reconnect after %lu seconds (status: %d), starting AP mode\n",
                     WIFI_RECONNECT_TIMEOUT / 1000, WiFi.status());
        wifiReconnecting = false;
        startAPMode();
      }
    }
    // Otherwise, let the reconnection continue in the background
  } else if (wifiReconnecting) {
    Debug.println("WiFi reconnected successfully!");
    Debug.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
    wifiReconnecting = false;
  }
}

// Ethernet-only: open the setup AP if Ethernet stays down, so the controller
// can always be reached to fix its settings.
static void handleEthOnlyFallback() {
  static unsigned long noIpSince = 0;
  static bool noIp = false;
  if (runMode != NET_MODE_ETH_ONLY) return;

  if (ethHasIP()) { noIp = false; return; }
  if (!noIp) { noIp = true; noIpSince = millis(); }
  if (!apMode && millis() - noIpSince >= ETH_ONLY_AP_DELAY_MS) {
    Debug.printf("Ethernet has had no IP for %lu s; starting setup access point\n", ETH_ONLY_AP_DELAY_MS / 1000);
    startAPMode();
  }
}

// Failover modes: start the backup when the primary has been down for a while,
// stop it again once the primary has been stable.
static void handleFailover() {
  static bool primaryWasUp = false;
  static unsigned long primaryChangedAt = 0;
  if (!isFailoverMode(runMode)) return;

  bool wifiPrimary = runMode == NET_MODE_WIFI_FAILOVER;
  bool primaryUp = wifiPrimary ? wifiIsConnected() : ethHasIP();
  bool backupRunning = wifiPrimary ? ethActive : (wifiActive || apMode);
  const char* primaryName = wifiPrimary ? "WiFi" : "Ethernet";
  const char* backupName = wifiPrimary ? "Ethernet" : "WiFi";

  unsigned long now = millis();
  if (primaryUp != primaryWasUp) {
    primaryWasUp = primaryUp;
    primaryChangedAt = now;
  }
  unsigned long held = now - primaryChangedAt;

  if (!backupRunning && !primaryUp && held >= FAILOVER_DELAY_MS) {
    Debug.printf("Failover: %s down for %lu s, starting %s\n", primaryName, held / 1000, backupName);
    if (wifiPrimary) {
      ethBackupUp();
    } else if (wifiCredentialsSet()) {
      beginWiFiSTA();
    } else {
      startAPMode();  // no credentials: the setup AP is the only way in
    }
  } else if (backupRunning && primaryUp && held >= FAILBACK_HOLD_MS) {
    Debug.printf("Failback: %s stable for %lu s, stopping %s\n", primaryName, held / 1000, backupName);
    if (wifiPrimary) ethBackupDown();
    else wifiRadioOff();
  }
}

// ── Status ───────────────────────────────────────────────────────────────────

bool netIsConnected() { return wifiIsConnected() || ethHasIP(); }

static NetInterface defaultInterface() {
  bool wifiUp = wifiIsConnected(), ethUp = ethHasIP();
  if (wifiUp && ethUp) return routePreferred();
  return ethUp ? NET_IF_ETH : NET_IF_WIFI;
}

IPAddress netLocalIP() {
  if (apMode && !ethHasIP()) return WiFi.softAPIP();
  if (!netIsConnected()) return IPAddress();
  return defaultInterface() == NET_IF_ETH ? ETH.localIP() : WiFi.localIP();
}

const char* netActiveInterfaceName() {
  if (netIsConnected()) return defaultInterface() == NET_IF_ETH ? "Ethernet" : "WiFi";
  return apMode ? "AP" : "None";
}

// Log every link/IP/default-route transition, polled from loop() so logging and
// the FRAM write it triggers never run on the network event task.
static void logTransitions() {
  static bool lastEthLink = false, lastEthIP = false, lastWifiUp = false;
  static String lastActive = "";

  bool ethLink = ethLinkUp(), ethIP = ethHasIP(), wifiUp = wifiIsConnected();
  if (ethLink != lastEthLink) {
    if (ethLink) Debug.printf("Ethernet link UP (%u Mbps, %s duplex)\n", ETH.linkSpeed(), ETH.fullDuplex() ? "full" : "half");
    else         Debug.println(ethActive ? "Ethernet link DOWN" : "Ethernet stopped");
    lastEthLink = ethLink;
  }
  if (ethIP != lastEthIP) {
    if (ethIP) Debug.printf("Ethernet got IP %s\n", ETH.localIP().toString().c_str());
    else       Debug.println("Ethernet lost IP");
    lastEthIP = ethIP;
  }
  if (wifiUp != lastWifiUp) {
    if (wifiUp) Debug.printf("WiFi up: %s (%d dBm)\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    else        Debug.println(wifiActive ? "WiFi down" : "WiFi off");
    lastWifiUp = wifiUp;
  }
  String active = String(netActiveInterfaceName());
  if (active != lastActive) {
    Debug.printf("Network: active interface now %s (%s)\n", active.c_str(), netLocalIP().toString().c_str());
    lastActive = active;
  }
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

void initNetwork() {
  runMode = netMode;
  Network.setHostname(netHostname);
  // Ethernet first: its DHCP completes while WiFi is connecting, so a failed
  // WiFi join can see that Ethernet is up and skip AP mode.
  initEthernet();
  initWiFi();
  if (runMode != netMode) Debug.printf("Network running as %s (configured: %s)\n", netModeLabel(runMode), netModeLabel(netMode));
  logTransitions();
}

void handleNetwork() {
  handleWiFi();
  handleEthOnlyFallback();
  handleFailover();
  logTransitions();
}

void fillNetworkStatus(JsonObject net) {
  net["mode"] = netModeKey(netMode);
  net["mode_label"] = netModeLabel(netMode);
  net["running_mode"] = netModeKey(runMode);
  net["active"] = netActiveInterfaceName();
  net["ip"] = netLocalIP().toString();
  net["hostname"] = netHostname;
  net["preferred"] = routePreferred() == NET_IF_ETH ? "Ethernet" : "WiFi";
  net["ap_mode"] = apMode;

  JsonObject wifi = net.createNestedObject("wifi");
  bool wifiUp = wifiIsConnected();
  wifi["enabled"] = wifiActive || apMode;
  wifi["standby"] = runMode == NET_MODE_ETH_FAILOVER && !wifiActive && !apMode;
  wifi["connected"] = wifiUp;
  wifi["ssid"] = wifiUp ? WiFi.SSID() : String("");
  wifi["ip"] = wifiUp ? WiFi.localIP().toString() : String("");
  wifi["rssi"] = wifiUp ? WiFi.RSSI() : 0;
  wifi["mac"] = wifiMacAddress();

  JsonObject eth = net.createNestedObject("ethernet");
  eth["enabled"] = runMode != NET_MODE_WIFI_ONLY;
  eth["started"] = ethStarted;
  eth["standby"] = ethStarted && !ethActive;
  eth["link"] = ethLinkUp();
  eth["speed"] = ethLinkSpeed();
  eth["full_duplex"] = ethLinkUp() && ETH.fullDuplex();
  eth["ip"] = ethHasIP() ? ETH.localIP().toString() : String("");
  eth["mac"] = ethMacAddress();
}
