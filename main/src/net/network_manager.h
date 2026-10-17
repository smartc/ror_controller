/*
 * ESP32-S3 ASCOM Alpaca Roll-Off Roof Controller (v4)
 * Network Manager - WiFi (primary, plus AP-mode setup) and optional W5500 Ethernet
 *
 * Both interfaces can be up at once, each with its own IP. All servers and UDP
 * listeners bind to every interface, so the web UI, Alpaca, discovery, NTP and
 * the sensor listeners answer on either address. The "preferred" interface only
 * decides the default route, i.e. which one outbound traffic (MQTT) uses while
 * both are up. If one interface drops, the other carries everything.
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <IPAddress.h>
#include <ArduinoJson.h>
#include "../../config.h"

enum NetInterface { NET_IF_WIFI = 0, NET_IF_ETH = 1 };

// WiFi credentials (loaded/saved with the rest of the config in web_ui_handler.cpp)
extern char ssid[SSID_SIZE];
extern char password[PASSWORD_SIZE];
extern bool apMode;

// Ethernet (W5500) and routing settings
extern bool ethEnabled;
extern int ethCsPin, ethSckPin, ethMosiPin, ethMisoPin, ethIrqPin, ethRstPin;
extern int netPreferred;                 // NetInterface
extern char netHostname[NET_HOSTNAME_SIZE];

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

// Validates W5500 pins against each other and the GPS pins; returns "" if OK
String ethPinConflict(int cs, int sck, int mosi, int miso, int irq, int rst);

void fillNetworkStatus(JsonObject net);  // Per-interface status for /api/status and MQTT

#endif // NETWORK_MANAGER_H
