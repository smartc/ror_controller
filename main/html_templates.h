/*
 * ESP32 ASCOM Alpaca Roll-Off Roof Controller
 * HTML Templates
 */

#ifndef HTML_TEMPLATES_H
#define HTML_TEMPLATES_H

#include <Arduino.h>
#include "config.h"
#include "mqtt_handler.h"
#include "roof_controller.h"
#include "park_sensor_udp.h"
#include <WiFi.h>

// Forward declarations of template components
String getPageHeader(String pageTitle);
String getNavBar();
String getCommonStyles();
String getToggleSwitchStyles();
String getStatusDisplay(RoofStatus status);
String getControlJS();
String getHomePage(RoofStatus status, bool isApMode);
String getSetupPage();
String getRoofControlPage();
String getWifiConfigPage();
String getWifiSettingsCard();
String getMqttSettingsCard();
String getSwitchConfigCard();
String getSystemManagementCard();
String getStatusCard();
String getParkSensorConfigCard();  // New function for park sensor configuration

// Common CSS styles used across pages - Dark Theme
inline String getCommonStyles() {
  String styles =
    "body { font-family: Arial, sans-serif; margin: 20px; background-color: #1a1a1a; color: #e0e0e0; }\n"
    "h1, h2 { color: #4fc3f7; margin-top: 0; }\n"
    "h3 { color: #81c784; }\n"
    "a { color: #4fc3f7; text-decoration: none; }\n"
    "a:hover { text-decoration: underline; color: #81d4fa; }\n"
    ".card { background: #2d2d2d; border-radius: 8px; padding: 20px; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); border: 1px solid #404040; }\n"
    "label { display: block; margin-bottom: 5px; font-weight: bold; color: #b0b0b0; }\n"
    "input[type=text], input[type=password], input[type=number] { width: 100%; padding: 8px; margin-bottom: 15px; border: 1px solid #555; border-radius: 4px; background-color: #333; color: #e0e0e0; }\n"
    "input[type=submit] { background: #4fc3f7; color: #000; border: none; padding: 10px 15px; border-radius: 4px; cursor: pointer; font-weight: bold; }\n"
    "input[type=submit]:hover { background: #81d4fa; }\n"
    "button { background-color: #4fc3f7; color: #000; border: none; padding: 10px 15px; margin: 5px; border-radius: 4px; cursor: pointer; font-weight: bold; }\n"
    "button:hover { background-color: #81d4fa; }\n"
    "button:disabled { background-color: #555; color: #888; cursor: not-allowed; opacity: 0.5; }\n"
    "table { border-collapse: collapse; width: 100%; }\n"
    "table, th, td { border: 1px solid #555; }\n"
    "th, td { padding: 8px; text-align: left; }\n"
    "th { background-color: #333; color: #4fc3f7; font-weight: bold; }\n"
    "td { background-color: #2d2d2d; }\n"
    ".status-open { color: #64b5f6; font-weight: bold; }\n"
    ".status-closed { color: #81c784; font-weight: bold; }\n"
    ".status-moving { color: #ffb74d; font-weight: bold; }\n"
    ".status-error { color: #e57373; font-weight: bold; }\n"
    ".button-row { display: flex; flex-wrap: wrap; gap: 10px; margin-top: 15px; }\n"
    ".button-primary { background-color: #4fc3f7; color: #000; }\n"
    ".button-warning { background-color: #ffb74d; color: #000; }\n"
    ".button-danger { background-color: #e57373; color: #000; }\n"
    ".telescope-status { display: inline-block; margin: 20px auto; text-align: center; }\n"
    ".status-indicator { display: inline-block; width: 24px; height: 24px; border-radius: 50%; margin-right: 10px; vertical-align: middle; }\n"
    ".status-text { display: inline-block; font-weight: bold; vertical-align: middle; }\n"
    ".indicator-red { background-color: #e57373; }\n"
    ".indicator-green { background-color: #81c784; }\n"
    ".indicator-yellow { background-color: #ffd54f; animation: blink 1s infinite alternate; }\n"
    "@keyframes blink { from { opacity: 0.6; } to { opacity: 1; } }\n";

  return styles;
}

// Add a new function to generate the telescope status indicator
inline String getTelescopeStatusIndicator() {
  String indicatorClass = "";
  String statusText = "";
  
  if (bypassParkSensor) {
    indicatorClass = "indicator-yellow";
    statusText = "Bypass Enabled";
  } else if (telescopeParked) {
    indicatorClass = "indicator-green";
    statusText = "Telescope Parked";
  } else {
    indicatorClass = "indicator-red";
    statusText = "Telescope NOT Parked";
  }
  
  String html = "<div class='telescope-status'>";
  html += "<div class='status-indicator " + indicatorClass + "'></div>";
  html += "<span class='status-text'>" + statusText + "</span>";
  html += "</div>";
  
  return html;
}

// Toggle switch CSS styles - Dark Theme
inline String getToggleSwitchStyles() {
  String styles =
    ".switch {position: relative; display: inline-block; width: 60px; height: 34px;}\n"
    ".switch input {opacity: 0; width: 0; height: 0;}\n"
    ".slider {position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #555; transition: .4s; border-radius: 34px;}\n"
    ".slider:before {position: absolute; content: \"\"; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: #e0e0e0; transition: .4s; border-radius: 50%;}\n"
    "input:checked + .slider {background-color: #4fc3f7;}\n"
    "input:focus + .slider {box-shadow: 0 0 1px #4fc3f7;}\n"
    "input:checked + .slider:before {transform: translateX(26px);}\n"
    "input.danger:checked + .slider {background-color: #e57373;}\n"
    ".switch-label {display: inline-block; vertical-align: middle; margin-left: 10px; font-weight: bold; color: #e0e0e0;}\n"
    ".switch-container {margin-bottom: 20px; display: flex; align-items: center;}\n"
    ".toggle-group { display: flex; flex-direction: column; gap: 15px; margin-bottom: 20px; padding: 15px; background-color: #333; border-radius: 8px; border: 1px solid #555; }\n"
    ".toggle-group h3 { margin-top: 0; color: #81c784; }\n"
    ".toggle-row { display: flex; flex-wrap: wrap; gap: 20px; }\n";

  return styles;
}

// Common HTML page header (title, meta tags, styles)
inline String getPageHeader(String pageTitle) {
  String header = "<!DOCTYPE html><html>\n"
    "<head><title>" + pageTitle + "</title>\n"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>\n"
    "<style>\n" + 
    getCommonStyles() + 
    getToggleSwitchStyles() + 
    "</style>\n"
    "</head>\n"
    "<body>\n";
  
  return header;
}

// Navigation links
inline String getNavBar() {
  String navbar =
    "<div style='margin-bottom: 20px; padding: 10px; background-color: #2d2d2d; border-radius: 4px; box-shadow: 0 2px 4px rgba(0,0,0,0.3);'>\n"
    "<a href='/' style='margin-right: 10px; padding: 8px 12px; background-color: #3498db; color: white; border-radius: 4px; text-decoration: none;'>Home</a>\n"
    "<a href='/control' style='margin-right: 10px; padding: 8px 12px; background-color: #2ecc71; color: white; border-radius: 4px; text-decoration: none;'>Roof Control</a>\n"
    "<a href='/setup' style='margin-right: 10px; padding: 8px 12px; background-color: #3498db; color: white; border-radius: 4px; text-decoration: none;'>Setup</a>\n"
    "<a href='/wificonfig' style='margin-right: 10px; padding: 8px 12px; background-color: #3498db; color: white; border-radius: 4px; text-decoration: none;'>WiFi Config</a>\n"
    "<a href='/update' style='padding: 8px 12px; background-color: #f39c12; color: white; border-radius: 4px; text-decoration: none;'>Update</a>\n"
    "</div>\n";

  return navbar;
}

