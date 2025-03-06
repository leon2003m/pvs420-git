#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>

// Wi-Fi Settings
const char* ssid = "PVS420";  // Open Wi-Fi network
const IPAddress local_IP(10, 4, 20, 1);
const IPAddress subnet(255, 255, 255, 0);
String logBuffer = ""; // Non-persistent log storage


WebServer server(80);
WiFiServer telnetServer(23);
WiFiClient telnetClient;

#define RX_PIN 17  // ESP32-C6 Serial1 RX
#define TX_PIN 16  // ESP32-C6 Serial1 TX

// ✅ **Function Prototypes (Declare Before setup())**
void handleRoot();
void handleOTAUpdate();
void handleLogs() {
    server.send(200, "text/plain", logBuffer);
}


// View Mode and Zoom Commands
struct Mode {
    const char* name;
    const uint8_t command[23];
};


int currentZoom = 1; // Default zoom 1.0x

// CRC16 function to compute the checksum
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

// Function to create the full custom zoom command
void sendCustomZoomCommand(int zoom) {
    uint8_t command[23] = {0x55, 0x43, 0x49, 0x12, 0x00, 0x01, 0x31, 0x42, 0x00, 0x00};
    command[10] = zoom;  // Set custom zoom value
    uint16_t crc = CRC16(command + 5, 16); // Calculate CRC16 for the relevant part
    command[21] = (uint8_t)(crc & 0xFF);  // CRC low byte
    command[22] = (uint8_t)((crc >> 8) & 0xFF);  // CRC high byte
    
    // Send the full command via Serial1
    Serial1.write(command, sizeof(command));
    
    // Log the command for debugging
    logBuffer += "Sent Command: ";
    for (int i = 0; i < 23; i++) {
        logBuffer += String(command[i], HEX) + " ";
    }
    logBuffer += "\n";
    Serial.println(logBuffer);
}

// HTTP endpoint to change zoom level dynamically
void handleZoomChange() {
    if (server.hasArg("zoom")) {
        int zoom = server.arg("zoom").toInt();  // Get zoom value from query parameter
        if (zoom >= 1 && zoom <= 4) {
            currentZoom = zoom;
            sendCustomZoomCommand(zoom);  // Send the custom zoom command
            server.send(200, "text/plain", "Zoom changed to: " + String(currentZoom) + "x");
        } else {
            server.send(400, "text/plain", "Invalid zoom value. Please enter a value between 1 and 4.");
        }
    }
}


struct Zoom {
    const char* name;
    const uint8_t command[23];
};

const uint8_t activate_shutter_control[23] = {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x02, 0x41, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x3D};
const uint8_t temperature_intervals[23] = {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x02, 0x42, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x93, 0xB1};
const uint8_t maximal_time_interval[23] = {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x02, 0x42, 0x00, 0x02, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0xAC};
const uint8_t save_parameters[23] = {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x10, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA9, 0xFB};

// Modes
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


// Zoom Levels
const Zoom zoomLevels[] = {
    {"1x", {0x55, 0x43, 0x49, 0x12, 0x00, 0x01, 0x31, 0x42, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x0A}},
    {"2x", {0x55, 0x43, 0x49, 0x12, 0x00, 0x01, 0x31, 0x42, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x0C}},
    {"3x", {0x55, 0x43, 0x49, 0x12, 0x00, 0x01, 0x31, 0x42, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0x0E}},
    {"4x", {0x55, 0x43, 0x49, 0x12, 0x00, 0x01, 0x31, 0x42, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xCF, 0x00}}
};
const int modeCount = sizeof(modes) / sizeof(modes[0]);
const int zoomCount = sizeof(zoomLevels) / sizeof(zoomLevels[0]);
int currentMode = 0;


void sendCommand(const uint8_t* command) {
    logBuffer += "Sending Command: ";
    for (int i = 0; i < 23; i++) {
        logBuffer += String(command[i], HEX) + " ";
    }
    logBuffer += "\n";

    // Keep log size manageable
    if (logBuffer.length() > 1000) {  // Trim if log gets too long
        logBuffer = logBuffer.substring(logBuffer.length() - 1000);
    }

    Serial1.write(command, 23);

    // Send to Telnet Client (if connected)
    if (telnetClient && telnetClient.connected()) {
        telnetClient.print("Sent Command: ");
        for (int i = 0; i < 23; i++) {
            telnetClient.printf("%02X ", command[i]);
        }
        telnetClient.println();
    }
}
void handleModeChange() {
    if (server.hasArg("mode")) {
        int modeIndex = server.arg("mode").toInt();
        if (modeIndex >= 0 && modeIndex < modeCount) {
            currentMode = modeIndex;
            sendCommand(modes[currentMode].command);
            server.send(200, "text/plain", "Mode changed to: " + String(modes[currentMode].name));
        } else {
            server.send(400, "text/plain", "Invalid mode index");
        }
    }
}



