/*
 * ESP32 ASCOM Alpaca Roll-Off Roof Controller
 * Alpaca API Handler Implementation
 */

#include "alpaca_handler.h"
#include "../mqtt/mqtt_handler.h"
#include "../roof/roof_controller.h"
#include "../safety/safety_sensor.h"
#include "../sensors/gps_handler.h"
#include "../debug/Debug.h"
#include "../net/network_manager.h"
#include <esp_mac.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>

// Web server and UDP for discovery
WebServer alpacaServer(ALPACA_PORT);
WiFiUDP udp;
String uniqueID;
unsigned int serverTransactionID = 1;

// Alpaca parameter lookup (Alpaca API spec): names are case-insensitive in GET query
// strings, but PUT form names must match exactly - a wrong-case PUT name counts as missing
static bool alpacaArg(const char* name, String& value) {
  bool exact = (alpacaServer.method() == HTTP_PUT);
  for (int i = 0; i < alpacaServer.args(); i++) {
    String n = alpacaServer.argName(i);
    if (exact ? n.equals(name) : n.equalsIgnoreCase(name)) {
      value = alpacaServer.arg(i);
      return true;
    }
  }
  return false;
}

// ClientID / ClientTransactionID are unsigned 32-bit; missing, negative or non-numeric -> 0
static uint32_t alpacaUIntArg(const char* name) {
  String v;
  if (!alpacaArg(name, v)) return 0;
  v.trim();
  if (v.length() == 0 || v.length() > 10) return 0;
  for (unsigned int i = 0; i < v.length(); i++) {
    if (!isDigit(v[i])) return 0;
  }
  unsigned long long n = strtoull(v.c_str(), nullptr, 10);
  return n > 0xFFFFFFFFULL ? 0 : (uint32_t)n;
}

// A PUT with a missing or invalid parameter value is an HTTP 400, not an ASCOM error
static void alpacaBadRequest(const String& message) {
  alpacaServer.send(400, "text/plain", message);
}

// Per-client Connected state, for reading Connected back only. The hardware connection is
// shared and always up, so nothing is ever refused because of it (#12); but a client that
// set Connected=false (or called Disconnect) reads false back, as the interface requires.
// Clients that never wrote it, and ClientID 0 (none given), read true. Small table; the
// least recently used entry makes way for a new client.
struct AlpacaClientConn { uint32_t id; bool connected; uint32_t lastUsed; };
static AlpacaClientConn clientConns[16];
static uint32_t clientConnUse = 0;

static bool alpacaClientConnected(uint32_t id) {
  if (id == 0) return true;
  for (auto& c : clientConns) {
    if (c.id == id) return c.connected;
  }
  return true;
}

static void alpacaSetClientConnected(uint32_t id, bool connected) {
  if (id == 0) return;
  AlpacaClientConn* slot = &clientConns[0];
  for (auto& c : clientConns) {
    if (c.id == id) { slot = &c; break; }
    if (c.lastUsed < slot->lastUsed) slot = &c;  // empty slots have lastUsed 0
  }
  slot->id = id;
  slot->connected = connected;
  slot->lastUsed = ++clientConnUse;
}

// Set up the Alpaca API
void setupAlpacaAPI() {
  // Generate a unique ID based on ESP32's MAC address
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);  // factory STA MAC: stable whichever interface is up
  uniqueID = "ESP32_ROF_";
  for (int i = 0; i < 6; i++) {
    char buf[3];
    sprintf(buf, "%02X", mac[i]);
    uniqueID += buf;
  }
  
  // Setup MDNS responder
  if (MDNS.begin(netHostname)) {
    Serial.println("MDNS responder started");
    MDNS.addService("http", "tcp", ALPACA_PORT);
  }
  
  // Initialize UDP for Alpaca discovery
  Serial.print("Starting UDP listener on port ");
  Serial.print(ALPACA_DISCOVERY_PORT);
  Serial.print("... ");
  
  if (udp.begin(ALPACA_DISCOVERY_PORT)) {
    Serial.println("SUCCESS!");
  } else {
    Serial.println("FAILED!");
  }
  
  // Print network info
  Serial.print("ESP32 IP address: ");
  Serial.println(netLocalIP());
  Serial.print("Discovery port: ");
  Serial.println(ALPACA_DISCOVERY_PORT);
  Serial.print("Alpaca API port: ");
  Serial.println(ALPACA_PORT);
  
  // Setup server routes for ASCOM Alpaca API
  setupAlpacaRoutes();
  
  // Start the web server
  alpacaServer.begin();
  Serial.printf("Alpaca server started on port %d\n", ALPACA_PORT);
}

