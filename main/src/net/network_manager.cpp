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
#include <ping/ping_sock.h>

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
bool netGatewayCheck = true;
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

// Route priorities: the netif with the highest priority that has an IP is the
// default route. Core defaults are WiFi STA 100, Ethernet 50. An interface
// whose gateway has stopped answering drops below both.
static const int ROUTE_PRIO_PREFERRED = 150;
static const int ROUTE_PRIO_WIFI_DEFAULT = 100;
static const int ROUTE_PRIO_ETH_DEFAULT = 50;
static const int ROUTE_PRIO_DEMOTED = 10;

// Gateway reachability per NetInterface (true until a gateway check fails)
static bool gwReachable[2] = {true, true};

static void startAPMode();

// ── Modes ────────────────────────────────────────────────────────────────────

static const char* const MODE_KEYS[NET_MODE_COUNT]   = {"wifi", "", "dual", "wifi_eth", "eth_wifi"};
static const char* const MODE_LABELS[NET_MODE_COUNT] = {
  "WiFi only (no Ethernet fitted)", "", "Dual (both up)", "WiFi primary, Ethernet backup", "Ethernet primary, WiFi backup"};

bool netModeSelectable(int mode)   { return mode >= 0 && mode < NET_MODE_COUNT && mode != NET_MODE_RETIRED_ETH_ONLY; }
const char* netModeKey(int mode)   { return netModeSelectable(mode) ? MODE_KEYS[mode] : "wifi"; }
const char* netModeLabel(int mode) { return netModeSelectable(mode) ? MODE_LABELS[mode] : "?"; }

int netModeFromKey(const String& key) {
  for (int i = 0; i < NET_MODE_COUNT; i++) if (netModeSelectable(i) && key == MODE_KEYS[i]) return i;
  return -1;
}

static bool isFailoverMode(int mode) { return mode == NET_MODE_WIFI_FAILOVER || mode == NET_MODE_ETH_FAILOVER; }

// Which interface gets the default route when both have an IP
static NetInterface routePreferred() {
  switch (runMode) {
    case NET_MODE_DUAL:          return (NetInterface)netPreferred;
    case NET_MODE_ETH_FAILOVER:  return NET_IF_ETH;
    default:                     return NET_IF_WIFI;
  }
}