// Updated getRoofControlJS function with better error handling
inline String getControlJS() {
  String js = 
    "<script>\n"
    // Add toggle switch update functions for setup page
    "function updateToggleLabel(toggleId, labelId, enabledText, disabledText) {\n"
    "  const toggle = document.getElementById(toggleId);\n"
    "  const label = document.getElementById(labelId);\n"
    "  if (toggle && label) {\n"
    "    label.textContent = toggle.checked ? enabledText : disabledText;\n"
    "    if (toggleId === 'bypassToggle') {\n"
    "      label.style.color = toggle.checked ? '#f44336' : '#333';\n"
    "    }\n"
    "  }\n"
    "}\n"
    
    // Settings application function
    "function applyPinSettings() {\n"
    "  if (confirm('Are you sure you want to change these settings? This may affect device operation.')) {\n"
    "    const triggerState = document.getElementById('triggerState').checked ? 'high' : 'low';\n"
    "    const swapSwitches = document.getElementById('swapSwitches').checked ? 'true' : 'false';\n"
    "    const mqttEnabled = document.getElementById('mqttEnabled').checked ? 'true' : 'false';\n"
    "    const inverterRelay = document.getElementById('inverterRelayToggle').checked ? 'true' : 'false';\n"
    "    const inverterSoftPwr = document.getElementById('inverterSoftPwrToggle').checked ? 'true' : 'false';\n"
    "    const limitSwitchTimeoutEnabled = document.getElementById('limitSwitchTimeoutEnabledToggle').checked ? 'true' : 'false';\n"
    "    const timeoutEnabled = document.getElementById('timeoutEnabledToggle').checked ? 'true' : 'false';\n"
    "    const delay1 = document.getElementById('delay1Input').value;\n"
    "    const delay2 = document.getElementById('delay2Input').value;\n"
    "    const limitSwitchTimeout = document.getElementById('limitSwitchTimeoutInput').value;\n"
    "    const timeout = document.getElementById('timeoutInput').value;\n"
    "    const parkSwitchType = document.getElementById('parkSwitchType').checked ? 'high' : 'low';\n"
    "    \n"
    "    fetch('/set_pins', {\n"
    "      method: 'POST',\n"
    "      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },\n"
    "      body: 'triggerState=' + triggerState + '&swapSwitches=' + swapSwitches + '&mqttEnabled=' + mqttEnabled + '&inverterRelay=' + inverterRelay + '&inverterSoftPwr=' + inverterSoftPwr + '&limitSwitchTimeoutEnabled=' + limitSwitchTimeoutEnabled + '&timeoutEnabled=' + timeoutEnabled + '&delay1=' + delay1 + '&delay2=' + delay2 + '&limitSwitchTimeout=' + limitSwitchTimeout + '&timeout=' + timeout + '&parkSwitchType=' + parkSwitchType\n"
    "    })\n"
    "    .then(response => response.text())\n"
    "    .then(data => {\n"
    "      alert(data);\n"
    "      location.reload();\n"
    "    })\n"
    "    .catch(err => {\n"
    "      console.error('Error:', err);\n"
    "      alert('Error applying settings: ' + err);\n"
    "    });\n"
    "  }\n"
    "}\n"
    
    // Restart device function
    "function restartDevice() {\n"
    "  if (confirm('Are you sure you want to restart the device?')) {\n"
    "    fetch('/restart', {\n"
    "      method: 'POST'\n"
    "    })\n"
    "    .then(response => {\n"
    "      alert('Device is restarting...');\n"
    "    })\n"
    "    .catch(err => {\n"
    "      console.error('Error:', err);\n"
    "    });\n"
    "  }\n"
    "}\n"
    
    // Toggle bypass park sensor
    "function toggleBypass(checked) {\n"
    "  fetch('/toggle_bypass', {\n"
    "    method: 'POST',\n"
    "    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },\n"
    "    body: 'bypass=' + checked\n"
    "  })\n"
    "  .then(response => response.text())\n"
    "  .then(data => {\n"
    "    console.log(data);\n"
    "    document.getElementById('bypassText').innerHTML = checked ? '(ENABLED)' : '(DISABLED)';\n"
    "    document.getElementById('bypassText').parentElement.style.color = checked ? '#e57373' : '#e0e0e0';\n"
    "  });\n"
    "}\n"
    
    // WiFi network selection for WiFi config page
    "function selectNetwork(name) {\n"
    "  document.getElementById('ssid').value = name;\n"
    "  document.getElementById('password').focus();\n"
    "}\n"
    
    // Park sensor management functions
    "function toggleSensorEnabled(uuid, enabled) {\n"
    "  fetch('/park_sensor_enabled', {\n"
    "    method: 'POST',\n"
    "    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },\n"
    "    body: 'uuid=' + encodeURIComponent(uuid) + '&enabled=' + enabled\n"
    "  })\n"
    "  .then(response => response.text())\n"
    "  .then(data => {\n"
    "    console.log(data);\n"
    "  })\n"
    "  .catch(err => {\n"
    "    console.error('Error:', err);\n"
    "    alert('Error toggling sensor: ' + err);\n"
    "  });\n"
    "}\n"
    
    "function toggleSensorBypass(uuid, bypassed) {\n"
    "  fetch('/park_sensor_bypass', {\n"
    "    method: 'POST',\n"
    "    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },\n"
    "    body: 'uuid=' + encodeURIComponent(uuid) + '&bypass=' + bypassed\n"
    "  })\n"
    "  .then(response => response.text())\n"
    "  .then(data => {\n"
    "    console.log(data);\n"
    "  })\n"
    "  .catch(err => {\n"
    "    console.error('Error:', err);\n"
    "    alert('Error toggling sensor bypass: ' + err);\n"
    "  });\n"
    "}\n"
    
    "function removeSensor(uuid) {\n"
    "  if (confirm('Are you sure you want to remove this park sensor?')) {\n"
    "    fetch('/park_sensor_remove', {\n"
    "      method: 'POST',\n"
    "      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },\n"
    "      body: 'uuid=' + encodeURIComponent(uuid)\n"
    "    })\n"
    "    .then(response => response.text())\n"
    "    .then(data => {\n"
    "      alert(data);\n"
    "      location.reload();\n"
    "    })\n"
    "    .catch(err => {\n"
    "      console.error('Error:', err);\n"
    "      alert('Error removing sensor: ' + err);\n"
    "    });\n"
    "  }\n"
    "}\n"
    
    "function removeAllSensors() {\n"
    "  if (confirm('Are you sure you want to remove ALL park sensors? This cannot be undone.')) {\n"
    "    fetch('/park_sensor_remove_all', {\n"
    "      method: 'POST'\n"
    "    })\n"
    "    .then(response => response.text())\n"
    "    .then(data => {\n"
    "      alert(data);\n"
    "      location.reload();\n"
    "    })\n"
    "    .catch(err => {\n"
    "      console.error('Error:', err);\n"
    "      alert('Error removing all sensors: ' + err);\n"
    "    });\n"
    "  }\n"
    "}\n"
    
    "function applyParkSensorType() {\n"
    "  const physicalRadio = document.getElementById('parkTypePhysical');\n"
    "  const udpRadio = document.getElementById('parkTypeUDP');\n"
    "  const bothRadio = document.getElementById('parkTypeBoth');\n"
    "  \n"
    "  let sensorType = 0;\n"
    "  if (udpRadio && udpRadio.checked) sensorType = 1;\n"
    "  else if (bothRadio && bothRadio.checked) sensorType = 2;\n"
    "  \n"
    "  fetch('/park_sensor_type', {\n"
    "    method: 'POST',\n"
    "    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },\n"
    "    body: 'type=' + sensorType\n"
    "  })\n"
    "  .then(response => response.text())\n"
    "  .then(data => {\n"
    "    alert(data);\n"
    "    location.reload();\n"
    "  })\n"
    "  .catch(err => {\n"
    "    console.error('Error:', err);\n"
    "    alert('Error setting park sensor type: ' + err);\n"
    "  });\n"
    "}\n"
    
    "function refreshSensors() {\n"
    "  location.reload();\n"
    "}\n"
    "</script>\n";
  
  return js;
}

// Generate status display with color coding
inline String getStatusDisplay(RoofStatus status) {
  String statusString = getRoofStatusString(status);
  String statusClass = "";
  
  if (statusString == "Open") {
    statusClass = "status-open";
  } else if (statusString == "Closed") {
    statusClass = "status-closed";
  } else if (statusString == "Opening" || statusString == "Closing") {
    statusClass = "status-moving";
  } else {
    statusClass = "status-error";
  }
  
  return "<div class='status " + statusClass + "' style='font-size: 24px; margin: 20px 0; padding: 15px; border-radius: 8px;'>"
         "Roof Status: " + statusString +
         "</div>";
}