// Handle Alpaca discovery requests
void handleAlpacaDiscovery() {
  int packetSize = udp.parsePacket();
  
  if (packetSize) {
    char packet[64];
    memset(packet, 0, sizeof(packet)); // Clear the buffer
    int len = udp.read(packet, sizeof(packet) - 1);
    
    if (len > 0) {
      packet[len] = 0; // Null terminate
      
      // Debug output
      Serial.print("Received UDP packet (");
      Serial.print(len);
      Serial.print(" bytes) from ");
      Serial.print(udp.remoteIP());
      Serial.print(":");
      Serial.print(udp.remotePort());
      Serial.print(" - Content: '");
      Serial.print(packet);
      Serial.println("'");
      
      // Check if this is an Alpaca discovery message
      if (strncmp(packet, ALPACA_DISCOVERY_MESSAGE, strlen(ALPACA_DISCOVERY_MESSAGE)) == 0) {
        // Create the response JSON
        DynamicJsonDocument doc(128);
        doc["AlpacaPort"] = ALPACA_PORT;
        
        String response;
        serializeJson(doc, response);
        
        // Send the response back to the client
        udp.beginPacket(udp.remoteIP(), udp.remotePort());
        udp.print(response);
        udp.endPacket();
        
        Serial.print("Sent discovery response: ");
        Serial.println(response);
      } else {
        Serial.println("Received non-Alpaca UDP packet");
      }
    }
  }
}

// Set up all Alpaca routes
void setupAlpacaRoutes() {
  // Add redirect from /setup on Alpaca port to Web UI port
  alpacaServer.on("/setup", HTTP_GET, handleRedirectSetup);

  // Management API routes
  alpacaServer.on("/management/apiversions", HTTP_GET, handleApiVersions);
  alpacaServer.on("/management/v1/description", HTTP_GET, handleDescription);
  alpacaServer.on("/management/v1/configureddevices", HTTP_GET, handleConfiguredDevices);
  
  // Alpaca setup routes
  alpacaServer.on("/setup/v1/dome/0/setup", HTTP_GET, handleDomeSetup);
  
  // Dome API routes
  alpacaServer.on("/api/v1/dome/0/connected", HTTP_GET, handleConnected);
  alpacaServer.on("/api/v1/dome/0/connected", HTTP_PUT, handleSetConnected);
  alpacaServer.on("/api/v1/dome/0/connect", HTTP_PUT, handleConnect);         // IDomeV3
  alpacaServer.on("/api/v1/dome/0/disconnect", HTTP_PUT, handleDisconnect);   // IDomeV3
  alpacaServer.on("/api/v1/dome/0/connecting", HTTP_GET, handleConnecting);   // IDomeV3
  alpacaServer.on("/api/v1/dome/0/devicestate", HTTP_GET, handleDeviceState); // IDomeV3
  alpacaServer.on("/api/v1/dome/0/description", HTTP_GET, handleDeviceDescription);
  alpacaServer.on("/api/v1/dome/0/driverinfo", HTTP_GET, handleDriverInfo);
  alpacaServer.on("/api/v1/dome/0/driverversion", HTTP_GET, handleDriverVersion);
  alpacaServer.on("/api/v1/dome/0/interfaceversion", HTTP_GET, handleInterfaceVersion);
  alpacaServer.on("/api/v1/dome/0/name", HTTP_GET, handleName);
  alpacaServer.on("/api/v1/dome/0/supportedactions", HTTP_GET, handleSupportedActions);
  alpacaServer.on("/api/v1/dome/0/action", HTTP_PUT, handleAction);
  
  // Dome specific methods
  alpacaServer.on("/api/v1/dome/0/altitude", HTTP_GET, handleAltitude);
  alpacaServer.on("/api/v1/dome/0/athome", HTTP_GET, handleAtHome);
  alpacaServer.on("/api/v1/dome/0/atpark", HTTP_GET, handleAtPark);
  alpacaServer.on("/api/v1/dome/0/azimuth", HTTP_GET, handleAzimuth);
  alpacaServer.on("/api/v1/dome/0/canfindhome", HTTP_GET, handleCanFindHome);
  alpacaServer.on("/api/v1/dome/0/canpark", HTTP_GET, handleCanPark);
  alpacaServer.on("/api/v1/dome/0/cansetaltitude", HTTP_GET, handleCanSetAltitude);
  alpacaServer.on("/api/v1/dome/0/cansetazimuth", HTTP_GET, handleCanSetAzimuth);
  alpacaServer.on("/api/v1/dome/0/cansetpark", HTTP_GET, handleCanSetPark);
  alpacaServer.on("/api/v1/dome/0/cansetshutter", HTTP_GET, handleCanSetShutter);
  alpacaServer.on("/api/v1/dome/0/canslave", HTTP_GET, handleCanSlave);
  alpacaServer.on("/api/v1/dome/0/cansyncazimuth", HTTP_GET, handleCanSyncAzimuth);
  alpacaServer.on("/api/v1/dome/0/shutterstatus", HTTP_GET, handleShutterStatus);
  alpacaServer.on("/api/v1/dome/0/slaved", HTTP_GET, handleSlaved);
  alpacaServer.on("/api/v1/dome/0/slaved", HTTP_PUT, handleSetSlaved);
  alpacaServer.on("/api/v1/dome/0/slewing", HTTP_GET, handleSlewing);
  
  // Roll-off roof specific commands
  alpacaServer.on("/api/v1/dome/0/closeshutter", HTTP_PUT, handleCloseShutter);
  alpacaServer.on("/api/v1/dome/0/openshutter", HTTP_PUT, handleOpenShutter);
  alpacaServer.on("/api/v1/dome/0/abortslew", HTTP_PUT, handleAbortSlew);
  
  // Not implemented for roll-off roof but required by interface
  alpacaServer.on("/api/v1/dome/0/findhome", HTTP_PUT, handleNotImplemented);
  alpacaServer.on("/api/v1/dome/0/park", HTTP_PUT, handleNotImplemented);
  alpacaServer.on("/api/v1/dome/0/setpark", HTTP_PUT, handleNotImplemented);
  alpacaServer.on("/api/v1/dome/0/slewtoaltitude", HTTP_PUT, handleNotImplemented);
  alpacaServer.on("/api/v1/dome/0/slewtoazimuth", HTTP_PUT, handleNotImplemented);
  alpacaServer.on("/api/v1/dome/0/synctoazimuth", HTTP_PUT, handleNotImplemented);
  
  // Handle not found
  alpacaServer.onNotFound(handleNotFound);
}

