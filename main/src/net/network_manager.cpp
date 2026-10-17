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

// WiFi credentials and AP state
char ssid[SSID_SIZE] = DEFAULT_WIFI_SSID;
char password[PASSWORD_SIZE] = DEFAULT_WIFI_PASSWORD;
bool apMode = false;
static unsigned long apStartTime = 0;

// Ethernet / routing settings
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
static bool ethStarted = false;

// WiFi reconnection state (non-blocking)
static bool wifiReconnecting = false;
static unsigned long wifiReconnectStartTime = 0;
static const unsigned long WIFI_RECONNECT_TIMEOUT = 30000;  // 30 seconds max

// Route priorities: the netif with the highest priority that has an IP is the
// default route. Core defaults are WiFi STA 100, Ethernet 50.
static const int ROUTE_PRIO_PREFERRED = 150;
static const int ROUTE_PRIO_ETH_DEFAULT = 50;

static void startAPMode();

// ── Configuration ────────────────────────────────────────────────────────────

void loadNetworkConfiguration() {
  Preferences prefs;
  prefs.begin(PREFERENCES_NAMESPACE, true);
  ethEnabled   = prefs.getBool(PREF_ETH_ENABLED, false);
  ethCsPin     = prefs.getInt(PREF_ETH_CS_PIN, DEFAULT_ETH_CS_PIN);
  ethSckPin    = prefs.getInt(PREF_ETH_SCK_PIN, DEFAULT_ETH_SCK_PIN);
  ethMosiPin   = prefs.getInt(PREF_ETH_MOSI_PIN, DEFAULT_ETH_MOSI_PIN);
  ethMisoPin   = prefs.getInt(PREF_ETH_MISO_PIN, DEFAULT_ETH_MISO_PIN);
  ethIrqPin    = prefs.getInt(PREF_ETH_IRQ_PIN, DEFAULT_ETH_IRQ_PIN);
  ethRstPin    = prefs.getInt(PREF_ETH_RST_PIN, DEFAULT_ETH_RST_PIN);
  netPreferred = prefs.getInt(PREF_NET_PREFERRED, NET_IF_WIFI);
  String host  = prefs.getString(PREF_NET_HOSTNAME, DEFAULT_NET_HOSTNAME);
  prefs.end();

  if (host.length() == 0) host = DEFAULT_NET_HOSTNAME;
  strncpy(netHostname, host.c_str(), sizeof(netHostname) - 1);
  netHostname[sizeof(netHostname) - 1] = '\0';

  Debug.printf("Network config: hostname=%s, preferred=%s, Ethernet=%s (CS=%d SCK=%d MOSI=%d MISO=%d INT=%d RST=%d)\n",
               netHostname, netPreferred == NET_IF_ETH ? "Ethernet" : "WiFi",
               ethEnabled ? "enabled" : "disabled",
               ethCsPin, ethSckPin, ethMosiPin, ethMisoPin, ethIrqPin, ethRstPin);
}

void saveNetworkConfiguration() {
  Preferences prefs;
  prefs.begin(PREFERENCES_NAMESPACE, false);
  prefs.putBool(PREF_ETH_ENABLED, ethEnabled);
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
  if (!ethEnabled) {
    Debug.println("Ethernet disabled");
    return;
  }
  String conflict = ethPinConflict(ethCsPin, ethSckPin, ethMosiPin, ethMisoPin, ethIrqPin, ethRstPin);
  if (conflict.length() > 0) {
    Debug.println("Ethernet NOT started - pin conflict: " + conflict);
    return;
  }

  Debug.printf("Starting W5500 Ethernet (CS=%d SCK=%d MOSI=%d MISO=%d INT=%d RST=%d, %d MHz)\n",
               ethCsPin, ethSckPin, ethMosiPin, ethMisoPin, ethIrqPin, ethRstPin, ETH_SPI_MHZ);
  ethSPI.begin(ethSckPin, ethMisoPin, ethMosiPin);
  ethStarted = ETH.begin(ETH_PHY_W5500, 1, ethCsPin, ethIrqPin, ethRstPin, ethSPI, ETH_SPI_MHZ);
  if (!ethStarted) {
    Debug.println("Ethernet start FAILED - check W5500 wiring and power");
    return;
  }
  ETH.setRoutePrio(netPreferred == NET_IF_ETH ? ROUTE_PRIO_PREFERRED : ROUTE_PRIO_ETH_DEFAULT);
  Debug.println("Ethernet MAC: " + ETH.macAddress());
}

bool ethIsStarted()     { return ethStarted; }
bool ethHasIP()         { return ethStarted && ETH.hasIP(); }
bool ethLinkUp()        { return ethStarted && ETH.linkUp(); }
IPAddress ethLocalIP()  { return ethStarted ? ETH.localIP() : IPAddress(); }
String ethMacAddress()  { return ethStarted ? ETH.macAddress() : String(""); }
uint16_t ethLinkSpeed() { return ethLinkUp() ? ETH.linkSpeed() : 0; }

