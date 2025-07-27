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
String getWifiConfigPage();
String getWifiSettingsCard();
String getMqttSettingsCard();
String getSwitchConfigCard();
String getSystemManagementCard();
String getStatusCard();
String getParkSensorConfigCard();  // New function for park sensor configuration

// Common CSS styles used across pages
inline String getCommonStyles() {
  String styles = 
    "body { font-family: Arial, sans-serif; margin: 20px; }\n"
    "h1, h2 { color: #2c3e50; }\n"
    "a { color: #3498db; text-decoration: none; }\n"
    "a:hover { text-decoration: underline; }\n"
    ".card { background: #f8f9fa; border-radius: 4px; padding: 15px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }\n"
    "label { display: block; margin-bottom: 5px; font-weight: bold; }\n"
    "input[type=text], input[type=password], input[type=number] { width: 100%; padding: 8px; margin-bottom: 15px; border: 1px solid #ddd; border-radius: 4px; }\n"
    "input[type=submit] { background: #3498db; color: white; border: none; padding: 10px 15px; border-radius: 4px; cursor: pointer; }\n"
    "input[type=submit]:hover { background: #2980b9; }\n"
    "button { background-color: #3498db; color: white; border: none; padding: 10px 15px; margin: 5px; border-radius: 4px; cursor: pointer; }\n"
    "button:hover { background-color: #2980b9; }\n"
    "table { border-collapse: collapse; width: 100%; }\n"
    "table, th, td { border: 1px solid #ddd; }\n"
    "th, td { padding: 8px; text-align: left; }\n"
    "th { background-color: #f2f2f2; }\n"
    ".status-open { color: blue; font-weight: bold; }\n"
    ".status-closed { color: green; font-weight: bold; }\n"
    ".status-moving { color: orange; font-weight: bold; }\n"
    ".status-error { color: darkred; font-weight: bold; }\n"
    ".button-row { display: flex; flex-wrap: wrap; gap: 10px; margin-top: 15px; }\n"
    ".button-primary { background-color: #3498db; }\n"
    ".button-warning { background-color: #f39c12; }\n"
    ".button-danger { background-color: #e74c3c; }\n"
    // Add new styles for telescope status indicator
    ".telescope-status { display: inline-block; margin: 20px auto; text-align: center; }\n"
    ".status-indicator { display: inline-block; width: 24px; height: 24px; border-radius: 50%; margin-right: 10px; vertical-align: middle; }\n"
    ".status-text { display: inline-block; font-weight: bold; vertical-align: middle; }\n"
    ".indicator-red { background-color: #e74c3c; }\n" // Unparked
    ".indicator-green { background-color: #2ecc71; }\n" // Parked
    ".indicator-yellow { background-color: #f1c40f; animation: blink 1s infinite alternate; }\n" // Bypassed
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

// Toggle switch CSS styles
inline String getToggleSwitchStyles() {
  String styles = 
    ".switch {position: relative; display: inline-block; width: 60px; height: 34px;}\n"
    ".switch input {opacity: 0; width: 0; height: 0;}\n"
    ".slider {position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 34px;}\n"
    ".slider:before {position: absolute; content: \"\"; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%;}\n"
    "input:checked + .slider {background-color: #2196F3;}\n"
    "input:focus + .slider {box-shadow: 0 0 1px #2196F3;}\n"
    "input:checked + .slider:before {transform: translateX(26px);}\n"
    "input.danger:checked + .slider {background-color: #f44336;}\n"
    ".switch-label {display: inline-block; vertical-align: middle; margin-left: 10px; font-weight: bold;}\n"
    ".switch-container {margin-bottom: 20px; display: flex; align-items: center;}\n"
    ".toggle-group { display: flex; flex-direction: column; gap: 15px; margin-bottom: 20px; padding: 15px; background-color: #f5f5f5; border-radius: 8px; }\n"
    ".toggle-group h3 { margin-top: 0; color: #2c3e50; }\n"
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
    "<div style='margin-bottom: 20px; padding: 10px; background-color: #f8f9fa; border-radius: 4px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);'>\n"
    "<a href='/' style='margin-right: 10px; padding: 8px 12px; background-color: #3498db; color: white; border-radius: 4px; text-decoration: none;'>Home</a>\n"
    "<a href='/setup' style='margin-right: 10px; padding: 8px 12px; background-color: #3498db; color: white; border-radius: 4px; text-decoration: none;'>Setup</a>\n"
    "<a href='/wificonfig' style='margin-right: 10px; padding: 8px 12px; background-color: #3498db; color: white; border-radius: 4px; text-decoration: none;'>WiFi Config</a>\n"
    "<a href='http://" + WiFi.localIP().toString() + ":" + String(ALPACA_PORT) + "/setup/v1/dome/0/setup' style='margin-right: 10px; padding: 8px 12px; background-color: #3498db; color: white; border-radius: 4px; text-decoration: none;'>ASCOM Controls</a>\n"
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
    "    \n"
    "    fetch('/set_pins', {\n"
    "      method: 'POST',\n"
    "      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },\n"
    "      body: 'triggerState=' + triggerState + '&swapSwitches=' + swapSwitches + '&mqttEnabled=' + mqttEnabled\n"
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
    "    document.getElementById('bypassText').parentElement.style.color = checked ? '#f44336' : '#333';\n"
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
  
  // Add custom styles for home page
  html += "<style>\n"
          "body { text-align: center; }\n"
          ".status-card { background-color: #f8f9fa; border-radius: 8px; padding: 20px; margin: 15px 0; box-shadow: 0 2px 4px rgba(0,0,0,0.1); text-align: left; }\n"
          ".open { background-color: #e6f2ff; color: #0047ab; }\n"
          ".closed { background-color: #e6ffe6; color: green; }\n"
          ".moving { background-color: #fff6e6; color: orange; }\n"
          ".error { background-color: #ffcccc; color: darkred; }\n"
          ".ap-mode-banner { background-color: #ffe66d; color: #5c4d00; padding: 10px; border-radius: 5px; margin: 10px 0; font-weight: bold; }\n"
          ".nav-button { display: inline-block; margin: 5px; padding: 8px 15px; background-color: #3498db; color: white; border-radius: 4px; text-decoration: none; }\n"
          ".nav-button:hover { background-color: #2980b9; text-decoration: none; color: white; }\n"
          ".status-indicator { display: inline-block; width: 12px; height: 12px; border-radius: 50%; margin-right: 5px; }\n"
          ".status-indicator.green { background-color: #2ecc71; }\n"
          ".status-indicator.red { background-color: #e74c3c; }\n"
          ".status-indicator.blue { background-color: #3498db; }\n"
          ".status-indicator.orange { background-color: #f39c12; }\n"
          ".status-indicator.blink { animation: blink 1s infinite alternate; }\n"
          ".status-header { font-size: 24px; margin: 20px 0; padding: 15px; border-radius: 8px; text-align: center; }\n"
          ".status-table { width: 100%; margin-bottom: 15px; }\n"
          ".status-table th { text-align: left; width: 40%; padding: 8px; background-color: #f2f2f2; }\n"
          ".status-table td { padding: 8px; }\n"
          ".mqtt-json { background-color: #f5f5f5; padding: 10px; border-radius: 4px; font-family: monospace; white-space: pre-wrap; font-size: 0.9em; max-height: 150px; overflow-y: auto; }\n"
          "@keyframes blink { from { opacity: 0.6; } to { opacity: 1; } }\n"
          "</style>\n";
          
  html += "<meta http-equiv='refresh' content='10'>\n"; // Auto-refresh every 10 seconds
  
  html += "<h1>ESP32 Roll-Off Roof Controller</h1>\n"
          "<p>Version: " + String(DEVICE_VERSION) + "</p>\n";
  
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
  html += "<div class='status-header " + statusClass + "'>\n";
  html += "<span class='status-indicator " + indicatorClass + "'></span> ";
  html += "Roof Status: " + statusString;
  html += "</div>\n";
  
  // Navigation buttons
  html += "<div style='margin: 20px 0;'>\n";
  html += "<a href='/setup' class='nav-button' style='background-color: #3498db;'>Device Setup</a>\n";
  html += "<a href='/wificonfig' class='nav-button' style='background-color: #3498db;'>WiFi Config</a>\n";
  html += "<a href='http://" + WiFi.localIP().toString() + ":" + String(ALPACA_PORT) + "/setup/v1/dome/0/setup' class='nav-button' style='background-color: #3498db;'>ASCOM Controls</a>\n";
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
  html += " <small style='margin-left: 10px; color: #666;'><a href='/setup'>Configure bypass in setup</a></small>";
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
  
  // Simple JavaScript to handle page refresh
  html += "<script>\n";
  html += "document.addEventListener('DOMContentLoaded', function() {\n";
  html += "  console.log('Page loaded, auto-refresh enabled');\n";
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
  html += "<label for='mqttTopicPrefix'>MQTT Topic Prefix:</label>";
  html += "<input type='text' id='mqttTopicPrefix' name='mqttTopicPrefix' value='" + String(mqttTopicPrefix) + "'>";
  html += "<p style='font-size: 0.8em; color: #666;'>Status and command topics will be created as [prefix]/status and [prefix]/command</p>";
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
  html += "<span class='switch-label' style='color: " + String(bypassParkSensor ? "#f44336" : "#333") + ";'>";
  html += "Bypass Telescope Park Sensor <strong id='bypassText'>(" + String(bypassParkSensor ? "ENABLED" : "DISABLED") + ")</strong><br>";
  html += "<small>Warning: When enabled, allows roof to move regardless of telescope position</small><br>";
  html += "<small style='color: #e67e22;'><strong>Note:</strong> This setting is not retained between restarts. Use a physical jumper on the PARK terminals if no sensor is installed.</small>";
  html += "</span>";
  html += "</div>";
  
  html += "</div>"; // End toggle-row
  html += "</div>"; // End toggle-group
  
  // Current configuration
  html += "<div style='margin-top: 10px; padding: 10px; background-color: #f0f0f0; border-radius: 4px;'>";
  html += "<p><strong>Current Configuration:</strong><br>";
  html += "Trigger state: " + String(TRIGGERED == HIGH ? "HIGH" : "LOW") + "<br>";
  html += "Open switch pin: " + String(LIMIT_SWITCH_OPEN_PIN) + "<br>";
  html += "Closed switch pin: " + String(LIMIT_SWITCH_CLOSED_PIN) + "<br>";
  html += "Park sensor bypass: " + String(bypassParkSensor ? "Enabled" : "Disabled") + "</p>";
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
  
  html += "<div style='margin-bottom: 20px;'>";
  html += "<input type='radio' id='parkTypePhysical' name='parkType' value='0'" + 
          String(parkSensorType == PARK_SENSOR_PHYSICAL ? " checked" : "") + ">";
  html += "<label for='parkTypePhysical' style='margin-left: 5px; margin-right: 20px;'>Physical Sensor Only</label>";
  
  html += "<input type='radio' id='parkTypeUDP' name='parkType' value='1'" + 
          String(parkSensorType == PARK_SENSOR_UDP ? " checked" : "") + ">";
  html += "<label for='parkTypeUDP' style='margin-left: 5px; margin-right: 20px;'>UDP Sensors Only</label>";
  
  html += "<input type='radio' id='parkTypeBoth' name='parkType' value='2'" + 
          String(parkSensorType == PARK_SENSOR_BOTH ? " checked" : "") + ">";
  html += "<label for='parkTypeBoth' style='margin-left: 5px;'>Both (AND Logic)</label>";
  html += "</div>";
  html += "</div>";
  
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
  
  html += "<h1>ESP32 Roll-Off Roof Controller</h1>";
  html += "<p>Version: " + String(DEVICE_VERSION) + "</p>";
  
  // Add navigation
  html += getNavBar();
  
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
  html += "document.addEventListener('DOMContentLoaded', function() {";
  html += "  // Initialize all toggle labels";
  html += "  updateToggleLabel('triggerState', 'triggerStateText', 'HIGH', 'LOW');";
  html += "  updateToggleLabel('swapSwitches', 'swapSwitchesText', 'Swapped', 'Default');";
  html += "  updateToggleLabel('mqttEnabled', 'mqttEnabledText', 'Enabled', 'Disabled');";
  html += "  const bypassToggle = document.getElementById('bypassToggle');";
  html += "  const bypassText = document.getElementById('bypassText');";
  html += "  if (bypassToggle && bypassText) {";
  html += "    bypassText.style.color = bypassToggle.checked ? '#f44336' : '#333';";
  html += "  }";
  html += "});";
  html += "</script>";
  
  html += "</body></html>";
  
  return html;
}

// WiFi configuration page
inline String getWifiConfigPage() {
  String html = getPageHeader("WiFi Configuration");
  
  // Add custom styles for the WiFi config page
  html += "<style>\n"
          "body { text-align: center; background-color: #f0f8ff; }\n"
          ".container { max-width: 500px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }\n"
          ".network-list { margin-bottom: 20px; text-align: left; }\n"
          ".network { padding: 10px; margin-bottom: 5px; border: 1px solid #ddd; border-radius: 4px; cursor: pointer; }\n"
          ".network:hover { background-color: #f5f5f5; }\n"
          ".back-link { margin-top: 20px; display: inline-block; }\n"
          "</style>\n";
  
  html += "<div class='container'>";
  html += "<h1>WiFi Configuration</h1>";
  
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