// Generate the home page HTML
inline String getHomePage(RoofStatus status, bool isApMode = false) {
  String html = getPageHeader("ESP32 Roll-Off Roof Status");
  
  // Add custom styles for home page - Dark Theme
  html += "<style>\n"
          "body { text-align: center; }\n"
          ".status-card { background-color: #2d2d2d; border-radius: 8px; padding: 20px; margin: 15px 0; box-shadow: 0 4px 6px rgba(0,0,0,0.3); text-align: left; border: 1px solid #404040; }\n"
          ".page-header { margin: 20px 0; }\n"
          ".open { background-color: #1e3a5f; color: #64b5f6; }\n"
          ".closed { background-color: #1e3a2f; color: #81c784; }\n"
          ".moving { background-color: #3a2f1e; color: #ffb74d; }\n"
          ".error { background-color: #3a1e1e; color: #e57373; }\n"
          ".ap-mode-banner { background-color: #3a3a1e; color: #ffd54f; padding: 10px; border-radius: 5px; margin: 10px 0; font-weight: bold; border: 1px solid #555; }\n"
          ".nav-button { display: inline-block; margin: 5px; padding: 10px 20px; background-color: #4fc3f7; color: #000; border-radius: 4px; text-decoration: none; font-weight: bold; }\n"
          ".nav-button:hover { background-color: #81d4fa; text-decoration: none; color: #000; }\n"
          ".status-indicator { display: inline-block; width: 12px; height: 12px; border-radius: 50%; margin-right: 5px; }\n"
          ".status-indicator.green { background-color: #81c784; }\n"
          ".status-indicator.red { background-color: #e57373; }\n"
          ".status-indicator.blue { background-color: #64b5f6; }\n"
          ".status-indicator.orange { background-color: #ffb74d; }\n"
          ".status-indicator.blink { animation: blink 1s infinite alternate; }\n"
          ".status-header { font-size: 24px; margin: 20px 0; padding: 15px; border-radius: 8px; text-align: center; font-weight: bold; border: 2px solid; }\n"
          ".status-table { width: 100%; margin-bottom: 15px; }\n"
          ".status-table th { text-align: left; width: 40%; padding: 8px; background-color: #333; color: #4fc3f7; }\n"
          ".status-table td { padding: 8px; background-color: #2d2d2d; color: #e0e0e0; }\n"
          ".mqtt-json { background-color: #333; padding: 10px; border-radius: 4px; font-family: monospace; white-space: pre-wrap; font-size: 0.9em; max-height: 150px; overflow-y: auto; color: #81c784; border: 1px solid #555; }\n"
          "@keyframes blink { from { opacity: 0.6; } to { opacity: 1; } }\n"
          "</style>\n";

  html += "<div class='page-header'>\n";
  html += "<h1>ESP32 Roll-Off Roof Controller</h1>\n";
  html += "<p style='color: #b0b0b0;'>Version: " + String(DEVICE_VERSION) + " | <span style='color: #81c784;'>Auto-updates every 2 seconds</span></p>\n";
  html += "</div>\n";
  
  // AP Mode Banner
  if (isApMode) {
    html += "<div class='ap-mode-banner'>\n"
            "SETUP MODE - Connect to WiFi network '" + String(AP_SSID) + "' with password '" + String(AP_PASSWORD) + "'\n"
            "<br>Then configure your WiFi settings at <a href='/wificonfig'>WiFi Configuration</a>\n"
            "</div>\n";
  }
  
  // Status class determination
  String statusClass = "";
  String indicatorClass = "";
  String statusString = getRoofStatusString(status);
  
  if (statusString == "Open") {
    statusClass = "open";
    indicatorClass = "blue";
  } else if (statusString == "Closed") {
    statusClass = "closed";
    indicatorClass = "green";
  } else if (statusString == "Opening") {
    statusClass = "moving";
    indicatorClass = "blue blink";
  } else if (statusString == "Closing") {
    statusClass = "moving";
    indicatorClass = "green blink";
  } else {
    statusClass = "error";
    indicatorClass = "red blink";
  }
  
  // Status header
  html += "<div id='mainStatusHeader' class='status-header " + statusClass + "'>\n";
  html += "<span id='mainStatusIndicator' class='status-indicator " + indicatorClass + "'></span> ";
  html += "Roof Status: <span id='mainStatusText'>" + statusString + "</span>";
  html += "</div>\n";

  // Navigation buttons
  html += "<div style='margin: 20px 0;'>\n";
  html += "<a href='/control' class='nav-button' style='background-color: #2ecc71;'>Roof Control</a>\n";
  html += "<a href='/setup' class='nav-button' style='background-color: #3498db;'>Device Setup</a>\n";
  html += "<a href='/wificonfig' class='nav-button' style='background-color: #3498db;'>WiFi Config</a>\n";
  html += "<a href='/update' class='nav-button' style='background-color: #f39c12;'>Update</a>\n";
  html += "</div>\n";
  
  // Device Information Card
  html += "<div class='status-card'>\n";
  html += "<h2>Device Information</h2>\n";
  html += "<table class='status-table'>\n";
  html += "<tr><th>Unique ID</th><td>" + uniqueID + "</td></tr>\n";
  html += "<tr><th>IP Address</th><td>" + WiFi.localIP().toString() + "</td></tr>\n";
  html += "<tr><th>MAC Address</th><td>" + WiFi.macAddress() + "</td></tr>\n";
  html += "<tr><th>Firmware Version</th><td>" + String(DEVICE_VERSION) + "</td></tr>\n";
  html += "<tr><th>Uptime</th><td>" + String(millis() / 1000 / 60) + " minutes</td></tr>\n";
  html += "</table>\n";
  html += "</div>\n";
  
  // Roof Status Card
  html += "<div class='status-card'>\n";
  html += "<h2>Roof Status</h2>\n";
  html += "<table class='status-table'>\n";
  html += "<tr><th>Current Status</th><td class='" + statusClass + "'>" + statusString + "</td></tr>\n";
  
  // Park sensor information based on type
  html += "<tr><th>Park Sensor Type</th><td>";
  switch (parkSensorType) {
    case PARK_SENSOR_PHYSICAL:
      html += "Physical Sensor Only";
      break;
    case PARK_SENSOR_UDP:
      html += "UDP Sensors Only";
      break;
    case PARK_SENSOR_BOTH:
      html += "Both (AND Logic)";
      break;
  }
  html += "</td></tr>\n";
  
  // Park sensor bypass state with appropriate indicator
  String bypassIndicatorClass = bypassParkSensor ? "red blink" : "green";
  html += "<tr><th>Park Sensor Bypass</th><td>";
  html += "<span class='status-indicator " + String(bypassParkSensor ? "red blink" : "green") + "'></span> ";
  html += bypassParkSensor ? "<span style='color: #e74c3c; font-weight: bold;'>ENABLED</span>" : "Disabled";
  html += " <small style='margin-left: 10px; color: #b0b0b0;'><a href='/setup'>Configure bypass in setup</a></small>";
  html += "</td></tr>\n";
  
  // Show telescope parked status based on park sensor type
  html += "<tr><th>Telescope Parked</th><td>";
  html += "<span class='status-indicator " + String(telescopeParked ? "green" : "red") + "'></span> ";
  html += telescopeParked ? "Yes" : "No";
  html += "</td></tr>\n";
  
  // Show park sensor details based on type
  if (parkSensorType == PARK_SENSOR_PHYSICAL || parkSensorType == PARK_SENSOR_BOTH) {
    html += "<tr><th>Physical Park Sensor</th><td>";
    html += "<span class='status-indicator " + String(telescopeParked ? "green" : "red") + "'></span> ";
    html += telescopeParked ? "Parked" : "Not Parked";
    html += "</td></tr>\n";
  }
  
  if (parkSensorType == PARK_SENSOR_UDP || parkSensorType == PARK_SENSOR_BOTH) {
    std::vector<ParkSensor> activeSensorsList = getActiveSensors();
    html += "<tr><th>UDP Park Sensors</th><td>";
    
    if (activeSensorsList.empty()) {
      html += "<span class='status-indicator red'></span> No sensors enabled";
    } else {
      bool allParked = isTelescopeParkedUDP();
      html += "<span class='status-indicator " + String(allParked ? "green" : "red") + "'></span> ";
      html += String(activeSensorsList.size()) + " sensor(s) - ";
      html += allParked ? "All Parked" : "Not All Parked";
      
      // Show individual sensor status
      html += "<br><small>";
      for (size_t i = 0; i < activeSensorsList.size(); i++) {
        const ParkSensor& sensor = activeSensorsList[i];
        if (i > 0) html += ", ";
        
        String sensorStatusClass = "";
        if (sensor.bypassEnabled) {
          sensorStatusClass = "orange";
        } else if (sensor.status == SENSOR_PARKED) {
          sensorStatusClass = "green";
        } else if (sensor.status == SENSOR_UNPARKED) {
          sensorStatusClass = "red";
        } else {
          sensorStatusClass = "red blink";
        }
        
        html += sensor.name + ": <span class='status-indicator " + sensorStatusClass + "'></span>";
        if (sensor.bypassEnabled) {
          html += "Bypassed";
        } else {
          html += getParkSensorStatusString(sensor.status);
        }
      }
      html += "</small>";
    }
    html += "</td></tr>\n";
  }
  
  // Limit switch states
  bool openSwitchTriggered = (digitalRead(LIMIT_SWITCH_OPEN_PIN) == TRIGGERED);
  bool closedSwitchTriggered = (digitalRead(LIMIT_SWITCH_CLOSED_PIN) == TRIGGERED);
  
  html += "<tr><th>Open Limit Switch</th><td>";
  html += "<span class='status-indicator " + String(openSwitchTriggered ? "green" : "red") + "'></span> ";
  html += openSwitchTriggered ? "Triggered" : "Not Triggered";
  html += "</td></tr>\n";
  
  html += "<tr><th>Closed Limit Switch</th><td>";
  html += "<span class='status-indicator " + String(closedSwitchTriggered ? "green" : "red") + "'></span> ";
  html += closedSwitchTriggered ? "Triggered" : "Not Triggered";
  html += "</td></tr>\n";
  
  html += "</table>\n";
  html += "</div>\n";

  // Inverter Status Card (v3) - Display only, no controls
  html += "<div class='status-card'>\n";
  html += "<h2>Inverter Status</h2>\n";
  html += "<table class='status-table'>\n";

  // Get inverter states
  bool inverterRelay = getInverterRelayState();
  bool inverterACPower = getInverterACPowerState();

  // Inverter relay state (K1)
  html += "<tr><th>Power Relay (K1)</th><td>";
  html += "<span class='status-indicator " + String(inverterRelay ? "green" : "red") + "'></span> ";
  html += inverterRelay ? "ON" : "OFF";
  html += "</td></tr>\n";

  // Inverter AC power state (via optocoupler)
  html += "<tr><th>AC Power Detected</th><td>";
  html += "<span class='status-indicator " + String(inverterACPower ? "green" : "red") + "'></span> ";
  html += inverterACPower ? "ON" : "OFF";
  html += "</td></tr>\n";

  html += "</table>\n";
  html += "<p style='font-size: 12px; color: #b0b0b0; margin-top: 10px; text-align: center;'>Use <a href='/control'>Roof Control</a> page to control inverter</p>\n";
  html += "</div>\n";

  // MQTT Information Card
  html += "<div class='status-card'>\n";
  html += "<h2>MQTT Information</h2>\n";
  html += "<table class='status-table'>\n";
  html += "<tr><th>MQTT Enabled</th><td>";
  html += "<span class='status-indicator " + String(mqttEnabled ? "green" : "red") + "'></span> ";
  html += mqttEnabled ? "Yes" : "No";
  html += "</td></tr>\n";
  
  if (mqttEnabled) {
    html += "<tr><th>MQTT Server</th><td>" + String(mqttServer) + ":" + String(mqttPort) + "</td></tr>\n";
    html += "<tr><th>MQTT Connected</th><td>";
    html += "<span class='status-indicator " + String(mqttClient.connected() ? "green" : "red") + "'></span> ";
    html += mqttClient.connected() ? "Yes" : "No";
    html += "</td></tr>\n";
    html += "<tr><th>MQTT Client ID</th><td>" + String(mqttClientId) + "</td></tr>\n";
    html += "<tr><th>Status Topic</th><td>" + String(mqttTopicStatus) + "</td></tr>\n";
    html += "<tr><th>Command Topic</th><td>" + String(mqttTopicCommand) + "</td></tr>\n";
    html += "<tr><th>Availability Topic</th><td>" + String(mqttTopicAvailability) + "</td></tr>\n";
    
    // Last MQTT Status Message (create a JSON example)
    DynamicJsonDocument doc(512);
    doc["status"] = statusString;
    doc["slaved"] = slaved;
    doc["telescope_parked"] = telescopeParked;
    doc["limit_open"] = openSwitchTriggered;
    doc["limit_closed"] = closedSwitchTriggered;
    doc["bypass_enabled"] = bypassParkSensor;
    doc["device_id"] = uniqueID;
    doc["ip_address"] = WiFi.localIP().toString();
    
    String lastStatusJson;
    serializeJsonPretty(doc, lastStatusJson);
    
    html += "<tr><th>Last Status Message</th><td><div class='mqtt-json'>" + lastStatusJson + "</div></td></tr>\n";
  }
  
  html += "</table>\n";
  html += "</div>\n";
  
  // Simple JavaScript for real-time status updates
  html += "<script>\n";
  html += "document.addEventListener('DOMContentLoaded', function() {\n";
  html += "  console.log('Page loaded, auto-refresh enabled');\n";
  html += "});\n\n";

  html += "function roofControl(action) {\n";
  html += "  fetch('/roof_control', {\n";
  html += "    method: 'POST',\n";
  html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},\n";
  html += "    body: 'action=' + action\n";
  html += "  })\n";
  html += "    .then(response => response.text())\n";
  html += "    .then(data => {\n";
  html += "      setTimeout(() => location.reload(), 500);\n";
  html += "    })\n";
  html += "    .catch(error => alert('Error: ' + error));\n";
  html += "}\n\n";

  // Add real-time status update function for home page
  html += "function updateHomeStatus() {\n";
  html += "  fetch('/api/status')\n";
  html += "    .then(response => response.json())\n";
  html += "    .then(data => {\n";
  html += "      // Update main status header\n";
  html += "      const statusHeader = document.getElementById('mainStatusHeader');\n";
  html += "      const statusIndicator = document.getElementById('mainStatusIndicator');\n";
  html += "      const statusText = document.getElementById('mainStatusText');\n";
  html += "      if (statusHeader && statusIndicator && statusText) {\n";
  html += "        statusText.textContent = data.status;\n";
  html += "        statusHeader.className = 'status-header ';\n";
  html += "        statusIndicator.className = 'status-indicator ';\n";
  html += "        if (data.status === 'Open') {\n";
  html += "          statusHeader.className += 'open';\n";
  html += "          statusIndicator.className += 'blue';\n";
  html += "        } else if (data.status === 'Closed') {\n";
  html += "          statusHeader.className += 'closed';\n";
  html += "          statusIndicator.className += 'green';\n";
  html += "        } else if (data.status === 'Opening') {\n";
  html += "          statusHeader.className += 'moving';\n";
  html += "          statusIndicator.className += 'blue blink';\n";
  html += "        } else if (data.status === 'Closing') {\n";
  html += "          statusHeader.className += 'moving';\n";
  html += "          statusIndicator.className += 'green blink';\n";
  html += "        } else {\n";
  html += "          statusHeader.className += 'error';\n";
  html += "          statusIndicator.className += 'red blink';\n";
  html += "        }\n";
  html += "      }\n";
  html += "    })\n";
  html += "    .catch(error => console.error('Error updating status:', error));\n";
  html += "}\n\n";

  // Start polling on page load
  html += "// Start auto-updating when page loads\n";
  html += "document.addEventListener('DOMContentLoaded', function() {\n";
  html += "  updateHomeStatus(); // Initial update\n";
  html += "  setInterval(updateHomeStatus, 2000); // Update every 2 seconds\n";
  html += "});\n";

  html += "</script>\n";

  html += "</body></html>";
  
  return html;
}