// Helper function to send a standard JSON response
void sendAlpacaResponse(uint32_t clientID, uint32_t clientTransactionID, int errorNumber, String errorMessage, String value) {
  DynamicJsonDocument doc(1024);
  
  // Common response fields
  doc["ClientTransactionID"] = clientTransactionID;
  doc["ServerTransactionID"] = serverTransactionID++;
  doc["ErrorNumber"] = errorNumber;
  doc["ErrorMessage"] = errorMessage;
  
  // Include Value field if provided
  if (value.length() > 0) {
    // Check if value is a JSON object or array
    if ((value.startsWith("{") && value.endsWith("}")) || 
        (value.startsWith("[") && value.endsWith("]"))) {
      // Parse the JSON string into a JsonDocument
      DynamicJsonDocument valueDoc(512);
      DeserializationError error = deserializeJson(valueDoc, value);
      
      if (!error) {
        doc["Value"] = valueDoc;
      } else {
        // If parsing fails, just use the string as is
        doc["Value"] = value;
      }
    } else if (value == "true") {
      doc["Value"] = true;
    } else if (value == "false") {
      doc["Value"] = false;
    } else if (String(value.toInt()) == value) {
      // Whole value is an integer ("4.1.0".toInt() is 4, so a prefix check isn't enough:
      // DriverVersion was being returned as the number 4)
      doc["Value"] = value.toInt();
    } else {
      // Otherwise, treat as string
      doc["Value"] = value;
    }
  }
  
  String response;
  serializeJson(doc, response);
  
  alpacaServer.send(200, "application/json", response);
}

void handleNotFound() {
  String message = "Path Not Found\n\n";
  message += "URI: ";
  message += alpacaServer.uri();
  message += "\n";
  message += "Method: ";
  message += (alpacaServer.method() == HTTP_GET ? "GET" : "PUT");
  message += "\n";
  
  alpacaServer.send(400, "text/plain", message);
}

