/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v4)
 * Network Manager - WiFi (plus AP-mode setup) and optional W5500 Ethernet
 *
 * Network modes:
 *   WiFi only       - no W5500 fitted; Ethernet not started.
 *   Dual            - both up, each with its own IP. The "preferred" interface
 *                     carries the default route (outbound MQTT); if it drops the
 *                     other carries everything.
 *   WiFi primary / Ethernet backup, Ethernet primary / WiFi backup
 *                   - only the primary runs. If it is down for 15 s the backup
 *                     is brought up; once the primary has been back for 60 s the
 *                     backup is shut down again.
 *
 * "Down" means no IP address, or (with the gateway check on) the primary's
 * gateway has stopped answering pings: an interface can hold an IP and still be
 * cut off (dead switch port, wrong VLAN). An unreachable primary also loses the
 * default route, so outbound traffic moves to the other interface.
 *
 * There is no Ethernet-only mode on purpose: WiFi is always the way back in.
 *
 * All servers and UDP listeners bind to every interface, so the web UI, Alpaca,
 * discovery, NTP and the sensor listeners answer on whichever addresses are up.
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <IPAddress.h>
#include <ArduinoJson.h>
#include "../../config.h"

enum NetInterface { NET_IF_WIFI = 0, NET_IF_ETH = 1 };

enum NetMode {
  NET_MODE_WIFI_ONLY = 0,
  NET_MODE_RETIRED_ETH_ONLY = 1,  // stored value maps to NET_MODE_ETH_FAILOVER
  NET_MODE_DUAL = 2,
  NET_MODE_WIFI_FAILOVER = 3,   // WiFi primary, Ethernet backup
  NET_MODE_ETH_FAILOVER = 4,    // Ethernet primary, WiFi backup
  NET_MODE_COUNT
};

// WiFi credentials (loaded/saved with the rest of the config in web_ui_handler.cpp)
extern char ssid[SSID_SIZE];
extern char password[PASSWORD_SIZE];
extern bool apMode;

// Ethernet (W5500) and routing settings
extern int netMode;                      // NetMode (takes effect after restart)
extern bool ethEnabled;                  // Derived: netMode uses the W5500
extern int ethCsPin, ethSckPin, ethMosiPin, ethMisoPin, ethIrqPin, ethRstPin;
extern int netPreferred;                 // NetInterface; Dual mode only
extern bool netGatewayCheck;             // Ping the primary's gateway to detect a dead link
extern char netHostname[NET_HOSTNAME_SIZE];

bool netModeSelectable(int mode);        // Offered on the setup page
const char* netModeKey(int mode);        // "wifi", "dual", "wifi_eth", "eth_wifi"
const char* netModeLabel(int mode);      // Human-readable
int netModeFromKey(const String& key);   // -1 if unknown

void loadNetworkConfiguration();
void saveNetworkConfiguration();

void initNetwork();                      // Call once from setup(), after config is loaded
void handleNetwork();                    // Call every loop()

bool netIsConnected();                   // Any interface has an IP (AP mode doesn't count)
IPAddress netLocalIP();                  // IP of the interface carrying the default route (AP IP in AP mode)
const char* netActiveInterfaceName();    // "WiFi", "Ethernet", "AP" or "None"

bool ethIsStarted();                     // W5500 driver running
bool ethHasIP();
bool ethLinkUp();
IPAddress ethLocalIP();
String ethMacAddress();
uint16_t ethLinkSpeed();
bool wifiIsConnected();
String wifiMacAddress();                 // Factory STA MAC, valid even with the radio off

// Validates W5500 pins against each other and the GPS pins; returns "" if OK
String ethPinConflict(int cs, int sck, int mosi, int miso, int irq, int rst);

void fillNetworkStatus(JsonObject net);  // Per-interface status for /api/status and MQTT

#endif // NETWORK_MANAGER_H