// Status card for the setup page
inline String getStatusCard() {
  String html = "<div class='card'>";
  html += "<h2>Device Status</h2>";
  html += "<table>";
  html += "<tr><td>Manufacturer</td><td>" + String(DEVICE_MANUFACTURER) + "</td></tr>";
  html += "<tr><td>Version</td><td>" + String(DEVICE_VERSION) + "</td></tr>";
  html += "<tr><td>Unique ID</td><td>" + uniqueID + "</td></tr>";
  html += "<tr><td>IP Address</td><td>" + WiFi.localIP().toString() + "</td></tr>";
  
  // Status with color coding
  String statusClass = "";
  String statusString = getRoofStatusString();
  if (statusString == "Open") {
    statusClass = "status-open";
  } else if (statusString == "Closed") {
    statusClass = "status-closed";
  } else if (statusString == "Opening" || statusString == "Closing") {
    statusClass = "status-moving";
  } else {
    statusClass = "status-error";
  }
  
  html += "<tr><td>Roof Status</td><td class='" + statusClass + "'>" + statusString + "</td></tr>";
  html += "<tr><td>Telescope Parked</td><td>" + String(telescopeParked ? "Yes" : "No") + "</td></tr>";
  html += "<tr><td>Slaved</td><td>" + String(slaved ? "Yes" : "No") + "</td></tr>";
  html += "<tr><td>Open Limit Switch</td><td>" + String(digitalRead(LIMIT_SWITCH_OPEN_PIN) == TRIGGERED ? "Triggered" : "Not Triggered") + "</td></tr>";
  html += "<tr><td>Closed Limit Switch</td><td>" + String(digitalRead(LIMIT_SWITCH_CLOSED_PIN) == TRIGGERED ? "Triggered" : "Not Triggered") + "</td></tr>";
  html += "<tr><td>MQTT Connected</td><td>" + String(mqttClient.connected() ? "Yes" : "No") + "</td></tr>";
  html += "</table>";
  html += "</div>";
  
  return html;
}

// WiFi settings card for the setup page
inline String getWifiSettingsCard() {
  String html = "<div class='card'>";
  html += "<h2>WiFi Settings</h2>";
  html += "<form method='post' action='/setup'>";
  html += "<label for='ssid'>WiFi SSID:</label>";
  html += "<input type='text' id='ssid' name='ssid' value='" + String(ssid) + "'>";
  html += "<label for='password'>WiFi Password:</label>";
  html += "<input type='password' id='password' name='password' value='" + String(password) + "'>";
  html += "<input type='submit' value='Save WiFi Settings'>";
  html += "</form>";
  html += "<p><a href='/wificonfig'>Advanced WiFi Configuration</a></p>";
  html += "</div>";
  
  return html;
}

// MQTT settings card for the setup page
inline String getMqttSettingsCard() {
  String html = "<div class='card'>";
  html += "<h2>MQTT Settings</h2>";
  html += "<form method='post' action='/setup'>";
  
  // MQTT enabled toggle
  html += "<div class='switch-container' style='margin-bottom: 20px;'>";
  html += "<label class='switch'>";
  html += "<input type='checkbox' id='mqttEnabled'" + String(mqttEnabled ? " checked" : "") + " onchange=\"updateToggleLabel('mqttEnabled', 'mqttEnabledText', 'Enabled', 'Disabled')\">";
  html += "<span class='slider'></span>";
  html += "</label>";
  html += "<span class='switch-label'>Enable MQTT: <strong id='mqttEnabledText'>" + String(mqttEnabled ? "Enabled" : "Disabled") + "</strong></span>";
  html += "</div>";
  
  html += "<label for='mqttServer'>MQTT Server:</label>";
  html += "<input type='text' id='mqttServer' name='mqttServer' value='" + String(mqttServer) + "'>";
  html += "<label for='mqttPort'>MQTT Port:</label>";
  html += "<input type='number' id='mqttPort' name='mqttPort' value='" + String(mqttPort) + "'>";
  html += "<label for='mqttUser'>MQTT Username:</label>";
  html += "<input type='text' id='mqttUser' name='mqttUser' value='" + String(mqttUser) + "'>";
  html += "<label for='mqttPassword'>MQTT Password:</label>";
  html += "<input type='password' id='mqttPassword' name='mqttPassword' value='" + String(mqttPassword) + "'>";
  html += "<label for='mqttClientId'>MQTT Client ID:</label>";
  html += "<input type='text' id='mqttClientId' name='mqttClientId' value='" + String(mqttClientId) + "'>";
  html += "<p style='font-size: 0.8em; color: #b0b0b0;'>Unique identifier for this controller. Change this to prevent conflicts when testing multiple controllers.</p>";
  html += "<label for='mqttTopicPrefix'>MQTT Topic Prefix:</label>";
  html += "<input type='text' id='mqttTopicPrefix' name='mqttTopicPrefix' value='" + String(mqttTopicPrefix) + "'>";
  html += "<p style='font-size: 0.8em; color: #b0b0b0;'>Status and command topics will be created as [prefix]/status and [prefix]/command</p>";
  html += "<input type='submit' value='Save MQTT Settings'>";
  html += "</form>";
  html += "</div>";
  
  return html;
}

// Switch configuration card for the setup page
inline String getSwitchConfigCard() {
  String html = "<div class='card'>";
  html += "<h2>Device Configuration</h2>";
  
  // Toggle group with clear sections
  html += "<div class='toggle-group'>";
  html += "<h3>Sensor Settings</h3>";
  
  // Toggle containers in a flex layout
  html += "<div class='toggle-row'>";
  
  // Trigger state toggle (HIGH/LOW)
  html += "<div class='switch-container'>";
  html += "<label class='switch'>";
  html += "<input type='checkbox' id='triggerState'" + String(TRIGGERED == LOW ? "" : " checked") + " onchange=\"updateToggleLabel('triggerState', 'triggerStateText', 'HIGH', 'LOW')\">";
  html += "<span class='slider'></span>";
  html += "</label>";
  html += "<span class='switch-label'>Limit Switch Trigger State: <strong id='triggerStateText'>" + String(TRIGGERED == HIGH ? "HIGH" : "LOW") + "</strong></span>";
  html += "</div>";
  
  // Switch pin swapping toggle
  html += "<div class='switch-container'>";
  html += "<label class='switch'>";
  html += "<input type='checkbox' id='swapSwitches'" + String(swapLimitSwitches ? " checked" : "") + " onchange=\"updateToggleLabel('swapSwitches', 'swapSwitchesText', 'Swapped', 'Default')\">";
  html += "<span class='slider'></span>";
  html += "</label>";
  html += "<span class='switch-label'>Swap Open/Close Switch Pins: <strong id='swapSwitchesText'>" + String(swapLimitSwitches ? "Swapped" : "Default") + "</strong></span>";
  html += "</div>";
  
  html += "</div>"; // End toggle-row
  
  // Add a new section for safety settings
  html += "<h3>Safety Settings</h3>";
  html += "<div class='toggle-row'>";
  
  // Bypass park sensor toggle with improved labeling
  html += "<div class='switch-container'>";
  html += "<label class='switch'>";
  html += "<input type='checkbox' id='bypassToggle' class='danger'" + String(bypassParkSensor ? " checked" : "") + " onchange=\"toggleBypass(this.checked)\">";
  html += "<span class='slider'></span>";
  html += "</label>";
  html += "<span class='switch-label' style='color: " + String(bypassParkSensor ? "#e57373" : "#e0e0e0") + ";'>";
  html += "Bypass Telescope Park Sensor <strong id='bypassText'>(" + String(bypassParkSensor ? "ENABLED" : "DISABLED") + ")</strong><br>";
  html += "<small>Warning: When enabled, allows roof to move regardless of telescope position</small><br>";
  html += "<small style='color: #ffb74d;'><strong>Note:</strong> This setting is not retained between restarts. Use a physical jumper on the PARK terminals if no sensor is installed.</small>";
  html += "</span>";
  html += "</div>";
  
  html += "</div>"; // End toggle-row

  // Park switch type toggle (normally open vs normally closed)
  html += "<div class='toggle-row'>";
  html += "<div class='switch-container'>";
  html += "<label class='switch'>";
  html += "<input type='checkbox' id='parkSwitchType'" + String(TELESCOPE_PARKED == HIGH ? " checked" : "") + " onchange=\"updateToggleLabel('parkSwitchType', 'parkSwitchTypeText', 'Normally Closed', 'Normally Open')\">";
  html += "<span class='slider'></span>";
  html += "</label>";
  html += "<span class='switch-label'>";
  html += "Park Switch Type: <strong id='parkSwitchTypeText'>" + String(TELESCOPE_PARKED == HIGH ? "Normally Closed" : "Normally Open") + "</strong><br>";
  html += "<small>Normally Open (recommended): Pin is pulled LOW when scope is parked</small><br>";
  html += "<small>Normally Closed: Pin is pulled HIGH when scope is parked</small><br>";
  html += "<small style='color: #ffb74d;'><strong>Note:</strong> A normally open switch is safer - a broken wire will indicate an unsafe (unparked) state.</small>";
  html += "</span>";
  html += "</div>";
  html += "</div>"; // End toggle-row

  // Add a new section for inverter settings
  html += "<h3>Inverter Settings</h3>";
  html += "<div class='toggle-row'>";

  // Inverter relay (K1) toggle
  html += "<div class='switch-container'>";
  html += "<label class='switch'>";
  html += "<input type='checkbox' id='inverterRelayToggle'" + String(inverterRelayEnabled ? " checked" : "") + " onchange=\"updateToggleLabel('inverterRelayToggle', 'inverterRelayText', 'ENABLED', 'DISABLED')\">";
  html += "<span class='slider'></span>";
  html += "</label>";
  html += "<span class='switch-label'>";
  html += "Enable Power Relay (K1) <strong id='inverterRelayText'>(" + String(inverterRelayEnabled ? "ENABLED" : "DISABLED") + ")</strong><br>";
  html += "<small>Auto-control K1 relay to power inverter when opening/closing roof</small>";
  html += "</span>";
  html += "</div>";

  html += "</div>"; // End toggle-row

  html += "<div class='toggle-row'>";

  // Inverter soft-power button (K3) toggle
  html += "<div class='switch-container'>";
  html += "<label class='switch'>";
  html += "<input type='checkbox' id='inverterSoftPwrToggle'" + String(inverterSoftPwrEnabled ? " checked" : "") + " onchange=\"updateToggleLabel('inverterSoftPwrToggle', 'inverterSoftPwrText', 'ENABLED', 'DISABLED')\">";
  html += "<span class='slider'></span>";
  html += "</label>";
  html += "<span class='switch-label'>";
  html += "Enable Soft-Power Button (K3) <strong id='inverterSoftPwrText'>(" + String(inverterSoftPwrEnabled ? "ENABLED" : "DISABLED") + ")</strong><br>";
  html += "<small>Auto-press K3 soft-power button if no AC power detected when opening/closing roof</small>";
  html += "</span>";
  html += "</div>";

  html += "</div>"; // End toggle-row

  // Inverter delay settings
  html += "<div class='toggle-row' style='margin-top: 15px;'>";

  html += "<div style='margin-right: 20px;'>";
  html += "<label for='delay1Input' style='display: block; margin-bottom: 5px;'><strong>Inverter Delay 1 (ms):</strong></label>";
  html += "<input type='number' id='delay1Input' min='0' max='10000' value='" + String(inverterDelay1) + "' ";
  html += "style='width: 120px; padding: 5px; font-size: 16px;' />";
  html += "<p style='margin-top: 5px; font-size: 12px; color: #b0b0b0;'>Time between K1 power relay and K3 soft-power button (0-10000ms, default: 750)</p>";
  html += "</div>";

  html += "<div>";
  html += "<label for='delay2Input' style='display: block; margin-bottom: 5px;'><strong>Inverter Delay 2 (ms):</strong></label>";
  html += "<input type='number' id='delay2Input' min='0' max='10000' value='" + String(inverterDelay2) + "' ";
  html += "style='width: 120px; padding: 5px; font-size: 16px;' />";
  html += "<p style='margin-top: 5px; font-size: 12px; color: #b0b0b0;'>Time between inverter power-on and K2 roof button (0-10000ms, default: 1500)</p>";
  html += "</div>";

  html += "</div>"; // End toggle-row
  html += "</div>"; // End toggle-group

  // Timing Settings
  html += "<h3>Timing Settings</h3>";

  // Limit switch timeout monitoring toggle
  html += "<div style='display: flex; justify-content: center; margin-bottom: 10px;'>";
  html += "<div class='switch-container'>";
  html += "<label class='switch'>";
  html += "<input type='checkbox' id='limitSwitchTimeoutEnabledToggle'" + String(limitSwitchTimeoutEnabled ? " checked" : "") + " onchange=\"updateToggleLabel('limitSwitchTimeoutEnabledToggle', 'limitSwitchTimeoutEnabledText', 'ENABLED', 'DISABLED')\">";
  html += "<span class='slider'></span>";
  html += "</label>";
  html += "<span class='switch-label'>";
  html += "Monitor Limit Switch Changes <strong id='limitSwitchTimeoutEnabledText'>(" + String(limitSwitchTimeoutEnabled ? "ENABLED" : "DISABLED") + ")</strong><br>";
  html += "<small>Turn off inverter if limit switch doesn't change state within timeout after movement starts</small>";
  html += "</span>";
  html += "</div>";
  html += "</div>";

  html += "<div style='margin-top: 10px; padding: 10px; background-color: #2d2d2d; border-radius: 4px;'>";
  html += "<label for='limitSwitchTimeoutInput' style='display: block; margin-bottom: 5px;'><strong>Limit Switch Timeout (seconds):</strong></label>";
  html += "<input type='number' id='limitSwitchTimeoutInput' min='1' max='30' value='" + String(limitSwitchTimeout / 1000) + "' ";
  html += "style='width: 100px; padding: 5px; font-size: 16px;' />";
  html += "<p style='margin-top: 5px; font-size: 12px; color: #b0b0b0;'>Time to wait for limit switch state to change after movement starts (1-30 seconds, default: 5)</p>";
  html += "</div>";

  // Movement timeout monitoring toggle
  html += "<div style='display: flex; justify-content: center; margin-bottom: 10px;'>";
  html += "<div class='switch-container'>";
  html += "<label class='switch'>";
  html += "<input type='checkbox' id='timeoutEnabledToggle'" + String(movementTimeoutEnabled ? " checked" : "") + " onchange=\"updateToggleLabel('timeoutEnabledToggle', 'timeoutEnabledText', 'ENABLED', 'DISABLED')\">";
  html += "<span class='slider'></span>";
  html += "</label>";
  html += "<span class='switch-label'>";
  html += "Monitor Total Movement Time <strong id='timeoutEnabledText'>(" + String(movementTimeoutEnabled ? "ENABLED" : "DISABLED") + ")</strong><br>";
  html += "<small>Automatically stop and turn off inverter if roof doesn't fully open/close within total timeout period</small>";
  html += "</span>";
  html += "</div>";
  html += "</div>";

  html += "<div style='margin-top: 10px; padding: 10px; background-color: #2d2d2d; border-radius: 4px;'>";
  html += "<label for='timeoutInput' style='display: block; margin-bottom: 5px;'><strong>Movement Timeout (seconds):</strong></label>";
  html += "<input type='number' id='timeoutInput' min='10' max='600' value='" + String(movementTimeout / 1000) + "' ";
  html += "style='width: 100px; padding: 5px; font-size: 16px;' />";
  html += "<p style='margin-top: 5px; font-size: 12px; color: #b0b0b0;'>Total time allowed for roof to fully open or close (10-600 seconds, default: 90)</p>";
  html += "</div>";

  // Current configuration
  html += "<div style='margin-top: 10px; padding: 10px; background-color: #2d2d2d; border-radius: 4px;'>";
  html += "<p><strong>Current Configuration:</strong><br>";
  html += "Trigger state: " + String(TRIGGERED == HIGH ? "HIGH" : "LOW") + "<br>";
  html += "Open switch pin: " + String(LIMIT_SWITCH_OPEN_PIN) + "<br>";
  html += "Closed switch pin: " + String(LIMIT_SWITCH_CLOSED_PIN) + "<br>";
  html += "Park sensor bypass: " + String(bypassParkSensor ? "Enabled" : "Disabled") + "<br>";
  html += "Inverter relay (K1): " + String(inverterRelayEnabled ? "Enabled" : "Disabled") + "<br>";
  html += "Soft-power button (K3): " + String(inverterSoftPwrEnabled ? "Enabled" : "Disabled") + "<br>";
  html += "Inverter Delay 1: " + String(inverterDelay1) + "ms<br>";
  html += "Inverter Delay 2: " + String(inverterDelay2) + "ms<br>";
  html += "Limit switch timeout monitoring: " + String(limitSwitchTimeoutEnabled ? "Enabled" : "Disabled") + "<br>";
  html += "Limit switch timeout: " + String(limitSwitchTimeout / 1000) + " seconds<br>";
  html += "Movement timeout monitoring: " + String(movementTimeoutEnabled ? "Enabled" : "Disabled") + "<br>";
  html += "Movement timeout: " + String(movementTimeout / 1000) + " seconds</p>";
  html += "</div>";

  // Apply button
  html += "<button onclick='applyPinSettings()' class='button-danger' style='margin-top: 15px;'>Apply Settings</button>";
  html += "</div>";

  return html;
}