void setup() {
    Serial.begin(115200);
    Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
    WiFi.mode(WIFI_AP);  // Enable SoftAP mode
    WiFi.softAPConfig(local_IP, local_IP, subnet);
    WiFi.softAP(ssid);
    if (!WiFi.softAP(ssid)) {
      Serial.println("Wi-Fi failed to start");
    } else {
      Serial.println("Wi-Fi started successfully");
    }
    WiFi.softAPConfig(local_IP, local_IP, subnet);
    server.begin();
    
    // ✅ Now functions are properly declared
    server.on("/", HTTP_GET, handleRoot);
    server.on("/mode", handleModeChange);
    server.on("/zoom", handleZoomChange);
    server.on("/save", handleSaveSettings);
    server.on("/update", HTTP_GET, handleOTAUpdate);
    server.on("/logs", handleLogs);
    server.begin();
        Serial.println("Wi-Fi SoftAP Enabled!");

    // ✅ Enable OTA Updates
    ArduinoOTA.setHostname("ESP32-C6-OTA");
    ArduinoOTA.setPassword("1234");

    ArduinoOTA.onStart([]() {
        Serial.println("OTA Update Started...");
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\nOTA Update Complete!");
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA Error [%u]\n", error);
    });

    ArduinoOTA.begin();

    // ✅ Start Telnet Server
    telnetServer.begin();
    Serial.println("Telnet Server Started - Use `telnet 192.168.4.1` to connect.");
    delay(10000); // 10-second delay before sending
    sendCommand(activate_shutter_control);
    
    delay(100); // Another 5-second delay before sending next command
    sendCommand(temperature_intervals);
    
    delay(100); // Another 5-second delay before sending next command
    sendCommand(maximal_time_interval);
    
    delay(100); // Another 5-second delay before sending save command
    sendCommand(save_parameters);
}
void handleOTAUpdate() {
    server.send(200, "text/html", "<h2>Ready for OTA Update!</h2>");
    Serial.println("Ready for OTA Update!");
}

void handleSaveSettings() {
    server.send(200, "text/plain", "Settings saved: Mode - " + String(modes[currentMode].name) + ", Zoom - " + String(zoomLevels[currentZoom].name));
}

void handleRoot() {
    String page = "<h2>PVS420 Web Server</h2>";
    
    page += "<h3>Zoom Controls</h3>";
    page += "<select id='zoomSelect' onchange='changeZoom()'>";
    for (int i = 0; i < zoomCount; i++) {
        page += "<option value='" + String(i) + "'>" + String(zoomLevels[i].name) + "</option>";
    }
    page += "</select>";
    
    page += "<h3>View Mode Controls</h3>";
    page += "<select id='modeSelect' onchange='changeMode()'>";
    for (int i = 0; i < modeCount; i++) {
        page += "<option value='" + String(i) + "'>" + String(modes[i].name) + "</option>";
    }
    page += "</select>";
    
    page += "<script>";
    page += "function changeZoom() {";
    page += "var zoom = document.getElementById('zoomSelect').value;";
    page += "fetch('/zoom?zoom=' + zoom);";
    page += "}";
    page += "function changeMode() {";
    page += "var mode = document.getElementById('modeSelect').value;";
    page += "fetch('/mode?mode=' + mode);";
    page += "}";
    page += "</script>";
    page += "<p><a href='/logs'><button>View Logs</button></a></p>";
    
    server.send(200, "text/html", page);
}

void loop() {
    server.handleClient();
    ArduinoOTA.handle();

    // ✅ Handle Telnet Client Connection
    if (telnetServer.hasClient()) {
        if (telnetClient && telnetClient.connected()) {
            telnetClient.stop();  // Close old connection
        }
        telnetClient = telnetServer.available();
        Serial.println("New Telnet Client Connected!");
        telnetClient.println("ESP32-C6 Wi-Fi Serial Monitor\n");
    }

    // ✅ Forward Serial Output to Telnet
    if (telnetClient && telnetClient.connected()) {
        while (Serial.available()) {
            char c = Serial.read();
            telnetClient.write(c);  // Send Serial input to Telnet
        }
    }

    Serial.println("Loop running...");
    delay(1000);
}