// ── WiFi ─────────────────────────────────────────────────────────────────────

bool wifiIsConnected() { return !apMode && WiFi.status() == WL_CONNECTED; }

static void applyWiFiRoutePrio() {
  // STA keeps its default priority (100) unless it is the preferred interface
  if (netPreferred == NET_IF_WIFI) WiFi.STA.setRoutePrio(ROUTE_PRIO_PREFERRED);
}

static void initWiFi() {
  Debug.println("Initializing WiFi...");

  // Clean up any existing WiFi state
  WiFi.disconnect(true);  // Disconnect and clear stored credentials
  WiFi.softAPdisconnect(true);  // Stop any running AP
  delay(100);  // Allow cleanup to complete

  // Check if we have credentials to connect with
  if (strlen(ssid) > 0 && strcmp(ssid, "YOUR_SSID") != 0) {
    Debug.printf("Connecting to WiFi network: %s\n", ssid);

    // Disable Power Saving Mode -- Troubleshooting device disconnects
    WiFi.setSleep(WIFI_PS_NONE);

    // Set WiFi mode to station
    WiFi.mode(WIFI_STA);
    delay(100);
    applyWiFiRoutePrio();

    WiFi.begin(ssid, password);

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
      apMode = false;
    } else if (ethHasIP()) {
      // Reachable over Ethernet, so stay in STA mode and keep retrying WiFi
      // instead of dropping into AP mode.
      Debug.printf("Failed to connect to WiFi (status: %d); Ethernet is up, so not starting AP mode\n", WiFi.status());
      wifiReconnecting = true;
      wifiReconnectStartTime = millis();
    } else {
      Debug.printf("Failed to connect to WiFi (status: %d), starting AP mode\n", WiFi.status());
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

// Handle WiFi connection in main loop (NON-BLOCKING except the AP-timeout retry)
static void handleWiFi() {
  if (apMode) {
    // Check if we should exit AP mode after timeout
    if (millis() - apStartTime > AP_TIMEOUT) {
      Debug.println("AP mode timeout, attempting to reconnect to WiFi");
      // Stop AP before trying to reconnect
      WiFi.softAPdisconnect(true);
      apMode = false;
      delay(100);
      initWiFi();
    }
    return;
  }

  if (strlen(ssid) == 0 || strcmp(ssid, "YOUR_SSID") == 0) {
    return;  // Ethernet-only: nothing to reconnect
  }

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

// ── Status ───────────────────────────────────────────────────────────────────

bool netIsConnected() { return wifiIsConnected() || ethHasIP(); }

static NetInterface defaultInterface() {
  bool wifiUp = wifiIsConnected(), ethUp = ethHasIP();
  if (wifiUp && ethUp) return (NetInterface)netPreferred;
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
    else         Debug.println("Ethernet link DOWN");
    lastEthLink = ethLink;
  }
  if (ethIP != lastEthIP) {
    if (ethIP) Debug.printf("Ethernet got IP %s\n", ETH.localIP().toString().c_str());
    else       Debug.println("Ethernet lost IP");
    lastEthIP = ethIP;
  }
  if (wifiUp != lastWifiUp) {
    if (wifiUp) Debug.printf("WiFi up: %s (%d dBm)\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    else        Debug.println("WiFi down");
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
  Network.setHostname(netHostname);
  // Ethernet first: its DHCP completes while WiFi is connecting, so a failed
  // WiFi join can see that Ethernet is up and skip AP mode.
  initEthernet();
  initWiFi();
  logTransitions();
}

void handleNetwork() {
  handleWiFi();
  logTransitions();
}

void fillNetworkStatus(JsonObject net) {
  net["active"] = netActiveInterfaceName();
  net["ip"] = netLocalIP().toString();
  net["hostname"] = netHostname;
  net["preferred"] = netPreferred == NET_IF_ETH ? "Ethernet" : "WiFi";
  net["ap_mode"] = apMode;

  JsonObject wifi = net.createNestedObject("wifi");
  bool wifiUp = wifiIsConnected();
  wifi["connected"] = wifiUp;
  wifi["ssid"] = wifiUp ? WiFi.SSID() : String("");
  wifi["ip"] = wifiUp ? WiFi.localIP().toString() : String("");
  wifi["rssi"] = wifiUp ? WiFi.RSSI() : 0;
  wifi["mac"] = WiFi.macAddress();

  JsonObject eth = net.createNestedObject("ethernet");
  eth["enabled"] = ethEnabled;
  eth["started"] = ethStarted;
  eth["link"] = ethLinkUp();
  eth["speed"] = ethLinkSpeed();
  eth["full_duplex"] = ethLinkUp() && ETH.fullDuplex();
  eth["ip"] = ethHasIP() ? ETH.localIP().toString() : String("");
  eth["mac"] = ethMacAddress();
}