// Park Sensor configuration card for the setup page
inline String getParkSensorConfigCard() {
  String html = "<div class='card'>";
  html += "<h2>Park Sensor Configuration</h2>";
  
  // Park sensor type selection
  html += "<div class='toggle-group'>";
  html += "<h3>Park Sensor Type</h3>";
  html += "<p>Choose which park sensor system to use:</p>";

  html += "<div style='display: flex; flex-direction: column; align-items: center; margin-bottom: 20px;'>";
  html += "<div style='display: inline-block; text-align: left;'>";

  html += "<div style='margin-bottom: 10px; display: flex; align-items: center;'>";
  html += "<input type='radio' id='parkTypePhysical' name='parkType' value='0'" +
          String(parkSensorType == PARK_SENSOR_PHYSICAL ? " checked" : "") + ">";
  html += "<label for='parkTypePhysical' style='margin-left: 8px; cursor: pointer;'>Physical Sensor Only</label>";
  html += "</div>";

  html += "<div style='margin-bottom: 10px; display: flex; align-items: center;'>";
  html += "<input type='radio' id='parkTypeUDP' name='parkType' value='1'" +
          String(parkSensorType == PARK_SENSOR_UDP ? " checked" : "") + ">";
  html += "<label for='parkTypeUDP' style='margin-left: 8px; cursor: pointer;'>UDP Sensors Only</label>";
  html += "</div>";

  html += "<div style='margin-bottom: 10px; display: flex; align-items: center;'>";
  html += "<input type='radio' id='parkTypeBoth' name='parkType' value='2'" +
          String(parkSensorType == PARK_SENSOR_BOTH ? " checked" : "") + ">";
  html += "<label for='parkTypeBoth' style='margin-left: 8px; cursor: pointer;'>Both (AND Logic)</label>";
  html += "</div>";

  html += "</div>"; // End inline-block container
  html += "</div>"; // End flex container
  html += "</div>"; // End toggle-group
  
  // UDP Park Sensor Status
  html += "<div class='toggle-group'>";
  html += "<h3>UDP Park Sensor Status</h3>";
  html += "<p>UDP Listener: <strong>" + String(udpParkSensorSystemEnabled ? "Active" : "Inactive") + "</strong> on port " + String(PARK_SENSOR_UDP_PORT) + "</p>";
  
  // Get discovered sensors
  std::vector<ParkSensor> discoveredSensorsList = getDiscoveredSensors();
  std::vector<ParkSensor> activeSensorsList = getActiveSensors();
  
  if (discoveredSensorsList.empty()) {
    html += "<p><em>No UDP park sensors discovered yet. Sensors broadcast every 2 seconds on UDP port " + String(PARK_SENSOR_UDP_PORT) + ".</em></p>";
  } else {
    html += "<h4>Discovered Sensors (" + String(discoveredSensorsList.size()) + " total)</h4>";
    html += "<table style='width: 100%; margin-bottom: 15px;'>";
    html += "<tr><th>Name</th><th>Status</th><th>IP Address</th><th>Last Seen</th><th>Enabled</th><th>Bypass</th><th>Actions</th></tr>";
    
    for (const auto& sensor : discoveredSensorsList) {
      html += "<tr>";
      html += "<td>" + sensor.name + "<br><small>" + sensor.uuid.substring(0, 8) + "...</small></td>";
      
      // Status with color coding
      String statusClass = "";
      if (sensor.status == SENSOR_PARKED) {
        statusClass = "style='color: green; font-weight: bold;'";
      } else if (sensor.status == SENSOR_UNPARKED) {
        statusClass = "style='color: red; font-weight: bold;'";
      } else if (sensor.status == SENSOR_OFFLINE) {
        statusClass = "style='color: orange; font-weight: bold;'";
      } else {
        statusClass = "style='color: gray;'";
      }
      html += "<td " + statusClass + ">" + getParkSensorStatusString(sensor.status) + "</td>";
      
      html += "<td>" + sensor.ipAddress + "</td>";
      
      // Last seen
      String lastSeenStr = "Never";
      if (sensor.lastSeen > 0) {
        unsigned long timeDiff = (millis() - sensor.lastSeen) / 1000;
        if (timeDiff < 60) {
          lastSeenStr = String(timeDiff) + "s ago";
        } else if (timeDiff < 3600) {
          lastSeenStr = String(timeDiff / 60) + "m ago";
        } else {
          lastSeenStr = String(timeDiff / 3600) + "h ago";
        }
      }
      html += "<td>" + lastSeenStr + "</td>";
      
      // Enabled status
      html += "<td>";
      html += "<label class='switch' style='transform: scale(0.7);'>";
      html += "<input type='checkbox' id='enabled_" + sensor.uuid + "'" + 
              String(sensor.enabled ? " checked" : "") + 
              " onchange='toggleSensorEnabled(\"" + sensor.uuid + "\", this.checked)'>";
      html += "<span class='slider'></span>";
      html += "</label>";
      html += "</td>";
      
      // Bypass status
      html += "<td>";
      html += "<label class='switch' style='transform: scale(0.7);'>";
      html += "<input type='checkbox' class='danger' id='bypass_" + sensor.uuid + "'" + 
              String(sensor.bypassEnabled ? " checked" : "") + 
              " onchange='toggleSensorBypass(\"" + sensor.uuid + "\", this.checked)'>";
      html += "<span class='slider'></span>";
      html += "</label>";
      html += "</td>";
      
      // Actions
      html += "<td>";
      html += "<button onclick='removeSensor(\"" + sensor.uuid + "\")' style='background: #e74c3c; color: white; border: none; padding: 4px 8px; border-radius: 3px; font-size: 12px;'>Remove</button>";
      html += "</td>";
      
      html += "</tr>";
    }
    html += "</table>";
    
    // Management buttons
    html += "<div class='button-row'>";
    html += "<button onclick='refreshSensors()' class='button-primary'>Refresh</button>";
    html += "<button onclick='removeAllSensors()' class='button-danger'>Remove All Sensors</button>";
    html += "<button onclick='applyParkSensorType()' class='button-primary'>Apply Sensor Type</button>";
    html += "</div>";
  }
  
  html += "</div>"; // End toggle-group
  html += "</div>"; // End card
  
  return html;
}