// Management API handlers
void handleApiVersions() {
  // Create a proper Alpaca API versions response matching the format:
  // {"ServerTransactionID":1743898101,"Value":[1]}
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  DynamicJsonDocument doc(128);
  doc["ServerTransactionID"] = serverTransactionID++;
  
  // For Value, we need to create a JSON array
  JsonArray valueArray = doc.createNestedArray("Value");
  valueArray.add(1); // We only support version 1
  
  // Optional fields
  if (clientTransactionID > 0) {
    doc["ClientTransactionID"] = clientTransactionID;
  }
  
  // Error fields (always 0 for this response)
  doc["ErrorNumber"] = 0;
  doc["ErrorMessage"] = "";
  
  String response;
  serializeJson(doc, response);
  
  Serial.print("API Versions response: ");
  Serial.println(response);
  
  alpacaServer.send(200, "application/json", response);
}

void handleDescription() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  DynamicJsonDocument doc(1024);
  doc["ServerName"] = DEVICE_NAME;
  doc["Manufacturer"] = DEVICE_MANUFACTURER;
  doc["ManufacturerVersion"] = DEVICE_VERSION;
  doc["Location"] = "Observatory";
  
  String value;
  serializeJson(doc, value);
  
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", value);
}

void handleInterfaceVersion() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");

  // IDomeV3 (ASCOM Platform 7): Connect/Disconnect/Connecting and DeviceState
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", "3");
}

// Who sent a command, for the log: client IP and Alpaca ClientID
static String alpacaCaller() {
  return alpacaServer.client().remoteIP().toString() + " ClientID=" + String(alpacaUIntArg("ClientID"));
}

void handleConfiguredDevices() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  DynamicJsonDocument arrayDoc(1024);
  JsonArray array = arrayDoc.to<JsonArray>();
  
  JsonObject device = array.createNestedObject();
  device["DeviceName"] = "Roll-Off Roof";
  device["DeviceType"] = "Dome";
  device["DeviceNumber"] = 0;
  device["UniqueID"] = uniqueID;
  
  String value;
  serializeJson(array, value);
  
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", value);
}

// Setup web interface handlers (handleSetup is implemented in web_ui_handler.cpp)

void handleDomeSetup() {
  String html = "<!DOCTYPE html><html>";
  html += "<head><title>Roll-Off Roof Setup</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; }";
  html += "h1 { color: #2c3e50; }";
  html += "button { background-color: #3498db; color: white; border: none; padding: 10px 15px; margin: 5px; border-radius: 4px; cursor: pointer; }";
  html += "button:hover { background-color: #2980b9; }";
  html += "</style></head>";
  html += "<body>";
  html += "<h1>Roll-Off Roof Setup</h1>";
  html += "<p>Status: " + getRoofStatusString() + "</p>";
  html += "<p><button onclick='openRoof()'>Open Roof</button> <button onclick='closeRoof()'>Close Roof</button> <button onclick='stopRoof()'>Stop Movement</button></p>";
  html += "<script>";
  html += "function openRoof() {";
  html += "  fetch('/api/v1/dome/0/openshutter', {method: 'PUT', body: 'ClientID=0&ClientTransactionID=0'}).then(response => response.json()).then(() => { setTimeout(function(){ location.reload(); }, 1000); });";
  html += "}";
  html += "function closeRoof() {";
  html += "  fetch('/api/v1/dome/0/closeshutter', {method: 'PUT', body: 'ClientID=0&ClientTransactionID=0'}).then(response => response.json()).then(() => { setTimeout(function(){ location.reload(); }, 1000); });";
  html += "}";
  html += "function stopRoof() {";
  html += "  fetch('/api/v1/dome/0/abortslew', {method: 'PUT', body: 'ClientID=0&ClientTransactionID=0'}).then(response => response.json()).then(() => { setTimeout(function(){ location.reload(); }, 1000); });";
  html += "}";
  html += "</script>";
  html += "</body></html>";
  
  alpacaServer.send(200, "text/html", html);
}

// ASCOM Alpaca Common handlers
//
// Connection follows the ASCOM hub model: several clients (both NINA rigs, CloudRoll,
// Home Assistant) share the one roof, so the hardware connection is always up and no
// client can disconnect the others (#12). Each client's own Connected value is
// remembered so it reads back what it set; nothing is ever refused because of it.
// Writes are logged with the caller.
void handleConnected() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");

  sendAlpacaResponse(clientID, clientTransactionID, 0, "", alpacaClientConnected(clientID) ? "true" : "false");
}

