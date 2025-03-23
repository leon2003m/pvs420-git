//libraries
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Preferences.h>

//Global preferences
Preferences prefs;


// Wi-Fi Settings (mutable, for Preferences)
String wifi_ssid = "PVS420";
String wifi_password = "";  // Empty = open
IPAddress IP_esp(10, 4, 20, 1);
IPAddress subnet_mask(255, 255, 255, 0);



// OTA Configuration
const char* OTA_HOSTNAME = "ESP32-C6-OTA";
String OTA_password = "1234";

//telnet client
WiFiClient telnetClient;

// Web Server setings
WebServer server(80);

//Zoomlevel
float currentZoom = 1;  // Default zoom 1.0x

// Log Buffer
String logBuffer = "";  // Non-persistent log storage
bool debugMode = true;  // Set to false to disable debug logging

//awake time
unsigned long startTime = millis();


// Serial Communication Pins
#define RX_PIN 17  // ESP32-C6 Serial1 RX
#define TX_PIN 16  // ESP32-C6 Serial1 TX

//Input Pins
#define MIDDLE_PIN GPIO_NUM_0
#define WIFI_PIN GPIO_NUM_1
#define UP_PIN GPIO_NUM_2
#define DOWN_PIN GPIO_NUM_21
#define LEFT_PIN GPIO_NUM_22
#define RIGHT_PIN GPIO_NUM_23

//Vibration Motor
#define Vibe 19

// Variables will change:
int ledState = HIGH;        // the current state of the output pin
int buttonState;            // the current reading from the input pin
int lastButtonState = LOW;  // the previous reading from the input pin

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

// View Modes
int currentMode = 8;  // Default mode General Mode
struct Mode {
  const char* name;
  const uint8_t command[23];
};


const Mode modes[] = {
  { "White Hot", { 0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x03, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x54, 0x6D } },
  { "Black Hot", { 0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x03, 0x45, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x69, 0x6F } },
  { "LOW Temp Highlight", { 0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x04, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC5, 0x65 } },
  { "LOW Contrast", { 0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x04, 0x42, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2F, 0x63 } },
  { "High Contrast", { 0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x04, 0x42, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x68 } },
  { "Highlight", { 0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x04, 0x42, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x6B } },
  { "Outline Mode", { 0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x04, 0x45, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF7, 0xAB } },
  { "General Mode", { 0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x04, 0x42, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x60 } }
};
const int modeCount = sizeof(modes) / sizeof(modes[0]);

// Shutter Serial Comands

const uint8_t activate_shutter_control[23] = { 0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x02, 0x41, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x3D };
const uint8_t temperature_intervals[23] = { 0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x02, 0x42, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x93, 0xB1 };
const uint8_t maximal_time_interval[23] = { 0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x02, 0x42, 0x00, 0x02, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0xAC };
const uint8_t save_parameters[23] = { 0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x10, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA9, 0xFB };

//other serial commands

//Setup

void serial_setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
}

void wifi_setup() {
  WiFi.mode(WIFI_AP);  // Enable SoftAP
  WiFi.softAPConfig(IP_esp, IP_esp, subnet_mask);

  if (wifi_password.length() > 0) {
    WiFi.softAP(wifi_ssid.c_str(), wifi_password.c_str());
  } else {
    WiFi.softAP(wifi_ssid.c_str());  // Open network
  }

  if (WiFi.softAPgetStationNum() >= 0) {
    logEvent("Wi-Fi started successfully!");
  }
}


void webserver_setup() {
  server.begin();
  server.on("/", HTTP_GET, handleRoot);
  server.on("/mode", handleModeChange);
  server.on("/zoom", handleZoomChange);
  server.on("/logs", handleLogs);
  server.begin();
  logEvent("Web Server Started - Use IP: " + IP_esp.toString());
}

void OTA_setup() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_password.c_str());


  ArduinoOTA.onStart([]() {
    logEvent("OTA Update Started...");
  });

  ArduinoOTA.onEnd([]() {
    logEvent("OTA Update Complete!");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    logEvent("OTA Error [" + String(error) + "]");
  });

  ArduinoOTA.begin();
}

void setup_shutter_control() {
  delay(100);
  sendCommand(activate_shutter_control);
  delay(100);
  sendCommand(temperature_intervals);
  delay(100);
  sendCommand(maximal_time_interval);
  delay(100);
  sendCommand(save_parameters);
};