// System management card for the setup page
inline String getSystemManagementCard() {
  String html = "<div class='card'>";
  html += "<h2>System Management</h2>";
  
  html += "<div class='button-row'>";
  html += "<button onclick='restartDevice()' class='button-danger'>Restart Device</button>";
  html += "<a href='/force_discovery' style='text-decoration: none;'><button class='button-primary'>Force Discovery</button></a>";
  html += "</div>";
  
  html += "</div>";
  
  return html;
}

// Complete setup page
inline String getSetupPage() {
  String html = getPageHeader("ESP32 Roll-Off Roof Controller Setup");

  // Add custom styles matching home page
  html += "<style>\n"
          "body { text-align: center; }\n"
          ".status-card { background-color: #2d2d2d; border-radius: 8px; padding: 20px; margin: 15px 0; box-shadow: 0 4px 6px rgba(0,0,0,0.3); text-align: left; border: 1px solid #404040; }\n"
          ".page-header { margin: 20px 0; }\n"
          ".open { background-color: #1e3a5f; color: #64b5f6; }\n"
          ".closed { background-color: #1e3a2f; color: #81c784; }\n"
          ".moving { background-color: #3a2f1e; color: #ffb74d; }\n"
          ".error { background-color: #3a1e1e; color: #e57373; }\n"
          ".nav-button { display: inline-block; margin: 5px; padding: 10px 20px; background-color: #4fc3f7; color: #000; border-radius: 4px; text-decoration: none; font-weight: bold; }\n"
          ".nav-button:hover { background-color: #81d4fa; text-decoration: none; color: #000; }\n"
          ".status-indicator { display: inline-block; width: 12px; height: 12px; border-radius: 50%; margin-right: 5px; }\n"
          ".status-indicator.green { background-color: #81c784; }\n"
          ".status-indicator.red { background-color: #e57373; }\n"
          ".status-indicator.blue { background-color: #64b5f6; }\n"
          ".status-indicator.orange { background-color: #ffb74d; }\n"
          ".status-indicator.blink { animation: blink 1s infinite alternate; }\n"
          ".status-header { font-size: 24px; margin: 20px 0; padding: 15px; border-radius: 8px; text-align: center; font-weight: bold; border: 2px solid; }\n"
          ".status-table { width: 100%; margin-bottom: 15px; }\n"
          ".status-table th { text-align: left; width: 40%; padding: 8px; background-color: #333; color: #4fc3f7; }\n"
          ".status-table td { padding: 8px; background-color: #2d2d2d; color: #e0e0e0; }\n"
          "@keyframes blink { from { opacity: 0.6; } to { opacity: 1; } }\n"
          "</style>\n";

  // Page header with title
  html += "<div class='page-header'>\n";
  html += "<h1>Device Setup</h1>\n";
  html += "<p style='color: #b0b0b0;'>Version: " + String(DEVICE_VERSION) + "</p>\n";
  html += "</div>\n";

  // Get current status for header
  String statusString = getRoofStatusString();
  String statusClass = "";
  String indicatorClass = "";

  if (statusString == "Open") {
    statusClass = "open";
    indicatorClass = "blue";
  } else if (statusString == "Closed") {
    statusClass = "closed";
    indicatorClass = "green";
  } else if (statusString == "Opening") {
    statusClass = "moving";
    indicatorClass = "blue blink";
  } else if (statusString == "Closing") {
    statusClass = "moving";
    indicatorClass = "green blink";
  } else {
    statusClass = "error";
    indicatorClass = "red blink";
  }

  // Status header (matching home page)
  html += "<div id='mainStatusHeader' class='status-header " + statusClass + "'>\n";
  html += "<span id='mainStatusIndicator' class='status-indicator " + indicatorClass + "'></span> ";
  html += "Roof Status: <span id='mainStatusText'>" + statusString + "</span>";
  html += "</div>\n";

  // Navigation buttons
  html += "<div style='margin: 20px 0;'>\n";
  html += "<a href='/' class='nav-button' style='background-color: #3498db;'>Home</a>\n";
  html += "<a href='/control' class='nav-button' style='background-color: #2ecc71;'>Roof Control</a>\n";
  html += "<a href='/setup' class='nav-button' style='background-color: #3498db;'>Device Setup</a>\n";
  html += "<a href='/wificonfig' class='nav-button' style='background-color: #3498db;'>WiFi Config</a>\n";
  html += "<a href='/update' class='nav-button' style='background-color: #f39c12;'>Update</a>\n";
  html += "</div>\n";
  
  // Status Card
  html += getStatusCard();
  
  // Device Configuration Card (with all toggle switches)
  html += getSwitchConfigCard();
  
  // Park Sensor Configuration Card
  html += getParkSensorConfigCard();
  
  // WiFi Settings Card
  html += getWifiSettingsCard();
  
  // MQTT Settings Card
  html += getMqttSettingsCard();
  
  // System Management Card
  html += getSystemManagementCard();
  
  // Add JavaScript for interactivity
  html += getControlJS();

  // Initialize all toggle labels on page load
  html += "<script>";

  // Add status update function for header
  html += "function updateStatus() {\n";
  html += "  fetch('/api/status')\n";
  html += "    .then(response => response.json())\n";
  html += "    .then(data => {\n";
  html += "      // Update main status header\n";
  html += "      const statusHeader = document.getElementById('mainStatusHeader');\n";
  html += "      const statusIndicator = document.getElementById('mainStatusIndicator');\n";
  html += "      const statusText = document.getElementById('mainStatusText');\n";
  html += "      if (statusHeader && statusIndicator && statusText) {\n";
  html += "        statusText.textContent = data.status;\n";
  html += "        statusHeader.className = 'status-header ';\n";
  html += "        statusIndicator.className = 'status-indicator ';\n";
  html += "        if (data.status === 'Open') {\n";
  html += "          statusHeader.className += 'open';\n";
  html += "          statusIndicator.className += 'blue';\n";
  html += "        } else if (data.status === 'Closed') {\n";
  html += "          statusHeader.className += 'closed';\n";
  html += "          statusIndicator.className += 'green';\n";
  html += "        } else if (data.status === 'Opening') {\n";
  html += "          statusHeader.className += 'moving';\n";
  html += "          statusIndicator.className += 'blue blink';\n";
  html += "        } else if (data.status === 'Closing') {\n";
  html += "          statusHeader.className += 'moving';\n";
  html += "          statusIndicator.className += 'green blink';\n";
  html += "        } else {\n";
  html += "          statusHeader.className += 'error';\n";
  html += "          statusIndicator.className += 'red blink';\n";
  html += "        }\n";
  html += "      }\n";
  html += "    })\n";
  html += "    .catch(error => console.error('Error updating status:', error));\n";
  html += "}\n\n";

  html += "document.addEventListener('DOMContentLoaded', function() {";
  html += "  // Initialize all toggle labels";
  html += "  updateToggleLabel('triggerState', 'triggerStateText', 'HIGH', 'LOW');";
  html += "  updateToggleLabel('swapSwitches', 'swapSwitchesText', 'Swapped', 'Default');";
  html += "  updateToggleLabel('mqttEnabled', 'mqttEnabledText', 'Enabled', 'Disabled');";
  html += "  updateToggleLabel('inverterRelayToggle', 'inverterRelayText', 'ENABLED', 'DISABLED');";
  html += "  updateToggleLabel('inverterSoftPwrToggle', 'inverterSoftPwrText', 'ENABLED', 'DISABLED');";
  html += "  updateToggleLabel('limitSwitchTimeoutEnabledToggle', 'limitSwitchTimeoutEnabledText', 'ENABLED', 'DISABLED');";
  html += "  updateToggleLabel('timeoutEnabledToggle', 'timeoutEnabledText', 'ENABLED', 'DISABLED');";
  html += "  const bypassToggle = document.getElementById('bypassToggle');";
  html += "  const bypassText = document.getElementById('bypassText');";
  html += "  if (bypassToggle && bypassText) {";
  html += "    bypassText.style.color = bypassToggle.checked ? '#f44336' : '#333';";
  html += "  }";
  html += "  // Start status update polling\n";
  html += "  updateStatus();\n";
  html += "  setInterval(updateStatus, 2000);\n";
  html += "});";
  html += "</script>";
  
  html += "</body></html>";

  return html;
}