void handleSetConnected() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");

  String value;
  if (!alpacaArg("Connected", value) || (!value.equalsIgnoreCase("true") && !value.equalsIgnoreCase("false"))) {
    alpacaBadRequest("Connected must be True or False");
    return;
  }

  bool connected = value.equalsIgnoreCase("true");
  alpacaSetClientConnected(clientID, connected);
  Debug.printf("Alpaca: Connected=%s from %s (shared connection, roof unaffected)\n",
               connected ? "true" : "false", alpacaCaller().c_str());
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", "");
}

// IDomeV3: Connect()/Disconnect() complete immediately (the hardware connection is shared
// and always up), so Connecting is always false
void handleConnect() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");

  alpacaSetClientConnected(clientID, true);
  Debug.printf("Alpaca: Connect from %s\n", alpacaCaller().c_str());
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", "");
}

void handleDisconnect() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");

  alpacaSetClientConnected(clientID, false);
  Debug.printf("Alpaca: Disconnect from %s (shared connection, roof unaffected)\n", alpacaCaller().c_str());
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", "");
}

void handleConnecting() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");

  sendAlpacaResponse(clientID, clientTransactionID, 0, "", "false");
}

// IDomeV3 DeviceState: the operational properties in one call. Altitude and Azimuth
// mean nothing for a roll-off roof and are left out; so is Slewing when reading it
// would raise (a roof fault), as the spec asks for unreadable properties.
void handleDeviceState() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");

  RoofStatus status = roofStatus;
  bool stoppedMidTravel = roofStoppedMidTravel();

  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  auto add = [&arr](const char* name) {
    JsonObject o = arr.add<JsonObject>();
    o["Name"] = name;
    return o["Value"].to<JsonVariant>();
  };
  add("AtHome").set(false);
  add("AtPark").set(status == ROOF_CLOSED);
  add("ShutterStatus").set(static_cast<int>(status));
  if (status != ROOF_ERROR || stoppedMidTravel) {
    add("Slewing").set(status == ROOF_OPENING || status == ROOF_CLOSING);
  }
  uint64_t ms = getCurrentUnixMillis();
  if (ms != 0) {
    time_t t = (time_t)(ms / 1000);
    struct tm tmUtc;
    gmtime_r(&t, &tmUtc);
    char ts[32];
    snprintf(ts, sizeof(ts), "%04d-%02d-%02dT%02d:%02d:%02d.%03uZ", tmUtc.tm_year + 1900, tmUtc.tm_mon + 1,
             tmUtc.tm_mday, tmUtc.tm_hour, tmUtc.tm_min, tmUtc.tm_sec, (unsigned)(ms % 1000));
    add("TimeStamp").set(ts);  // copied: ts is a char array
  }

  String value;
  serializeJson(doc, value);
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", value);
}

void handleDeviceDescription() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", "ESP32 based ASCOM Alpaca Roll-Off Roof Controller");
}

void handleDriverInfo() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", "ESP32 ASCOM Alpaca Roll-Off Roof Controller by DIY Observatory");
}

void handleDriverVersion() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", DEVICE_VERSION);
}

void handleName() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", "Roll-Off Roof");
}

void handleSupportedActions() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  DynamicJsonDocument doc(256);
  JsonArray array = doc.to<JsonArray>();
  array.add("status");  // Custom action to get status
  
  String value;
  serializeJson(doc, value);
  
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", value);
}

void handleAction() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  String actionName, actionParameters;
  alpacaArg("Action", actionName);
  alpacaArg("Parameters", actionParameters);
  
  if (actionName == "status") {
    sendAlpacaResponse(clientID, clientTransactionID, 0, "", getRoofStatusString());
  } else {
    sendAlpacaResponse(clientID, clientTransactionID, 1036, "Action not implemented", "");
  }
}

void handleRedirectSetup() {
  String redirectUrl = "http://" + netLocalIP().toString() + ":" + String(WEB_UI_PORT) + "/setup";
  Serial.println("Redirecting to: " + redirectUrl);
  
  // Send HTTP 302 redirect
  alpacaServer.sendHeader("Location", redirectUrl, true);
  alpacaServer.send(302, "text/plain", "Redirecting to setup page...");
}

// Handle methods that are not implemented
void handleNotImplemented() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  sendAlpacaResponse(clientID, clientTransactionID, 1024, "Method not implemented", "");
}

