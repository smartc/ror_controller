/*
 * ESP32 ASCOM Alpaca Roll-Off Roof Controller
 * MQTT Handler
 */

#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include "config.h"
#include "PubSubClient.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ESP.h>

// MQTT Configuration
extern char mqttServer[MQTT_SERVER_SIZE];
extern int mqttPort;
extern char mqttUser[MQTT_USER_SIZE];
extern char mqttPassword[MQTT_PASSWORD_SIZE];
extern char mqttClientId[MQTT_CLIENTID_SIZE];
extern char mqttTopicPrefix[MQTT_TOPIC_SIZE];
extern char mqttTopicStatus[MQTT_TOPIC_SIZE];
extern char mqttTopicCommand[MQTT_TOPIC_SIZE];
extern char mqttTopicAvailability[MQTT_TOPIC_SIZE];

// External references
extern WiFiClient espClient;
extern PubSubClient mqttClient;
extern RoofStatus roofStatus;
extern bool slaved;
extern String uniqueID;
extern bool mqttEnabled;
extern bool telescopeParked;
extern bool bypassParkSensor;

// Function prototypes
void setupMQTT();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishStatusToMQTT();
void publishDiscovery();
void forceDiscovery();
String getRoofStatusString();
String getRoofStatusString(RoofStatus status); // Overloaded version

#endif // MQTT_HANDLER_H