void setup_buttons() {
  pinMode(MIDDLE_PIN, INPUT_PULLUP);
  pinMode(WIFI_PIN, INPUT_PULLUP);
  pinMode(UP_PIN, INPUT_PULLUP);
  pinMode(DOWN_PIN, INPUT_PULLUP);
  pinMode(LEFT_PIN, INPUT_PULLUP);
  pinMode(RIGHT_PIN, INPUT_PULLUP);

  gpio_hold_en(MIDDLE_PIN);
  gpio_hold_en(WIFI_PIN);
  gpio_hold_en(UP_PIN);
  gpio_hold_en(DOWN_PIN);
  gpio_hold_en(LEFT_PIN);
  gpio_hold_en(RIGHT_PIN);
}

// LOOP

// Log INFO
void logEvent(const String& message) {
  String logMessage = "[INFO] [" + String(millis()) + "ms] " + message + "\n";

  // Serial Output
  Serial.print(logMessage);

  // Store in Log Buffer (limit buffer size to 1000 chars)
  logBuffer += logMessage;
  if (logBuffer.length() > 1000) {
    logBuffer = logBuffer.substring(logBuffer.length() - 1000);
  }
}

void debugLogEvent(const String& message) {
  if (debugMode) {
    String logMessage = "[DEBUG] [" + String(millis()) + "ms] " + message + "\n";

    // Serial Output
    Serial.print(logMessage);

    // Telnet Client Output (if connected)
    if (telnetClient && telnetClient.connected()) {
      telnetClient.print(logMessage);
    }

    // Store in Log Buffer (limit buffer size to 1000 chars)
    logBuffer += logMessage;
    if (logBuffer.length() > 1000) {
      logBuffer = logBuffer.substring(logBuffer.length() - 1000);
    }
  }
}

void handleLogs() {
  server.send(200, "text/plain", logBuffer);
}

//Zoom Handleing

// HTTP endpoint to change zoom level dynamically
void handleZoomChange() {
  if (server.hasArg("zoom")) {
    int zoom = server.arg("zoom").toInt();  // Get zoom value from query parameter
    if (zoom >= 1 && zoom <= 8) {
      currentZoom = zoom;
      sendCustomZoomCommand(zoom);  // Send the custom zoom command
      server.send(200, "text/plain", "Zoom changed to: " + String(currentZoom) + "x");
    } else {
      server.send(400, "text/plain", "Invalid zoom value. Please enter a value between 1 and 4.");
    }
  }
}

uint16_t CRC(const uint8_t* data, size_t len) {
  uint16_t crc = 0x0000;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000)
        crc = (crc << 1) ^ 0x1021;
      else
        crc <<= 1;
    }
  }
  return crc;
}

void sendCustomZoomCommand(float zoom) {
  uint8_t command[23] = {0};

  // Fill header
  command[0] = 0x55;
  command[1] = 0x43;
  command[2] = 0x49;
  command[3] = 0x12;
  command[4] = 0x00;
  command[5] = 0x01;
  command[6] = 0x31;
  command[7] = 0x42;
  command[8] = 0x00;
  command[9] = 0x00;

  // Set zoom level
  command[10] = (uint8_t)(zoom * 10);  // e.g., 2.0 → 0x14

  // Padding
  for (int i = 11; i <= 20; i++) {
    command[i] = 0x00;
  }

  // Calculate and insert CRC
  uint16_t crc = CRC(command + 5, 16);  // CRC over bytes 5–20
  command[21] = crc & 0xFF;                     // LOW byte
  command[22] = (crc >> 8) & 0xFF;              // HIGH byte

  // Send command
  Serial1.write(command, 23);

  // Debug output
  String debug = "Zoom: " + String(zoom, 1) + "x → Final Command: ";
  for (int i = 0; i < 23; i++) {
    if (command[i] < 0x10) debug += "0";
    debug += String(command[i], HEX) + " ";
  }
  logEvent(debug);
}





//View Modechange
void handleModeChange() {
  if (server.hasArg("mode")) {
    int modeIndex = server.arg("mode").toInt();
    if (modeIndex >= 0 && modeIndex < modeCount) {
      currentMode = modeIndex;
      logEvent("Mode changed to: " + String(modes[currentMode].name));

      if (modes[currentMode].command == nullptr) {
        logEvent("[ERROR] modes[currentMode].command is nullptr!");
        server.send(500, "text/plain", "Internal error: Command not found.");
        return;
      }

      sendCommand(modes[currentMode].command);
      server.send(200, "text/plain", "Mode changed to: " + String(modes[currentMode].name));
    } else {
      logEvent("[ERROR] Invalid mode index: " + String(modeIndex));
      server.send(400, "text/plain", "Invalid mode index");
    }
  }
}