// Roof Control page (v3)
inline String getRoofControlPage() {
  String html = getPageHeader("Roof Control");

  // Add custom styles matching home page
  html += "<style>\n"
          "body { text-align: center; }\n"
          ".status-card { background-color: #2d2d2d; border-radius: 8px; padding: 20px; margin: 15px 0; box-shadow: 0 4px 6px rgba(0,0,0,0.3); text-align: left; border: 1px solid #404040; }\n"
          ".page-header { margin: 20px 0; }\n"
          ".open { background-color: #1e3a5f; color: #64b5f6; }\n"
          ".closed { background-color: #1e3a2f; color: #81c784; }\n"
          ".moving { background-color: #3a2f1e; color: #ffb74d; }\n"
          ".error { background-color: #3a1e1e; color: #e57373; }\n"
          ".nav-button { display: inline-block; margin: 5px; padding: 10px 20px; background-color: #4fc3f7; color: #000; border-radius: 4px; text-decoration: none; font-weight: bold; }\n"
          ".nav-button:hover { background-color: #81d4fa; text-decoration: none; color: #000; }\n"
          ".status-indicator { display: inline-block; width: 12px; height: 12px; border-radius: 50%; margin-right: 5px; }\n"
          ".status-indicator.green { background-color: #81c784; }\n"
          ".status-indicator.red { background-color: #e57373; }\n"
          ".status-indicator.blue { background-color: #64b5f6; }\n"
          ".status-indicator.orange { background-color: #ffb74d; }\n"
          ".status-indicator.blink { animation: blink 1s infinite alternate; }\n"
          ".status-header { font-size: 24px; margin: 20px 0; padding: 15px; border-radius: 8px; text-align: center; font-weight: bold; border: 2px solid; }\n"
          ".status-table { width: 100%; margin-bottom: 15px; }\n"
          ".status-table th { text-align: left; width: 40%; padding: 8px; background-color: #333; color: #4fc3f7; }\n"
          ".status-table td { padding: 8px; background-color: #2d2d2d; color: #e0e0e0; }\n"
          "@keyframes blink { from { opacity: 0.6; } to { opacity: 1; } }\n"
          "</style>\n";

  // Page header with title
  html += "<div class='page-header'>\n";
  html += "<h1>Roof Control</h1>\n";
  html += "<p style='color: #b0b0b0;'>Version: " + String(DEVICE_VERSION) + " | <span style='color: #81c784;'>Auto-updates every 2 seconds</span></p>\n";
  html += "</div>\n";

  // Get current status for header
  String statusString = getRoofStatusString();
  String statusClass = "";
  String indicatorClass = "";

  if (statusString == "Open") {
    statusClass = "open";
    indicatorClass = "blue";
  } else if (statusString == "Closed") {
    statusClass = "closed";
    indicatorClass = "green";
  } else if (statusString == "Opening") {
    statusClass = "moving";
    indicatorClass = "blue blink";
  } else if (statusString == "Closing") {
    statusClass = "moving";
    indicatorClass = "green blink";
  } else {
    statusClass = "error";
    indicatorClass = "red blink";
  }

  // Status header (matching home page)
  html += "<div id='mainStatusHeader' class='status-header " + statusClass + "'>\n";
  html += "<span id='mainStatusIndicator' class='status-indicator " + indicatorClass + "'></span> ";
  html += "Roof Status: <span id='mainStatusText'>" + statusString + "</span>";
  html += "</div>\n";

  // Navigation buttons
  html += "<div style='margin: 20px 0;'>\n";
  html += "<a href='/' class='nav-button' style='background-color: #3498db;'>Home</a>\n";
  html += "<a href='/control' class='nav-button' style='background-color: #2ecc71;'>Roof Control</a>\n";
  html += "<a href='/setup' class='nav-button' style='background-color: #3498db;'>Device Setup</a>\n";
  html += "<a href='/wificonfig' class='nav-button' style='background-color: #3498db;'>WiFi Config</a>\n";
  html += "<a href='/update' class='nav-button' style='background-color: #f39c12;'>Update</a>\n";
  html += "</div>\n";

  // Current Status Card
  html += "<div class='status-card'>\n";
  html += "<h2>Current Status</h2>\n";
  html += "<table class='status-table'>\n";

  // Roof status - with IDs for dynamic updates (reuse statusClass from above)
  String tableStatusClass = "";
  if (statusString == "Open") {
    tableStatusClass = "status-open";
  } else if (statusString == "Closed") {
    tableStatusClass = "status-closed";
  } else if (statusString == "Opening" || statusString == "Closing") {
    tableStatusClass = "status-moving";
  } else {
    tableStatusClass = "status-error";
  }

  html += "<tr><th>Roof Status</th><td id='roofStatus' class='" + tableStatusClass + "'>" + statusString + "</td></tr>\n";

  // Park sensor status
  html += "<tr><th>Telescope Parked</th><td id='telescopeParked'>";
  html += "<span id='telescopeParkedIndicator' class='status-indicator " + String(telescopeParked ? "green" : "red") + "'></span> ";
  html += "<span id='telescopeParkedText'>" + String(telescopeParked ? "Yes" : "No") + "</span>";
  html += "</td></tr>\n";

  // Bypass state
  html += "<tr><th>Park Sensor Bypass</th><td id='bypassEnabled'>";
  html += "<span id='bypassIndicator' class='status-indicator " + String(bypassParkSensor ? "red blink" : "green") + "'></span> ";
  html += "<span id='bypassText'>" + String(bypassParkSensor ? "<span style='color: #e74c3c; font-weight: bold;'>ENABLED</span>" : "Disabled") + "</span>";
  html += "</td></tr>\n";

  // Limit switch states
  bool openSwitchTriggered = (digitalRead(LIMIT_SWITCH_OPEN_PIN) == TRIGGERED);
  bool closedSwitchTriggered = (digitalRead(LIMIT_SWITCH_CLOSED_PIN) == TRIGGERED);

  html += "<tr><th>Open Limit Switch</th><td id='openLimit'>";
  html += "<span id='openLimitIndicator' class='status-indicator " + String(openSwitchTriggered ? "green" : "red") + "'></span> ";
  html += "<span id='openLimitText'>" + String(openSwitchTriggered ? "Triggered" : "Not Triggered") + "</span>";
  html += "</td></tr>\n";

  html += "<tr><th>Closed Limit Switch</th><td id='closedLimit'>";
  html += "<span id='closedLimitIndicator' class='status-indicator " + String(closedSwitchTriggered ? "green" : "red") + "'></span> ";
  html += "<span id='closedLimitText'>" + String(closedSwitchTriggered ? "Triggered" : "Not Triggered") + "</span>";
  html += "</td></tr>\n";

  // Inverter states
  bool inverterRelay = getInverterRelayState();
  bool inverterACPower = getInverterACPowerState();

  html += "<tr><th>Inverter Relay (K1)</th><td id='inverterRelay'>";
  html += "<span id='inverterRelayIndicator' class='status-indicator " + String(inverterRelay ? "green" : "red") + "'></span> ";
  html += "<span id='inverterRelayText'>" + String(inverterRelay ? "ON" : "OFF") + "</span>";
  html += "</td></tr>\n";

  html += "<tr><th>Inverter AC Power</th><td id='inverterACPower'>";
  html += "<span id='inverterACPowerIndicator' class='status-indicator " + String(inverterACPower ? "green" : "red") + "'></span> ";
  html += "<span id='inverterACPowerText'>" + String(inverterACPower ? "ON" : "OFF") + "</span>";
  html += "</td></tr>\n";

  html += "</table>\n";
  html += "</div>\n";

  // Roof Control Card
  html += "<div class='status-card'>\n";
  html += "<h2>Roof Movement</h2>\n";

  // Add bypass toggle
  html += "<div style='margin: 15px 0; padding: 15px; background-color: #2d2d2d; border-radius: 4px;'>\n";
  html += "<div class='switch-container' style='display: flex; align-items: center; justify-content: center;'>\n";
  html += "<label class='switch'>\n";
  html += "<input type='checkbox' id='bypassToggleControl' class='danger'" + String(bypassParkSensor ? " checked" : "") + " onchange='toggleBypassControl(this.checked)'>\n";
  html += "<span class='slider'></span>\n";
  html += "</label>\n";
  html += "<span class='switch-label' id='bypassLabelControl' style='color: " + String(bypassParkSensor ? "#e57373" : "#e0e0e0") + ";'>\n";
  html += "Bypass Park Sensor <strong>" + String(bypassParkSensor ? "(ENABLED)" : "(DISABLED)") + "</strong><br>\n";
  html += "<small style='color: #e0e0e0;'>Enable to control roof regardless of telescope position</small>\n";
  html += "</span>\n";
  html += "</div>\n";
  html += "</div>\n";

  html += "<div style='text-align: center; margin: 20px 0;'>\n";

  // Determine if buttons should be disabled
  bool buttonDisabled = !bypassParkSensor && !telescopeParked;

  // Intelligent button - uses ASCOM/MQTT logic
  html += "<button id='roofOpenCloseButton' class='btn' onclick='roofOpenClose()' style='background-color: #2ecc71; font-size: 20px; padding: 15px 30px; margin: 5px;'" + String(buttonDisabled ? " disabled" : "") + ">OPEN / CLOSE</button>\n";

  // Manual button - mimics physical button press
  html += "<button id='roofControlButton' class='btn' onclick='roofButtonPress()' style='background-color: #3498db; font-size: 20px; padding: 15px 30px; margin: 5px;'" + String(buttonDisabled ? " disabled" : "") + ">START / STOP</button>\n";

  if (buttonDisabled) {
    html += "<p style='font-size: 14px; color: #e74c3c; margin-top: 10px; font-weight: bold;'>⚠ Telescope not parked - Enable bypass to control roof</p>\n";
  } else {
    html += "<p style='font-size: 14px; color: #b0b0b0; margin-top: 10px;'><strong>START/STOP:</strong> Mimics physical button | <strong>OPEN/CLOSE:</strong> Intelligent control</p>\n";
  }

  html += "</div>\n";
  html += "</div>\n";

  // Inverter Control Card
  html += "<div class='status-card'>\n";
  html += "<h2>Inverter Control</h2>\n";
  html += "<div style='text-align: center; margin: 20px 0;'>\n";
  html += "<button class='btn' onclick='toggleInverterPower()' style='margin: 5px;'>Toggle Power Relay (K1)</button>\n";
  html += "<button class='btn' onclick='sendInverterButton()' style='margin: 5px;'>Press Soft-Power Button (K3)</button>\n";
  html += "</div>\n";
  html += "</div>\n";

  // Add JavaScript for control functions
  html += "<script>\n";
  html += "function toggleInverterPower() {\n";
  html += "  fetch('/inverter_toggle', { method: 'POST' })\n";
  html += "    .then(response => response.text())\n";
  html += "    .then(data => { console.log('Inverter toggle:', data); updateStatus(); })\n";
  html += "    .catch(error => alert('Error: ' + error));\n";
  html += "}\n\n";

  html += "function sendInverterButton() {\n";
  html += "  fetch('/inverter_button', { method: 'POST' })\n";
  html += "    .then(response => response.text())\n";
  html += "    .then(data => { console.log('Inverter button:', data); updateStatus(); })\n";
  html += "    .catch(error => alert('Error: ' + error));\n";
  html += "}\n\n";

  html += "function roofControl(action) {\n";
  html += "  fetch('/roof_control', {\n";
  html += "    method: 'POST',\n";
  html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},\n";
  html += "    body: 'action=' + action\n";
  html += "  })\n";
  html += "    .then(response => response.text())\n";
  html += "    .then(data => { setTimeout(() => location.reload(), 500); })\n";
  html += "    .catch(error => alert('Error: ' + error));\n";
  html += "}\n\n";

  html += "function roofButtonPress() {\n";
  html += "  fetch('/roof_button', { method: 'POST' })\n";
  html += "    .then(response => response.text())\n";
  html += "    .then(data => { console.log('Roof button pressed:', data); })\n";
  html += "    .catch(error => alert('Error: ' + error));\n";
  html += "}\n\n";

  html += "function roofOpenClose() {\n";
  html += "  fetch('/roof_openclose', { method: 'POST' })\n";
  html += "    .then(response => response.text())\n";
  html += "    .then(data => { console.log('Intelligent roof control:', data); })\n";
  html += "    .catch(error => alert('Error: ' + error));\n";
  html += "}\n\n";

  html += "function toggleBypassControl(checked) {\n";
  html += "  fetch('/toggle_bypass', {\n";
  html += "    method: 'POST',\n";
  html += "    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },\n";
  html += "    body: 'bypass=' + checked\n";
  html += "  })\n";
  html += "  .then(response => response.text())\n";
  html += "  .then(data => {\n";
  html += "    console.log(data);\n";
  html += "    const label = document.getElementById('bypassLabelControl');\n";
  html += "    if (label) {\n";
  html += "      label.style.color = checked ? '#e57373' : '#e0e0e0';\n";
  html += "      label.innerHTML = 'Bypass Park Sensor <strong>' + (checked ? '(ENABLED)' : '(DISABLED)') + '</strong><br><small>Enable to control roof regardless of telescope position</small>';\n";
  html += "    }\n";
  html += "    updateStatus(); // Refresh status\n";
  html += "  })\n";
  html += "  .catch(error => {\n";
  html += "    console.error('Error:', error);\n";
  html += "    alert('Error toggling bypass: ' + error);\n";
  html += "  });\n";
  html += "}\n\n";

  // Add real-time status update function
  html += "function updateStatus() {\n";
  html += "  fetch('/api/status')\n";
  html += "    .then(response => response.json())\n";
  html += "    .then(data => {\n";
  html += "      // Update roof status\n";
  html += "      const statusEl = document.getElementById('roofStatus');\n";
  html += "      if (statusEl) {\n";
  html += "        statusEl.textContent = data.status;\n";
  html += "        statusEl.className = '';\n";
  html += "        if (data.status === 'Open') statusEl.className = 'status-open';\n";
  html += "        else if (data.status === 'Closed') statusEl.className = 'status-closed';\n";
  html += "        else if (data.status === 'Opening' || data.status === 'Closing') statusEl.className = 'status-moving';\n";
  html += "        else statusEl.className = 'status-error';\n";
  html += "      }\n\n";

  html += "      // Update telescope parked\n";
  html += "      const tpInd = document.getElementById('telescopeParkedIndicator');\n";
  html += "      const tpText = document.getElementById('telescopeParkedText');\n";
  html += "      if (tpInd && tpText) {\n";
  html += "        tpInd.className = 'status-indicator ' + (data.telescope_parked ? 'green' : 'red');\n";
  html += "        tpText.textContent = data.telescope_parked ? 'Yes' : 'No';\n";
  html += "      }\n\n";

  html += "      // Update bypass status\n";
  html += "      const bypassInd = document.getElementById('bypassIndicator');\n";
  html += "      const bypassText = document.getElementById('bypassText');\n";
  html += "      if (bypassInd && bypassText) {\n";
  html += "        bypassInd.className = 'status-indicator ' + (data.bypass_enabled ? 'red blink' : 'green');\n";
  html += "        bypassText.innerHTML = data.bypass_enabled ? \"<span style='color: #e74c3c; font-weight: bold;'>ENABLED</span>\" : 'Disabled';\n";
  html += "      }\n";
  html += "      // Update bypass toggle checkbox\n";
  html += "      const bypassToggle = document.getElementById('bypassToggleControl');\n";
  html += "      const bypassLabel = document.getElementById('bypassLabelControl');\n";
  html += "      if (bypassToggle) bypassToggle.checked = data.bypass_enabled;\n";
  html += "      if (bypassLabel) {\n";
  html += "        bypassLabel.style.color = data.bypass_enabled ? '#f44336' : '#333';\n";
  html += "        bypassLabel.innerHTML = 'Bypass Park Sensor <strong>' + (data.bypass_enabled ? '(ENABLED)' : '(DISABLED)') + '</strong><br><small>Enable to control roof regardless of telescope position</small>';\n";
  html += "      }\n\n";

  html += "      // Update roof control buttons disabled state\n";
  html += "      const shouldDisable = !data.bypass_enabled && !data.telescope_parked;\n";
  html += "      const roofButton = document.getElementById('roofControlButton');\n";
  html += "      if (roofButton) {\n";
  html += "        roofButton.disabled = shouldDisable;\n";
  html += "        if (shouldDisable) {\n";
  html += "          roofButton.style.opacity = '0.5';\n";
  html += "          roofButton.style.cursor = 'not-allowed';\n";
  html += "        } else {\n";
  html += "          roofButton.style.opacity = '1';\n";
  html += "          roofButton.style.cursor = 'pointer';\n";
  html += "        }\n";
  html += "      }\n";
  html += "      const openCloseButton = document.getElementById('roofOpenCloseButton');\n";
  html += "      if (openCloseButton) {\n";
  html += "        openCloseButton.disabled = shouldDisable;\n";
  html += "        if (shouldDisable) {\n";
  html += "          openCloseButton.style.opacity = '0.5';\n";
  html += "          openCloseButton.style.cursor = 'not-allowed';\n";
  html += "        } else {\n";
  html += "          openCloseButton.style.opacity = '1';\n";
  html += "          openCloseButton.style.cursor = 'pointer';\n";
  html += "        }\n";
  html += "      }\n\n";

  html += "      // Update open limit switch\n";
  html += "      const openInd = document.getElementById('openLimitIndicator');\n";
  html += "      const openText = document.getElementById('openLimitText');\n";
  html += "      if (openInd && openText) {\n";
  html += "        openInd.className = 'status-indicator ' + (data.limit_open ? 'green' : 'red');\n";
  html += "        openText.textContent = data.limit_open ? 'Triggered' : 'Not Triggered';\n";
  html += "      }\n\n";

  html += "      // Update closed limit switch\n";
  html += "      const closedInd = document.getElementById('closedLimitIndicator');\n";
  html += "      const closedText = document.getElementById('closedLimitText');\n";
  html += "      if (closedInd && closedText) {\n";
  html += "        closedInd.className = 'status-indicator ' + (data.limit_closed ? 'green' : 'red');\n";
  html += "        closedText.textContent = data.limit_closed ? 'Triggered' : 'Not Triggered';\n";
  html += "      }\n\n";

  html += "      // Update inverter relay\n";
  html += "      const invRelayInd = document.getElementById('inverterRelayIndicator');\n";
  html += "      const invRelayText = document.getElementById('inverterRelayText');\n";
  html += "      if (invRelayInd && invRelayText) {\n";
  html += "        invRelayInd.className = 'status-indicator ' + (data.inverter_relay ? 'green' : 'red');\n";
  html += "        invRelayText.textContent = data.inverter_relay ? 'ON' : 'OFF';\n";
  html += "      }\n\n";

  html += "      // Update inverter AC power\n";
  html += "      const invACInd = document.getElementById('inverterACPowerIndicator');\n";
  html += "      const invACText = document.getElementById('inverterACPowerText');\n";
  html += "      if (invACInd && invACText) {\n";
  html += "        invACInd.className = 'status-indicator ' + (data.inverter_ac_power ? 'green' : 'red');\n";
  html += "        invACText.textContent = data.inverter_ac_power ? 'ON' : 'OFF';\n";
  html += "      }\n\n";

  html += "      // Update main status header\n";
  html += "      const statusHeader = document.getElementById('mainStatusHeader');\n";
  html += "      const statusIndicator = document.getElementById('mainStatusIndicator');\n";
  html += "      const statusText = document.getElementById('mainStatusText');\n";
  html += "      if (statusHeader && statusIndicator && statusText) {\n";
  html += "        statusText.textContent = data.status;\n";
  html += "        statusHeader.className = 'status-header ';\n";
  html += "        statusIndicator.className = 'status-indicator ';\n";
  html += "        if (data.status === 'Open') {\n";
  html += "          statusHeader.className += 'open';\n";
  html += "          statusIndicator.className += 'blue';\n";
  html += "        } else if (data.status === 'Closed') {\n";
  html += "          statusHeader.className += 'closed';\n";
  html += "          statusIndicator.className += 'green';\n";
  html += "        } else if (data.status === 'Opening') {\n";
  html += "          statusHeader.className += 'moving';\n";
  html += "          statusIndicator.className += 'blue blink';\n";
  html += "        } else if (data.status === 'Closing') {\n";
  html += "          statusHeader.className += 'moving';\n";
  html += "          statusIndicator.className += 'green blink';\n";
  html += "        } else {\n";
  html += "          statusHeader.className += 'error';\n";
  html += "          statusIndicator.className += 'red blink';\n";
  html += "        }\n";
  html += "      }\n";
  html += "    })\n";
  html += "    .catch(error => console.error('Error updating status:', error));\n";
  html += "}\n\n";

  // Start polling on page load
  html += "// Start auto-updating when page loads\n";
  html += "document.addEventListener('DOMContentLoaded', function() {\n";
  html += "  updateStatus(); // Initial update\n";
  html += "  setInterval(updateStatus, 2000); // Update every 2 seconds\n";
  html += "});\n";

  html += "</script>\n";

  html += "</body></html>";

  return html;
}