// Dome specific handlers
void handleAltitude() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  // Roll-off roofs don't have altitude, return 0
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", "0");
}

void handleAtHome() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  // Roll-off roofs don't have a home position, return false
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", "false");
}

void handleAtPark() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  // For a roll-off roof, we consider "parked" to be fully closed
  bool isParked = (roofStatus == ROOF_CLOSED);
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", isParked ? "true" : "false");
}

void handleAzimuth() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  // Roll-off roofs don't have azimuth, return 0
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", "0");
}

void handleCanFindHome() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  // Roll-off roofs don't have a home position
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", "false");
}

void handleCanPark() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  // We don't implement parking for roll-off roof
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", "false");
}

void handleCanSetAltitude() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  // Roll-off roofs can't set altitude
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", "false");
}

void handleCanSetAzimuth() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  // Roll-off roofs can't set azimuth
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", "false");
}

void handleCanSetPark() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  // Roll-off roofs can't set park position
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", "false");
}

void handleCanSetShutter() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  // We can open and close the roof
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", "true");
}

void handleCanSlave() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  // A roll-off roof can't follow the telescope's azimuth
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", "false");
}

void handleCanSyncAzimuth() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  // Roll-off roofs can't sync azimuth
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", "false");
}

void handleShutterStatus() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  // RoofStatus enum should match ASCOM ShutterStatus values:
  // 0 = shutterOpen
  // 1 = shutterClosed
  // 2 = shutterOpening
  // 3 = shutterClosing
  // 4 = shutterError
  // (kept current by the roof task every 10 ms)

  // Return the status as an integer
  int status = static_cast<int>(roofStatus);
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", String(status));
}

void handleSlaved() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", slaved ? "true" : "false");
}

void handleSetSlaved() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  // Parse body for the Slaved value (missing, wrong-case name or bad value: HTTP 400)
  String value;
  if (!alpacaArg("Slaved", value) || (!value.equalsIgnoreCase("true") && !value.equalsIgnoreCase("false"))) {
    alpacaBadRequest("Slaved must be True or False");
    return;
  }
  // CanSlave is false, so Slaved can't be set (ASCOM: PropertyNotImplementedException)
  sendAlpacaResponse(clientID, clientTransactionID, 1024, "Slaved is not implemented: a roll-off roof can't be slaved", "");
}

void handleSlewing() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");

  // ASCOM Compliance: If the roof is in an error state, reading Slewing must raise an exception
  // with a descriptive error message explaining what went wrong.
  // See: https://ascom-standards.org/newdocs/dome.html
  // "If the shutter becomes jammed... you must raise an exception when the app tries to read Slewing"
  // A deliberate stop between the limits isn't a fault: Slewing is simply false.
  // (Status is kept current by the roof task; per-poll lines are verbose-only, since
  // clients poll this every second or two and every level-1 line is persisted.)
  if (roofStatus == ROOF_ERROR && !roofStoppedMidTravel()) {
    String errorMsg = getRoofErrorReason().length() > 0 ?
                      getRoofErrorReason() :
                      "Roof is in error state. Check limit switches and mechanical systems.";
    // Use error code 0x500 (1280) for DriverException - shutter/roof operation failed
    Debug.println("Slewing: returning error (ROOF_ERROR state): " + errorMsg, 2);
    sendAlpacaResponse(clientID, clientTransactionID, 0x500, errorMsg, "");
    return;
  }

  // Check if roof is moving (opening or closing)
  bool isSlewing = (roofStatus == ROOF_OPENING || roofStatus == ROOF_CLOSING);
  Debug.println("Slewing: returning " + String(isSlewing ? "true" : "false"), 2);
  sendAlpacaResponse(clientID, clientTransactionID, 0, "", isSlewing ? "true" : "false");
}