void waitForButtonRelease(int pin) {
  delay(50);
  while (digitalRead(pin) == LOW)
    ;
  delay(50);
}

void sendCommand(const uint8_t* command) {
  if (command == nullptr) {
    logEvent("[ERROR] sendCommand received nullptr!");
    return;
  }

  // Format final command string for logging
  String commandString = "Command Sent: ";
  for (int i = 0; i < 23; i++) {
    commandString += String(command[i], HEX) + " ";
  }

  // Log and send the command
  logEvent("Sending command...");
  debugLogEvent(commandString);

  // Send via Serial1
  Serial1.write(command, 23);
}

void handleRoot() {
  String page = "<h2>PVS420 Web Server</h2>";

  // Zoom Controls section
  page += "<h3>Zoom Controls</h3>";
  page += "<input type='number' id='zoomInput' placeholder='Enter zoom level (1-4)' min='1' max='4' value='1'>";
  page += "<button onclick='changeZoom()'>Set Zoom</button>";

  page += "<script>";
  page += "function changeZoom() {";
  page += "    var zoom = document.getElementById('zoomInput').value;";  // Get the custom zoom value from input
  page += "    fetch('/zoom?zoom=' + zoom);";                            // Send custom zoom value to the server
  page += "}";
  page += "</script>";

  // View Mode Controls section
  page += "<h3>View Mode Controls</h3>";
  page += "<select id='modeSelect' onchange='changeMode()'>";
  for (int i = 0; i < modeCount; i++) {
    page += "<option value='" + String(i) + "'>" + String(modes[i].name) + "</option>";
  }
  page += "</select>";

  page += "<script>";
  page += "function changeMode() {";
  page += "    var mode = document.getElementById('modeSelect').value;";  // Get the selected mode
  page += "    fetch('/mode?mode=' + mode);";                             // Send the selected mode to the server
  page += "}";
  page += "</script>";

  // View Logs button
  page += "<p><a href='/logs'><button>View Logs</button></a></p>";

  server.send(200, "text/html", page);  // Send the HTML page to the client
}

#define DEBOUNCE_DELAY 50  // Debounce delay in milliseconds

// Define the Button struct **before** using it
struct Button {
  int pin;
  unsigned long lastDebounceTime;
  bool lastState;
  bool pressed;
};

// Declare function prototype for debounce (optional but ensures ordering)
bool isButtonPressed(Button& btn);

// Initialize button objects
Button buttons[] = {
  { RIGHT_PIN, 0,  digitalRead(RIGHT_PIN), false },
  { LEFT_PIN, 0,  digitalRead(RIGHT_PIN), false },
  { UP_PIN, 0,  digitalRead(RIGHT_PIN), false },
  { DOWN_PIN, 0,  digitalRead(RIGHT_PIN), false },
  { MIDDLE_PIN, 0,  digitalRead(RIGHT_PIN), false }
};

void init_buttons() {
  for (int i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
    buttons[i].lastState = digitalRead(buttons[i].pin);
  }
}


void saveSettings() {
  if (!prefs.begin("pvs420", false)) {
    logEvent("[ERROR] Could not open preferences for writing.");
    return;
  }

  prefs.putInt("mode", currentMode);
  prefs.putFloat("zoom", currentZoom);
  prefs.putString("wifi_ssid", wifi_ssid);
  prefs.putString("wifi_password", wifi_password);
  prefs.putString("ota_password", OTA_password);
  prefs.putString("ip", IP_esp.toString());
  prefs.putString("subnet", subnet_mask.toString());

  prefs.end();  // Always end session
  logEvent("✅ Settings saved to NVS.");
}



void loadSettings() {
  if (prefs.begin("pvs420", true)) {
    currentMode = prefs.getInt("mode", 7);
    currentZoom = prefs.getFloat("zoom", 1.0);
    wifi_ssid = prefs.getString("wifi_ssid", "PVS420");
    wifi_password = prefs.getString("wifi_password", "");
    OTA_password = prefs.getString("ota_password", "1234");
    IP_esp.fromString(prefs.getString("ip", "10.4.20.1"));
    subnet_mask.fromString(prefs.getString("subnet", "255.255.255.0"));
    prefs.end();

    logEvent("Settings loaded!");
  }
}

// Function to handle debounced button press
bool isButtonPressed(Button& btn) {
  int reading = digitalRead(btn.pin);

  if (reading != btn.lastState) {
    btn.lastDebounceTime = millis();  // Reset debounce timer
  }

  if ((millis() - btn.lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading == LOW && !btn.pressed) {  // Button just pressed
      btn.lastState = reading;
      btn.pressed = true;
      return true;
    } else if (reading == HIGH) {  // Button released
      btn.pressed = false;
    }
  }

  btn.lastState = reading;
  return false;  // No valid press detected
}

