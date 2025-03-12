//libraries
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>

// Wi-Fi Settings
const char* ssid = "PVS420";  // Open Wi-Fi network
const IPAddress IP_esp(10, 4, 20, 1);
const IPAddress subnet_mask(255, 255, 255, 0);
WiFiServer telnetServer(23);

// OTA Configuration
const char* OTA_HOSTNAME = "ESP32-C6-OTA";
const char* OTA_PASSWORD = "1234";

//telnet client
WiFiClient telnetClient;

// Web Server setings
WebServer server(80);

//Zoomlvel
int currentZoom = 1; // Default zoom 1.0x

// Log Buffer
String logBuffer = ""; // Non-persistent log storage
bool debugMode = true;  // Set to false to disable debug logging

//Pinout

// Serial Communication Pins
#define RX_PIN 17  // ESP32-C6 Serial1 RX
#define TX_PIN 16  // ESP32-C6 Serial1 TX

//Input Pins
#define Middle_PIN 0
#define Wifi_PIN 1
#define Up_PIN 2
#define Doen_PIN 21
#define Left_PIN 22
#define Right_Pin 23

//Vibration Motor
#define Vibe 19

// View Modes
int currentMode = 8; // Default mode General Mode
struct Mode {
    const char* name;
    const uint8_t command[23];
};

const Mode modes[] = {
    {"White Hot", {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x03, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x54, 0x6D}},
    {"Black Hot", {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x03, 0x45, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x69, 0x6F}},
    {"Low Temp Highlight", {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x04, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC5, 0x65}},
    {"Low Contrast", {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x04, 0x42, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2F, 0x63}},
    {"High Contrast", {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x04, 0x42, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x68}},
    {"Highlight", {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x04, 0x42, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x6B}},
    {"Outline Mode", {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x04, 0x42, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x7D}},
    {"General Mode", {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x04, 0x42, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x60}}
};
const int modeCount = sizeof(modes) / sizeof(modes[0]);

// Shutter Serial Comands

const uint8_t activate_shutter_control[23] = {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x02, 0x41, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x3D};
const uint8_t temperature_intervals[23] = {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x02, 0x42, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x93, 0xB1};
const uint8_t maximal_time_interval[23] = {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x02, 0x42, 0x00, 0x02, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0xAC};
const uint8_t save_parameters[23] = {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x10, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA9, 0xFB};

//Setup

void serial_setup() {
    Serial.begin(115200);
    Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
}

void wifi_setup() {
    WiFi.mode(WIFI_AP);  // Enable SoftAP mode
    WiFi.softAPConfig(IP_esp, IP_esp, subnet_mask);
    WiFi.softAP(ssid);
    if (WiFi.softAP(ssid)) {
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
    ArduinoOTA.setPassword(OTA_PASSWORD);

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

void telnet_setup() {
    telnetServer.begin();
    logEvent("Telnet Server Started - Use `telnet " + IP_esp.toString() + "` to connect.");
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

// Setup Function
void setup() {
    serial_setup();
    wifi_setup();
    webserver_setup();
    OTA_setup();
    telnet_setup();
    setup_shutter_control();
}


// LOOP

// Log INFO
void logEvent(const String& message) {
    String logMessage = "[INFO] [" + String(millis()) + "ms] " + message + "\n";

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
        if (zoom >= 0 && zoom <= 8) {
            currentZoom = zoom;
            sendCustomZoomCommand(zoom);  // Send the custom zoom command
            server.send(200, "text/plain", "Zoom changed to: " + String(currentZoom) + "x");
        } else {
            server.send(400, "text/plain", "Invalid zoom value. Please enter a value between 1 and 4.");
        }
    }
}

// CRC16 function to compute the checksum for dynamic zoom command
uint16_t CRC16(uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= (data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x8005;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

// Function to create and send a custom zoom command
void sendCustomZoomCommand(int zoom) {
    uint8_t command[23] = {0x55, 0x43, 0x49, 0x12, 0x00, 0x01, 0x31, 0x42, 0x00, 0x00};
    command[10] = zoom;  // Set custom zoom value

    // Log input debug information
    debugLogEvent("Preparing Zoom Command - Input Zoom Level: " + String(zoom));

    // Calculate CRC16 for the relevant part of the command
    uint16_t crc = CRC16(command + 5, 16);
    command[21] = (uint8_t)(crc & 0xFF);  // CRC low byte
    command[22] = (uint8_t)((crc >> 8) & 0xFF);  // CRC high byte

    // Send the full command via Serial1
    Serial1.write(command, sizeof(command));

    // Format final command string
    String commandString = "Final Zoom Command: ";
    for (int i = 0; i < 23; i++) {
        commandString += String(command[i], HEX) + " ";
    }

    // Log the command
    logEvent("Zoom changed to " + String(zoom) + "x");
    debugLogEvent(commandString);
}



//View Modechange
void handleModeChange() {
    if (server.hasArg("mode")) {
        int modeIndex = server.arg("mode").toInt();
        if (modeIndex >= 0 && modeIndex < modeCount) {
            currentMode = modeIndex;
            logEvent("Mode changed to: " + String(modes[currentMode].name));
            sendCommand(modes[currentMode].command);
            server.send(200, "text/plain", "Mode changed to: " + String(modes[currentMode].name));
        } else {
            server.send(400, "text/plain", "Invalid mode index");
        }
    }
}

void sendCommand(const uint8_t* command) {
    // Debug log for input command
    debugLogEvent("Preparing to send command");

    // Format final command string for logging
    String commandString = "Final Command Sent: ";
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
    page += "    fetch('/zoom?zoom=' + zoom);";  // Send custom zoom value to the server
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
    page += "    fetch('/mode?mode=' + mode);";  // Send the selected mode to the server
    page += "}";
    page += "</script>";
    
    // View Logs button
    page += "<p><a href='/logs'><button>View Logs</button></a></p>";
    
    server.send(200, "text/html", page);  // Send the HTML page to the client
}


void telnet_rw() {
    if (telnetServer.hasClient()) {
        if (telnetClient && telnetClient.connected()) {
            telnetClient.stop();  // Close old connection
        }
        telnetClient = telnetServer.available();
        logEvent("New Telnet Client Connected!");
        telnetClient.println("ESP32-C6 Wi-Fi Serial Monitor\n");
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



void loop() {
    server.handleClient();
    ArduinoOTA.handle();
    telnet_rw();
}

