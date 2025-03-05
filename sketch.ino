#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>

// Wi-Fi Settings
const char* ssid = "PVS420";  // Open Wi-Fi network
const IPAddress local_IP(10, 4, 20, 1);
const IPAddress subnet(255, 255, 255, 0);

WebServer server(80);
WiFiServer telnetServer(23);
WiFiClient telnetClient;

#define RX_PIN 17  // ESP32-C6 Serial1 RX
#define TX_PIN 16  // ESP32-C6 Serial1 TX

// ✅ **Function Prototypes (Declare Before setup())**
void handleRoot();
void handleOTAUpdate();

// View Mode and Zoom Commands
const uint8_t white_hot[23]       = {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x03, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x54, 0x6D};
const uint8_t black_hot[23]       = {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x03, 0x45, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x69, 0x6F};
const uint8_t low_temp_highlight[23] = {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x04, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC5, 0x65};
const uint8_t low_contrast[23]    = {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x04, 0x42, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2F, 0x63};
const uint8_t high_contrast[23]   = {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x04, 0x42, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x68};
const uint8_t highlight[23]       = {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x04, 0x42, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x6B};
const uint8_t outline_mode[23]    = {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x04, 0x42, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x7D};
const uint8_t general_mode[23]    = {0x55, 0x43, 0x49, 0x12, 0x00, 0x10, 0x04, 0x42, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x60};

const uint8_t zoom_1x[23]         = {0x55, 0x43, 0x49, 0x12, 0x00, 0x01, 0x31, 0x42, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x0A};
const uint8_t zoom_2x[23]         = {0x55, 0x43, 0x49, 0x12, 0x00, 0x01, 0x31, 0x42, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x0C};
const uint8_t zoom_3x[23]         = {0x55, 0x43, 0x49, 0x12, 0x00, 0x01, 0x31, 0x42, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0x0E};
const uint8_t zoom_4x[23]         = {0x55, 0x43, 0x49, 0x12, 0x00, 0x01, 0x31, 0x42, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xCF, 0x00};


const uint8_t* const modes[] PROGMEM = {white_hot, black_hot, low_temp_highlight};
const uint8_t* const zoomLevels[] PROGMEM = {zoom_1x, zoom_2x};
const int modeCount = sizeof(modes) / sizeof(modes[0]);
const int zoomCount = sizeof(zoomLevels) / sizeof(zoomLevels[0]);
int currentMode = 0;
int currentZoom = 0;

void sendCommand(const uint8_t* command) {
    uint8_t buffer[23];
    memcpy_P(buffer, command, 23);  // Copy from PROGMEM

    // Print to Serial Monitor
    Serial.println("Sending Command:");
    for (int i = 0; i < 23; i++) {
        Serial.printf("%02X ", buffer[i]);  // Print as HEX
    }
    Serial.println();

    // Print to Telnet Client (if connected)
    if (telnetClient && telnetClient.connected()) {
        telnetClient.println("Sending Command:");
        for (int i = 0; i < 23; i++) {
            telnetClient.printf("%02X ", buffer[i]);  // Print as HEX
        }
        telnetClient.println();
    }

    // Send command via Serial1
    Serial1.write(buffer, 23);
}



void handleZoomChange() {
    if (server.hasArg("zoom")) {
        int zoomIndex = server.arg("zoom").toInt();
        if (zoomIndex >= 0 && zoomIndex < zoomCount) {
            currentZoom = zoomIndex;
            sendCommand(zoomLevels[currentZoom]);
            server.send(200, "text/plain", "Zoom changed");
            Serial.printf("Zoom changed to %d\n", currentZoom);
        }
    }
}

void handleModeChange() {
    if (server.hasArg("mode")) {
        int modeIndex = server.arg("mode").toInt();
        if (modeIndex >= 0 && modeIndex < modeCount) {
            currentMode = modeIndex;
            sendCommand(modes[currentMode]);
            server.send(200, "text/plain", "Mode changed");
        }
    }
}


void setup() {
    Serial.begin(115200);
    Serial1.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    WiFi.mode(WIFI_AP);  // Enable SoftAP mode
    WiFi.softAPConfig(local_IP, local_IP, subnet);
    WiFi.softAP(ssid);
    if (!WiFi.softAP(ssid)) {
      Serial.println("Wi-Fi failed to start");
    } else {
      Serial.println("Wi-Fi started successfully");
    }
    WiFi.softAPConfig(local_IP, local_IP, subnet);
    server.on("/mode", handleModeChange);
    server.on("/zoom", handleZoomChange);
    server.begin();
    
    // ✅ Now functions are properly declared
    server.on("/", HTTP_GET, handleRoot);
    server.on("/update", HTTP_GET, handleOTAUpdate);
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

// ✅ Function Definitions (Now After setup())
void handleRoot() {
    String page = "<h2>ESP32-C6 Web Server</h2>";
    
    // OTA Update Button
    page += "<p><a href='/update'><button>Start OTA Update</button></a></p>";

    // Zoom Buttons
    page += "<h3>Zoom Controls</h3>";
    for (int i = 0; i < zoomCount; i++) {
        page += "<p><a href='/zoom?zoom=" + String(i) + "'><button>Zoom " + String(i+1) + "x</button></a></p>";
    }

    // Mode Buttons
    page += "<h3>View Mode Controls</h3>";
    for (int i = 0; i < modeCount; i++) {
        page += "<p><a href='/mode?mode=" + String(i) + "'><button>Mode " + String(i+1) + "</button></a></p>";
    }

    server.send(200, "text/html", page);
}


void handleOTAUpdate() {
    server.send(200, "text/html", "<h2>Ready for OTA Update!</h2>");
    Serial.println("Ready for OTA Update!");
}