void button_controlls() {

  if (isButtonPressed(buttons[3])) {  // RIGHT button
    int nextMode = (currentMode + 1) % modeCount;
    currentMode = nextMode;
    logEvent("Mode changed to: " + String(modes[currentMode].name));
    sendCommand(modes[currentMode].command);
  }

  if (isButtonPressed(buttons[2])) {  // LEFT button
    int nextMode = (currentMode - 1 + modeCount) % modeCount;
    currentMode = nextMode;
    logEvent("Mode changed to: " + String(modes[currentMode].name));
    sendCommand(modes[nextMode].command);
  }

  if (isButtonPressed(buttons[1])) {  // UP button
    if (currentZoom < 8.0) {          // Ensures max zoom is 8.0x
      currentZoom += 0.5;
      currentZoom = min(currentZoom, 8.0f);  // Ensure it never exceeds 8.0
      sendCustomZoomCommand((int)currentZoom);
      logEvent("Zoom increased to: " + String(currentZoom) + "x");
    } else {
      logEvent("Zoom already at max: " + String(currentZoom) + "x");
    }
  }

  if (isButtonPressed(buttons[0])) {  // DOWN button
    if (currentZoom > 0.5) {          // Ensures min zoom is 0.5x
      currentZoom -= 0.5;
      currentZoom = max(currentZoom, 1.0f);  // Ensure it never drops below 1
      sendCustomZoomCommand((int)currentZoom);
      logEvent("Zoom decreased to: " + String(currentZoom) + "x");
    }
  }
  if (isButtonPressed(buttons[4])) {  // Middle button
    saveSettings();
  }


  // ✅ Forward Serial Output to Telnet
  if (telnetClient && telnetClient.connected()) {
    while (Serial.available()) {
      char serial1read = Serial.read();
      telnetClient.write(serial1read);  // Send Serial input to Telnet
    }
    while (telnetClient.available()) {
      char serial1write = telnetClient.read();
      Serial1.write(serial1write);  // Send Telnet input to Serial
    }
  }
}

void wifi_mode() {
  debugLogEvent("Wifi Mode");
  wifi_setup();
  webserver_setup();
  OTA_setup();

  while (millis() - startTime < 480000) {  // Stay awake for 8 minute
    server.handleClient();
    ArduinoOTA.handle();
    button_controlls();
  }
}

void button_only_mode() {
  logEvent("Button only Mode");
  while (millis() - startTime < 100000) {  // Stay awake 30 seconds
    if (digitalRead(WIFI_PIN) == LOW) {    // If button pressed, restart into Wi-Fi mode
      return;
    }
    button_controlls();
  }
}

void sleep_setup() {
  // Set GPIOs for EXT1 wakeup
  pinMode(MIDDLE_PIN, INPUT_PULLUP);
  pinMode(WIFI_PIN, INPUT_PULLUP);
  pinMode(UP_PIN, INPUT_PULLUP);

  // Hold the pullups during deep sleep
  gpio_hold_en(MIDDLE_PIN);
  gpio_hold_en(WIFI_PIN);
  gpio_hold_en(UP_PIN);

  // Create bitmask for EXT1 wakeup on GPIOs 0, 1, and 2
  const uint64_t WAKEUP_PINS = (1ULL << MIDDLE_PIN) | (1ULL << WIFI_PIN) | (1ULL << UP_PIN);

  esp_sleep_enable_ext1_wakeup(WAKEUP_PINS, ESP_EXT1_WAKEUP_ANY_LOW);

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT1:
      logEvent("Wakeup caused by EXT1 (GPIO LOW)");
      break;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      logEvent("Cold boot (power on or reset)");
      break;
    default:
      logEvent("Wakeup cause: " + String(wakeup_reason));
      break;
  }
}

// Setup Function
void setup() {
  startTime = millis();
  setup_buttons();
  init_buttons();
  serial_setup();
  delay(300);
  loadSettings();
  sleep_setup();


esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();

if (reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
  logEvent("Cold boot detected. Applying saved mode and zoom.");
  setup_shutter_control();
  delay(100);
  sendCommand(modes[currentMode].command);
  sendCustomZoomCommand(currentZoom);
}

  button_only_mode();

  if (digitalRead(WIFI_PIN) == LOW) {
    wifi_mode();
  }

  delay(100);
  logEvent("Entering sleep mode...");
  delay(100);
  esp_deep_sleep_start();
}

void loop() {}