// WiFi configuration page
inline String getWifiConfigPage() {
  String html = getPageHeader("WiFi Configuration");

  // Add custom styles for the WiFi config page - Dark Theme
  html += "<style>\n"
          "body { text-align: center; }\n"
          ".container { max-width: 600px; margin: 0 auto; background-color: #2d2d2d; padding: 20px; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); border: 1px solid #404040; }\n"
          ".network-list { margin-bottom: 20px; text-align: left; }\n"
          ".network { padding: 10px; margin-bottom: 5px; border: 1px solid #555; border-radius: 4px; cursor: pointer; background-color: #333; color: #e0e0e0; }\n"
          ".network:hover { background-color: #404040; }\n"
          ".back-link { margin-top: 20px; display: inline-block; }\n"
          "</style>\n";

  html += "<div class='container'>";
  html += "<div class='page-header'>\n";
  html += "<h1>WiFi Configuration</h1>\n";
  html += "<p style='color: #b0b0b0;'>Version: " + String(DEVICE_VERSION) + "</p>\n";
  html += "</div>\n";
  
  // Add network scanning functionality
  html += "<div class='network-list'>";
  html += "<p><strong>Select a network or enter details manually:</strong></p>";
  
  // Scan for networks
  int numNetworks = WiFi.scanNetworks();
  if (numNetworks == 0) {
    html += "<p>No WiFi networks found</p>";
  } else {
    for (int i = 0; i < numNetworks; i++) {
      html += "<div class='network' onclick='selectNetwork(\"" + WiFi.SSID(i) + "\")'>";
      html += WiFi.SSID(i) + " (Signal: " + WiFi.RSSI(i) + " dBm, Encryption: " + 
              (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "Open" : "Secured") + ")";
      html += "</div>";
    }
  }
  html += "</div>";
  
  // WiFi Configuration Form
  html += "<form method='post' action='/wificonfig'>";
  html += "<label for='ssid'>WiFi SSID:</label>";
  html += "<input type='text' id='ssid' name='ssid' value='" + String(ssid) + "'>";
  html += "<label for='password'>WiFi Password:</label>";
  html += "<input type='password' id='password' name='password' value=''>";
  html += "<input type='submit' value='Save & Connect'>";
  html += "</form>";
  
  // Add a back link
  html += "<p><a href='/' class='back-link'>Back to Home</a></p>";
  
  // JavaScript
  html += getControlJS();
  
  html += "</div>"; // End container
  html += "</body></html>";
  
  return html;
}

#endif // HTML_TEMPLATES_H