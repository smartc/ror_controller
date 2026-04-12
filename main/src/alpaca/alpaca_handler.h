/*
 * ESP32 ASCOM Alpaca Roll-Off Roof Controller
 * Alpaca API Handler
 */

#ifndef ALPACA_HANDLER_H
#define ALPACA_HANDLER_H

#include <WebServer.h>
#include <WiFiUdp.h>
#include "config.h"

// External server instance
extern WebServer alpacaServer;
extern WiFiUDP udp;
extern String uniqueID;
extern unsigned int serverTransactionID;
extern bool bypassParkSensor;  // Add bypass park sensor reference

// Function prototypes
void setupAlpacaAPI();
void handleAlpacaDiscovery();
void setupAlpacaRoutes();
void sendAlpacaResponse(int clientID, int clientTransactionID, int errorNumber, String errorMessage, String value = "");

// Management API handlers
void handleApiVersions();
void handleDescription();
void handleConfiguredDevices();

// Setup web interface handlers
void handleSetup();
void handleDomeSetup();
void handleRedirectSetup();

// ASCOM Alpaca Common handlers
void handleConnected();
void handleSetConnected();
void handleDeviceDescription();
void handleDriverInfo();
void handleDriverVersion();
void handleInterfaceVersion();
void handleName();
void handleSupportedActions();
void handleAction();

// Handle methods that are not implemented
void handleNotFound();
void handleNotImplemented();

// Dome specific handlers
void handleAltitude();
void handleAtHome();
void handleAtPark();
void handleAzimuth();
void handleCanFindHome();
void handleCanPark();
void handleCanSetAltitude();
void handleCanSetAzimuth();
void handleCanSetPark();
void handleCanSetShutter();
void handleCanSlave();
void handleCanSyncAzimuth();
void handleShutterStatus();
void handleSlaved();
void handleSetSlaved();
void handleSlewing();

// Roll-off roof specific commands
void handleOpenShutter();
void handleCloseShutter();
void handleAbortSlew();

#endif // ALPACA_HANDLER_H