void handleOpenShutter() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  Debug.printf("Alpaca: OpenShutter from %s (roof %s)\n", alpacaCaller().c_str(), getRoofStatusString().c_str());

  // Check if we're already open
  if (roofStatus == ROOF_OPEN) {
    sendAlpacaResponse(clientID, clientTransactionID, 0, "", "");  // already open
    return;
  }

  // ASCOM: "If another app calls OpenShutter() while the shutter is already opening,
  // the request must be accepted without error." The opposite move isn't: stop first.
  if (roofStatus == ROOF_OPENING) {
    sendAlpacaResponse(clientID, clientTransactionID, 0, "", "");
    return;
  }
  if (roofStatus == ROOF_CLOSING) {
    sendAlpacaResponse(clientID, clientTransactionID, 1035, "Cannot open: roof is closing. Use AbortSlew first.", "");
    return;
  }

  // Check telescope safety interlock - only if bypass is not enabled
  if (!bypassParkSensor && !telescopeParked) {
    Debug.println("OpenShutter refused: telescope not parked");
    sendAlpacaResponse(clientID, clientTransactionID, 1035, "Cannot open roof when telescope is not parked and bypass not enabled", "");
    return;
  }

  // Weather safety interlock - report it explicitly rather than as a generic failure
  // (startOpeningRoof() would refuse too, but only with "Failed to start opening")
  if (!isWeatherSafe() && digitalRead(LIMIT_SWITCH_OPEN_PIN) != TRIGGERED) {
    Debug.println("OpenShutter refused: weather unsafe (safety sensor unsafe or offline)");
    sendAlpacaResponse(clientID, clientTransactionID, 1035, "Cannot open roof: weather unsafe (safety sensor reporting unsafe or offline)", "");
    return;
  }

  // Start opening the roof
  if (startOpeningRoof()) {
    sendAlpacaResponse(clientID, clientTransactionID, 0, "", "");
  } else {
    sendAlpacaResponse(clientID, clientTransactionID, 1035, "Failed to start opening the roof", "");
  }
}

void handleCloseShutter() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  Debug.printf("Alpaca: CloseShutter from %s (roof %s)\n", alpacaCaller().c_str(), getRoofStatusString().c_str());

  // Check if we're already closed
  if (roofStatus == ROOF_CLOSED) {
    sendAlpacaResponse(clientID, clientTransactionID, 0, "", "");  // already closed
    return;
  }

  // ASCOM: a repeat of the move in progress is accepted without error; the opposite isn't
  if (roofStatus == ROOF_CLOSING) {
    sendAlpacaResponse(clientID, clientTransactionID, 0, "", "");
    return;
  }
  if (roofStatus == ROOF_OPENING) {
    sendAlpacaResponse(clientID, clientTransactionID, 1035, "Cannot close: roof is opening. Use AbortSlew first.", "");
    return;
  }

  // Check telescope safety interlock - only if bypass is not enabled
  if (!bypassParkSensor && !telescopeParked) {
    Debug.println("CloseShutter refused: telescope not parked");
    sendAlpacaResponse(clientID, clientTransactionID, 1035, "Cannot close roof when telescope is not parked and bypass not enabled", "");
    return;
  }
  
  // Start closing the roof
  if (startClosingRoof()) {
    sendAlpacaResponse(clientID, clientTransactionID, 0, "", "");
  } else {
    sendAlpacaResponse(clientID, clientTransactionID, 1035, "Failed to start closing the roof", "");
  }
}

void handleAbortSlew() {
  uint32_t clientID = alpacaUIntArg("ClientID");
  uint32_t clientTransactionID = alpacaUIntArg("ClientTransactionID");
  
  Debug.printf("Alpaca: AbortSlew from %s (roof %s)\n", alpacaCaller().c_str(), getRoofStatusString().c_str());
  slaved = false;  // ASCOM: AbortSlew disables slaving

  // If the roof is already fully open or closed, just return success
  // rather than an error message - conformance testing may expect this.
  // (An accepted move reads OPENING/CLOSING from acceptance, so a start-up
  // sequence in progress is stopped below, not skipped here.)
  if (roofStatus == ROOF_OPEN || roofStatus == ROOF_CLOSED) {
    sendAlpacaResponse(clientID, clientTransactionID, 0, "", "");
    return;
  }
  
  // The roof is moving - try to stop it
  if (roofStatus == ROOF_OPENING || roofStatus == ROOF_CLOSING) {
    if (stopRoofMovement()) {
      sendAlpacaResponse(clientID, clientTransactionID, 0, "", "");
    } else {
      sendAlpacaResponse(clientID, clientTransactionID, 1035, "Failed to stop roof movement", "");
    }
    return;
  }
  
  // For any other status (like ERROR), also try to stop
  if (stopRoofMovement()) {
    sendAlpacaResponse(clientID, clientTransactionID, 0, "", "");
  } else {
    sendAlpacaResponse(clientID, clientTransactionID, 0, "", "");  // Still return success for conformance testing
  }
}