static int routePrioFor(NetInterface i) {
  if (!gwReachable[i]) return ROUTE_PRIO_DEMOTED;
  if (routePreferred() == i) return ROUTE_PRIO_PREFERRED;
  return i == NET_IF_ETH ? ROUTE_PRIO_ETH_DEFAULT : ROUTE_PRIO_WIFI_DEFAULT;
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
  netGatewayCheck = prefs.getBool(PREF_NET_GW_CHECK, true);
  String host  = prefs.getString(PREF_NET_HOSTNAME, DEFAULT_NET_HOSTNAME);
  prefs.end();

  // Settings saved before network modes existed only had the Ethernet toggle
  if (mode < 0 || mode >= NET_MODE_COUNT) mode = ethOn ? NET_MODE_DUAL : NET_MODE_WIFI_ONLY;
  if (mode == NET_MODE_RETIRED_ETH_ONLY) mode = NET_MODE_ETH_FAILOVER;  // WiFi stays the way back in
  netMode = mode;
  ethEnabled = netMode != NET_MODE_WIFI_ONLY;

  if (host.length() == 0) host = DEFAULT_NET_HOSTNAME;
  strncpy(netHostname, host.c_str(), sizeof(netHostname) - 1);
  netHostname[sizeof(netHostname) - 1] = '\0';

  Debug.printf("Network config: mode=%s, gateway check=%s, hostname=%s, preferred=%s (CS=%d SCK=%d MOSI=%d MISO=%d INT=%d RST=%d)\n",
               netModeLabel(netMode), netGatewayCheck ? "on" : "off", netHostname, netPreferred == NET_IF_ETH ? "Ethernet" : "WiFi",
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
  prefs.putBool(PREF_NET_GW_CHECK, netGatewayCheck);
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
  ETH.setRoutePrio(routePrioFor(NET_IF_ETH));
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
  WiFi.STA.setRoutePrio(routePrioFor(NET_IF_WIFI));
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
  if (runMode == NET_MODE_ETH_FAILOVER) {
    WiFi.mode(WIFI_OFF);
    wifiActive = false;
    Debug.println("WiFi on standby (Ethernet primary)");
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
      // Setup AP opened as the WiFi backup (no credentials): close it once
      // Ethernet is back, else keep it
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

// ── Gateway reachability ─────────────────────────────────────────────────────
// The primary interface (the route-preferred one) pings its gateway every 30 s,
// 3 echoes per probe. Three failed probes in a row mark it unreachable; one good
// probe marks it reachable again. Pings run on the lwIP ping task, so the loop
// only starts a probe and later collects the result.

static const unsigned long GW_PROBE_INTERVAL_MS = 30000;
static const int GW_FAIL_LIMIT = 3;
static volatile bool gwProbeDone = false;
static volatile uint32_t gwProbeReplies = 0;
static bool gwProbeRunning = false;
static int gwProbeIf = NET_IF_WIFI;
static int gwFailCount = 0;

static void onGwPingSuccess(esp_ping_handle_t, void*) { gwProbeReplies = gwProbeReplies + 1; }
static void onGwPingEnd(esp_ping_handle_t hdl, void*) {
  esp_ping_delete_session(hdl);
  gwProbeDone = true;
}

static bool interfaceHasIP(int i) { return i == NET_IF_ETH ? ethHasIP() : wifiIsConnected(); }
static const char* ifName(int i)  { return i == NET_IF_ETH ? "Ethernet" : "WiFi"; }

static void setGatewayReachable(int i, bool reachable, const char* why) {
  if (gwReachable[i] == reachable) return;
  gwReachable[i] = reachable;
  Debug.printf("Gateway check: %s %s\n", ifName(i), why);
  if (ethStarted) ETH.setRoutePrio(routePrioFor(NET_IF_ETH));
  if (WiFi.getMode() & WIFI_MODE_STA) WiFi.STA.setRoutePrio(routePrioFor(NET_IF_WIFI));
}

static void handleGatewayCheck() {
  static unsigned long lastProbe = 0;

  if (gwProbeRunning) {
    if (!gwProbeDone) return;
    gwProbeRunning = false;
    if (gwProbeReplies > 0) {
      gwFailCount = 0;
      setGatewayReachable(gwProbeIf, true, "gateway answering again - link usable");
    } else {
      gwFailCount++;
      Debug.printf(2, "Gateway check: %s gateway did not answer (%d/%d)\n", ifName(gwProbeIf), gwFailCount, GW_FAIL_LIMIT);
      if (gwFailCount >= GW_FAIL_LIMIT) {
        setGatewayReachable(gwProbeIf, false, "gateway not answering - treating link as down");
      }
    }
    return;
  }

  int target = routePreferred();
  if (!netGatewayCheck || runMode == NET_MODE_WIFI_ONLY || !interfaceHasIP(target)) {
    // Nothing to check; losing the IP is handled as "down" on its own
    gwFailCount = 0;
    setGatewayReachable(NET_IF_WIFI, true, "gateway check idle");
    setGatewayReachable(NET_IF_ETH, true, "gateway check idle");
    return;
  }
  if (lastProbe != 0 && millis() - lastProbe < GW_PROBE_INTERVAL_MS) return;
  lastProbe = millis();

  IPAddress gw = target == NET_IF_ETH ? ETH.gatewayIP() : WiFi.gatewayIP();
  esp_netif_t* nif = target == NET_IF_ETH ? ETH.netif() : WiFi.STA.netif();
  if (gw == IPAddress(0, 0, 0, 0) || nif == nullptr) return;  // no gateway (e.g. isolated LAN)

  esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
  IP_ADDR4(&cfg.target_addr, gw[0], gw[1], gw[2], gw[3]);
  cfg.count = 3;
  cfg.interval_ms = 1000;
  cfg.timeout_ms = 1000;
  cfg.interface = esp_netif_get_netif_impl_index(nif);  // probe out of this interface only

  esp_ping_callbacks_t cbs = {};
  cbs.on_ping_success = onGwPingSuccess;
  cbs.on_ping_end = onGwPingEnd;

  esp_ping_handle_t hdl;
  gwProbeReplies = 0;
  gwProbeDone = false;
  if (esp_ping_new_session(&cfg, &cbs, &hdl) == ESP_OK) {
    gwProbeIf = target;
    gwProbeRunning = true;
    esp_ping_start(hdl);
  }
}

// Failover modes: start the backup when the primary has been down for a while,
// stop it again once the primary has been stable.
static void handleFailover() {
  static bool primaryWasUp = false;
  static unsigned long primaryChangedAt = 0;
  if (!isFailoverMode(runMode)) return;

  bool wifiPrimary = runMode == NET_MODE_WIFI_FAILOVER;
  bool primaryUp = wifiPrimary ? (wifiIsConnected() && gwReachable[NET_IF_WIFI])
                               : (ethHasIP() && gwReachable[NET_IF_ETH]);
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
  if (wifiUp && ethUp) {
    NetInterface pref = routePreferred();
    NetInterface other = pref == NET_IF_ETH ? NET_IF_WIFI : NET_IF_ETH;
    return (gwReachable[pref] || !gwReachable[other]) ? pref : other;
  }
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
  handleGatewayCheck();
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
  net["gateway_check"] = netGatewayCheck;
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
  wifi["gateway_ok"] = gwReachable[NET_IF_WIFI];

  JsonObject eth = net.createNestedObject("ethernet");
  eth["enabled"] = runMode != NET_MODE_WIFI_ONLY;
  eth["started"] = ethStarted;
  eth["standby"] = ethStarted && !ethActive;
  eth["link"] = ethLinkUp();
  eth["speed"] = ethLinkSpeed();
  eth["full_duplex"] = ethLinkUp() && ETH.fullDuplex();
  eth["ip"] = ethHasIP() ? ETH.localIP().toString() : String("");
  eth["mac"] = ethMacAddress();
  eth["gateway_ok"] = gwReachable[NET_IF_ETH];
}
