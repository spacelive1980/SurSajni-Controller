/*
 * Web Server Handler Functions
 * This file contains all the web server route handlers for the Sursajni Controller
 * 
 * Note: SPIFFS.h is included in the main .ino file
 */

// ===== WEB SERVER SETUP =====
void setupWebServer() {
  // Define routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/get_settings", HTTP_GET, handleGetSettings);
  server.on("/update_all_settings", HTTP_POST, handleUpdateSettings);
  server.on("/toggle_pump", HTTP_POST, handleTogglePump);
  server.on("/reset_dry_run", HTTP_POST, handleResetDryRun);
  server.on("/reset_sensor_error", HTTP_POST, handleResetSensorError);
  server.on("/factory_reset", HTTP_POST, handleFactoryReset);
  server.on("/get_live_data", HTTP_GET, handleGetLiveData);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Web server started");
  Serial.println("Access points:");
  Serial.printf("  AP Mode: http://%s\n", WiFi.softAPIP().toString().c_str());
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("  Station Mode: http://%s\n", WiFi.localIP().toString().c_str());
  }
}

// Helper function to check authentication
bool checkAuth() {
  if (!server.authenticate(settings.httpUsername, settings.httpPassword)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

// ===== ROOT HANDLER =====
void handleRoot() {
  if (!checkAuth()) return;
  
  // Read the index.html from the parent directory
  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    // If SPIFFS not available, serve a basic page
    String html = F("<!DOCTYPE html><html><head><title>Sursajni Controller</title></head>");
    html += F("<body><h1>Sursajni Controller ESP32</h1>");
    html += F("<p>Upload index.html to SPIFFS or access via the main repository file.</p>");
    html += F("<p><a href='/get_settings'>View Settings JSON</a></p>");
    html += F("</body></html>");
    server.send(200, "text/html", html);
    return;
  }
  
  server.streamFile(file, "text/html");
  file.close();
}

// ===== GET SETTINGS HANDLER =====
void handleGetSettings() {
  if (!checkAuth()) return;
  
  DateTime now = rtc.now();
  
  // Build JSON response
  String json = "{";
  
  // System info
  json += "\"firmwareVersion\":\"1.0.0-ESP32\",";
  json += "\"deviceId\":\"" + deviceId + "\",";
  json += "\"macAddress\":\"" + macAddress + "\",";
  json += "\"chipInfo\":\"" + String(ESP.getChipModel()) + "\",";
  json += "\"freeHeap\":\"" + String(ESP.getFreeHeap()) + " bytes\",";
  json += "\"uptime\":\"" + String(millis() / 1000) + " seconds\",";
  
  // Current status
  json += "\"pumpRunning\":" + String(pumpRunning ? "true" : "false") + ",";
  json += "\"currentTankLevel\":" + String(currentTankLevel) + ",";
  json += "\"batteryVoltage\":" + String(batteryVoltage, 2) + ",";
  json += "\"flowOk\":" + String(flowOk ? "true" : "false") + ",";
  json += "\"dryRunError\":" + String(dryRunError ? "true" : "false") + ",";
  json += "\"heartbeatExpired\":" + String(heartbeatExpired ? "true" : "false") + ",";
  json += "\"signalStrength\":" + String(signalStrength) + ",";
  json += "\"nextSchedule\":\"" + nextSchedule + "\",";
  
  // Sensor error
  json += "\"sensorError\":";
  switch (sensorError) {
    case SENSOR_ERROR_NONE: json += "\"SENSOR_ERROR_NONE\""; break;
    case SENSOR_ERROR_TANK_LEVEL: json += "\"SENSOR_ERROR_TANK_LEVEL\""; break;
    case SENSOR_ERROR_FLOW: json += "\"SENSOR_ERROR_FLOW\""; break;
    case SENSOR_ERROR_TURBIDITY: json += "\"SENSOR_ERROR_TURBIDITY\""; break;
    case SENSOR_ERROR_LORA: json += "\"SENSOR_ERROR_LORA\""; break;
    default: json += "\"SENSOR_ERROR_UNKNOWN\""; break;
  }
  json += ",";
  
  // Settings - System Profile
  json += "\"systemProfile\":" + String(settings.systemProfile) + ",";
  
  // Settings - Pump
  json += "\"pumpMode\":" + String(settings.pumpMode) + ",";
  json += "\"startSensorLevel\":" + String(settings.startSensorLevel) + ",";
  json += "\"endSensorLevel\":" + String(settings.endSensorLevel) + ",";
  json += "\"scheduleDurationMin\":" + String(settings.scheduleDurationMin) + ",";
  json += "\"primingTimeMs\":" + String(settings.primingTimeMs) + ",";
  json += "\"forceScheduledRun\":" + String(settings.forceScheduledRun ? "true" : "false") + ",";
  json += "\"manualModeSafetyLogicEnabled\":" + String(settings.manualModeSafetyLogicEnabled ? "true" : "false") + ",";
  
  // Settings - Safety
  json += "\"dryRunLogicEnabled\":" + String(settings.dryRunLogicEnabled ? "true" : "false") + ",";
  json += "\"dryRunDelayMin\":" + String(settings.dryRunDelayMin) + ",";
  json += "\"retryLogicEnabled\":" + String(settings.retryLogicEnabled ? "true" : "false") + ",";
  json += "\"dryRunAttempts\":" + String(settings.dryRunAttempts) + ",";
  json += "\"retryIntervalMin\":" + String(settings.retryIntervalMin) + ",";
  json += "\"sensorErrorBypassEnabled\":" + String(settings.sensorErrorBypassEnabled ? "true" : "false") + ",";
  json += "\"autoResetDryRunOnWater\":" + String(settings.autoResetDryRunOnWater ? "true" : "false") + ",";
  json += "\"waterPresenceThreshold\":" + String(settings.waterPresenceThreshold) + ",";
  json += "\"waterSensingStartDelaySec\":" + String(settings.waterSensingStartDelaySec) + ",";
  json += "\"waterSensingStopDelaySec\":" + String(settings.waterSensingStopDelaySec) + ",";
  
  // Settings - Buzzer
  json += "\"buzzerEnabled\":" + String(settings.buzzerEnabled ? "true" : "false") + ",";
  json += "\"buzzerVolume\":" + String(settings.buzzerVolume) + ",";
  json += "\"dryRunBeepStyle\":\"" + getBeepStyleName(settings.dryRunBeepStyle) + "\",";
  json += "\"sensorErrorBeepStyle\":\"" + getBeepStyleName(settings.sensorErrorBeepStyle) + "\",";
  json += "\"lowBatteryBeepStyle\":\"" + getBeepStyleName(settings.lowBatteryBeepStyle) + "\",";
  json += "\"heartbeatExpiredBeepStyle\":\"" + getBeepStyleName(settings.heartbeatExpiredBeepStyle) + "\",";
  json += "\"tankEmptyBeepStyle\":\"" + getBeepStyleName(settings.tankEmptyBeepStyle) + "\",";
  json += "\"tankFullBeepStyle\":\"" + getBeepStyleName(settings.tankFullBeepStyle) + "\",";
  json += "\"dirtyWaterBeepStyle\":\"" + getBeepStyleName(settings.dirtyWaterBeepStyle) + "\",";
  
  // Settings - Turbidity
  json += "\"turbidityBypassEnabled\":" + String(settings.turbidityBypassEnabled ? "true" : "false") + ",";
  json += "\"turbidityTimeoutSec\":" + String(settings.turbidityTimeoutSec) + ",";
  json += "\"turbidityLimit\":" + String(settings.turbidityLimit) + ",";
  json += "\"calibratedCleanValue\":" + String(settings.calibratedCleanValue) + ",";
  json += "\"calibratedDirtyValue\":" + String(settings.calibratedDirtyValue) + ",";
  
  // Settings - Display
  json += "\"oledTheme\":" + String(settings.oledTheme) + ",";
  json += "\"webTheme\":" + String(settings.webTheme) + ",";
  json += "\"webUiRefreshIntervalSec\":" + String(settings.webUiRefreshIntervalSec) + ",";
  json += "\"currentFontIndex\":" + String(settings.currentFontIndex) + ",";
  json += "\"oledShowTime\":" + String(settings.oledShowTime ? "true" : "false") + ",";
  json += "\"oledShowDate\":" + String(settings.oledShowDate ? "true" : "false") + ",";
  json += "\"oledShowDay\":" + String(settings.oledShowDay ? "true" : "false") + ",";
  json += "\"oledShowBattery\":" + String(settings.oledShowBattery ? "true" : "false") + ",";
  
  // Settings - LoRa
  json += "\"loraPowerOptimizationEnabled\":" + String(settings.loraPowerOptimizationEnabled ? "true" : "false") + ",";
  json += "\"transmitterTxPower\":" + String(settings.transmitterTxPower) + ",";
  json += "\"transmitterAckAttempts\":" + String(settings.transmitterAckAttempts) + ",";
  
  // RTC
  json += "\"rtc\":{";
  json += "\"year\":" + String(now.year()) + ",";
  json += "\"month\":" + String(now.month()) + ",";
  json += "\"day\":" + String(now.day()) + ",";
  json += "\"hour\":" + String(now.hour()) + ",";
  json += "\"minute\":" + String(now.minute()) + ",";
  json += "\"second\":" + String(now.second());
  json += "},";
  
  // Schedules
  json += "\"schedules\":[";
  for (int i = 0; i < 10; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"hour\":" + String(settings.schedules[i].hour) + ",";
    json += "\"minute\":" + String(settings.schedules[i].minute) + ",";
    json += "\"repeatType\":\"" + getRepeatTypeName(settings.schedules[i].repeatType) + "\",";
    json += "\"year\":" + String(settings.schedules[i].year) + ",";
    json += "\"month\":" + String(settings.schedules[i].month) + ",";
    json += "\"day\":" + String(settings.schedules[i].day);
    json += "}";
  }
  json += "]";
  
  json += "}";
  
  server.send(200, "application/json", json);
}

// Helper functions for enum to string conversion
String getBeepStyleName(uint8_t style) {
  switch (style) {
    case BEEP_SILENT: return "SILENT";
    case BEEP_ALERT: return "ALERT";
    case BEEP_WARNING: return "WARNING";
    case BEEP_PULSE: return "PULSE";
    case BEEP_LONG: return "LONG";
    case BEEP_SPARROW: return "SPARROW";
    default: return "UNKNOWN";
  }
}

String getRepeatTypeName(uint8_t type) {
  switch (type) {
    case REPEAT_EVERY_DAY: return "EVERY_DAY";
    case REPEAT_ODD_DAY: return "ODD_DAY";
    case REPEAT_EVEN_DAY: return "EVEN_DAY";
    case REPEAT_NO_REPEAT: return "NO_REPEAT";
    case REPEAT_SPECIFIC_DATE: return "SPECIFIC_DATE";
    default: return "UNKNOWN";
  }
}

uint8_t getBeepStyleFromName(String name) {
  if (name == "SILENT") return BEEP_SILENT;
  if (name == "ALERT") return BEEP_ALERT;
  if (name == "WARNING") return BEEP_WARNING;
  if (name == "PULSE") return BEEP_PULSE;
  if (name == "LONG") return BEEP_LONG;
  if (name == "SPARROW") return BEEP_SPARROW;
  return BEEP_SILENT;
}

uint8_t getRepeatTypeFromName(String name) {
  if (name == "EVERY_DAY") return REPEAT_EVERY_DAY;
  if (name == "ODD_DAY") return REPEAT_ODD_DAY;
  if (name == "EVEN_DAY") return REPEAT_EVEN_DAY;
  if (name == "NO_REPEAT") return REPEAT_NO_REPEAT;
  if (name == "SPECIFIC_DATE") return REPEAT_SPECIFIC_DATE;
  return REPEAT_NO_REPEAT;
}

// ===== UPDATE SETTINGS HANDLER =====
void handleUpdateSettings() {
  if (!checkAuth()) return;
  
  // Parse form data
  for (int i = 0; i < server.args(); i++) {
    String argName = server.argName(i);
    String argValue = server.arg(i);
    
    // Handle special actions
    if (argName == "action") {
      if (argValue == "clear_schedules") {
        for (int j = 0; j < 10; j++) {
          settings.schedules[j].hour = 0;
          settings.schedules[j].minute = 0;
          settings.schedules[j].year = 0;
          settings.schedules[j].month = 0;
          settings.schedules[j].day = 0;
          settings.schedules[j].repeatType = REPEAT_NO_REPEAT;
        }
      } else if (argValue == "clear_schedule") {
        int index = server.arg("index").toInt();
        if (index >= 0 && index < 10) {
          settings.schedules[index].hour = 0;
          settings.schedules[index].minute = 0;
          settings.schedules[index].year = 0;
          settings.schedules[index].month = 0;
          settings.schedules[index].day = 0;
          settings.schedules[index].repeatType = REPEAT_NO_REPEAT;
        }
      }
      continue;
    }
    
    // System Profile
    if (argName == "systemProfile") settings.systemProfile = argValue.toInt();
    
    // Pump settings
    if (argName == "pumpMode") settings.pumpMode = argValue.toInt();
    if (argName == "startSensorLevel") settings.startSensorLevel = argValue.toInt();
    if (argName == "endSensorLevel") settings.endSensorLevel = argValue.toInt();
    if (argName == "scheduleDurationMin") settings.scheduleDurationMin = argValue.toInt();
    if (argName == "primingTimeMs") settings.primingTimeMs = argValue.toInt();
    if (argName == "forceScheduledRun") settings.forceScheduledRun = (argValue == "1");
    if (argName == "manualModeSafetyLogicEnabled") settings.manualModeSafetyLogicEnabled = (argValue == "1");
    
    // Safety settings
    if (argName == "dryRunLogicEnabled") settings.dryRunLogicEnabled = (argValue == "1");
    if (argName == "dryRunDelayMin") settings.dryRunDelayMin = argValue.toInt();
    if (argName == "retryLogicEnabled") settings.retryLogicEnabled = (argValue == "1");
    if (argName == "dryRunAttempts") settings.dryRunAttempts = argValue.toInt();
    if (argName == "retryIntervalMin") settings.retryIntervalMin = argValue.toInt();
    if (argName == "sensorErrorBypassEnabled") settings.sensorErrorBypassEnabled = (argValue == "1");
    if (argName == "autoResetDryRunOnWater") settings.autoResetDryRunOnWater = (argValue == "1");
    if (argName == "waterPresenceThreshold") settings.waterPresenceThreshold = argValue.toInt();
    if (argName == "waterSensingStartDelaySec") settings.waterSensingStartDelaySec = argValue.toInt();
    if (argName == "waterSensingStopDelaySec") settings.waterSensingStopDelaySec = argValue.toInt();
    
    // Buzzer settings
    if (argName == "buzzerEnabled") settings.buzzerEnabled = (argValue == "1");
    if (argName == "buzzerVolume") settings.buzzerVolume = argValue.toInt();
    if (argName == "dryRunBeepStyle") settings.dryRunBeepStyle = getBeepStyleFromName(argValue);
    if (argName == "sensorErrorBeepStyle") settings.sensorErrorBeepStyle = getBeepStyleFromName(argValue);
    if (argName == "lowBatteryBeepStyle") settings.lowBatteryBeepStyle = getBeepStyleFromName(argValue);
    if (argName == "heartbeatExpiredBeepStyle") settings.heartbeatExpiredBeepStyle = getBeepStyleFromName(argValue);
    if (argName == "tankEmptyBeepStyle") settings.tankEmptyBeepStyle = getBeepStyleFromName(argValue);
    if (argName == "tankFullBeepStyle") settings.tankFullBeepStyle = getBeepStyleFromName(argValue);
    if (argName == "dirtyWaterBeepStyle") settings.dirtyWaterBeepStyle = getBeepStyleFromName(argValue);
    
    // Turbidity settings
    if (argName == "turbidityBypassEnabled") settings.turbidityBypassEnabled = (argValue == "1");
    if (argName == "turbidityTimeoutSec") settings.turbidityTimeoutSec = argValue.toInt();
    if (argName == "turbidityLimit") settings.turbidityLimit = argValue.toInt();
    if (argName == "calibratedCleanValue") settings.calibratedCleanValue = argValue.toInt();
    if (argName == "calibratedDirtyValue") settings.calibratedDirtyValue = argValue.toInt();
    
    // Display settings
    if (argName == "oledTheme") settings.oledTheme = argValue.toInt();
    if (argName == "webTheme") settings.webTheme = argValue.toInt();
    if (argName == "webUiRefreshIntervalSec") settings.webUiRefreshIntervalSec = argValue.toInt();
    if (argName == "displayFontIndex") settings.currentFontIndex = argValue.toInt();
    if (argName == "oledShowTime") settings.oledShowTime = (argValue == "1");
    if (argName == "oledShowDate") settings.oledShowDate = (argValue == "1");
    if (argName == "oledShowDay") settings.oledShowDay = (argValue == "1");
    if (argName == "oledShowBattery") settings.oledShowBattery = (argValue == "1");
    
    // LoRa settings
    if (argName == "loraPowerOptimizationEnabled") settings.loraPowerOptimizationEnabled = (argValue == "1");
    if (argName == "transmitterTxPower") settings.transmitterTxPower = argValue.toInt();
    if (argName == "transmitterAckAttempts") settings.transmitterAckAttempts = argValue.toInt();
    
    // Security
    if (argName == "httpPassword" && argValue.length() > 0) {
      strncpy(settings.httpPassword, argValue.c_str(), sizeof(settings.httpPassword) - 1);
    }
    
    // RTC
    if (argName == "rtcYear" || argName == "rtcMonth" || argName == "rtcDay" ||
        argName == "rtcHour" || argName == "rtcMinute" || argName == "rtcSecond") {
      DateTime now = rtc.now();
      int year = now.year();
      int month = now.month();
      int day = now.day();
      int hour = now.hour();
      int minute = now.minute();
      int second = now.second();
      
      if (argName == "rtcYear") year = argValue.toInt();
      if (argName == "rtcMonth") month = argValue.toInt();
      if (argName == "rtcDay") day = argValue.toInt();
      if (argName == "rtcHour") hour = argValue.toInt();
      if (argName == "rtcMinute") minute = argValue.toInt();
      if (argName == "rtcSecond") second = argValue.toInt();
      
      rtc.adjust(DateTime(year, month, day, hour, minute, second));
    }
    
    // Schedules
    for (int j = 0; j < 10; j++) {
      String prefix = "sched" + String(j);
      if (argName == prefix + "Hour") settings.schedules[j].hour = argValue.toInt();
      if (argName == prefix + "Minute") settings.schedules[j].minute = argValue.toInt();
      if (argName == prefix + "RepeatType") settings.schedules[j].repeatType = getRepeatTypeFromName(argValue);
      if (argName == prefix + "Year") settings.schedules[j].year = argValue.toInt();
      if (argName == prefix + "Month") settings.schedules[j].month = argValue.toInt();
      if (argName == prefix + "Day") settings.schedules[j].day = argValue.toInt();
    }
  }
  
  // Save settings
  saveSettings();
  
  server.send(200, "text/plain", "Settings updated successfully");
}

// ===== TOGGLE PUMP HANDLER =====
void handleTogglePump() {
  if (!checkAuth()) return;
  
  if (pumpRunning) {
    stopPump();
    server.send(200, "text/plain", "Pump stopped");
  } else {
    startPump();
    server.send(200, "text/plain", "Pump started");
  }
}

// ===== RESET DRY RUN HANDLER =====
void handleResetDryRun() {
  if (!checkAuth()) return;
  
  dryRunError = false;
  remainingAttempts = settings.dryRunAttempts;
  server.send(200, "text/plain", "Dry run error reset");
}

// ===== RESET SENSOR ERROR HANDLER =====
void handleResetSensorError() {
  if (!checkAuth()) return;
  
  sensorError = SENSOR_ERROR_NONE;
  server.send(200, "text/plain", "Sensor error reset");
}

// ===== FACTORY RESET HANDLER =====
void handleFactoryReset() {
  if (!checkAuth()) return;
  
  server.send(200, "text/plain", "Factory reset initiated. Device will restart.");
  delay(1000);
  factoryReset();
}

// ===== GET LIVE DATA HANDLER =====
void handleGetLiveData() {
  if (!checkAuth()) return;
  
  String json = "{";
  json += "\"turbidityValue\":" + String(turbidityValue) + ",";
  json += "\"tankLevel\":" + String(currentTankLevel) + ",";
  json += "\"batteryVoltage\":" + String(batteryVoltage, 2) + ",";
  json += "\"pumpRunning\":" + String(pumpRunning ? "true" : "false") + ",";
  json += "\"flowOk\":" + String(flowOk ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

// ===== NOT FOUND HANDLER =====
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: " + server.uri() + "\n";
  message += "Method: " + String((server.method() == HTTP_GET) ? "GET" : "POST") + "\n";
  message += "Arguments: " + String(server.args()) + "\n";
  
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  
  server.send(404, "text/plain", message);
}
