#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>

const int loopTickSpeed = 65;

// WiFi and server setup
const char* ssid = "ESP-Dashboard";
const char* password = "12345678";
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Turn signal & client activity
unsigned long clientTimeout = 30000;
unsigned long lastActivityTime = 0;
// Replace String with char arrays for turn signal
char currentTurnSignal[8] = "off";

// Thermistor settings
const int analogPin = 34;
const float Vref = 3.3, R_fixed = 2200.0;
const float resistanceTable[] = {7000, 5000, 3500, 2000, 1300, 900, 650, 470, 350, 270, 200, 150};
const float temperatureTable[] = {-10, 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
const int tableSize = sizeof(resistanceTable) / sizeof(resistanceTable[0]);
float smoothedTemp = 20.0;
const float tempAlpha = 0.1;

// Fuel sensor settings
const int fuelAnalogPin = 35;
float smoothedFuel = 50.0;
const float fuelAlpha = 0.1;

// Calibration table for fuel sensor
const int numPoints = 5;
float voltages[numPoints] = {0.00, 0.07, 0.30, 0.60, 0.85};
float percentages[numPoints] = {0, 25, 50, 75, 100};

float interpolate(float voltage) {
  if (voltage <= voltages[0]) return percentages[0];
  if (voltage >= voltages[numPoints - 1]) return percentages[numPoints - 1];
  for (int i = 0; i < numPoints - 1; i++) {
    if (voltage >= voltages[i] && voltage <= voltages[i + 1]) {
      float rangeV = voltages[i + 1] - voltages[i];
      float rangeP = percentages[i + 1] - percentages[i];
      float fraction = (voltage - voltages[i]) / rangeV;
      return percentages[i] + fraction * rangeP;
    }
  }
  return 0;
}

float getFuelLevel() {
  int rawADC = analogRead(fuelAnalogPin);
  if (rawADC < 0) return -999.0;
  float voltage = rawADC * (Vref / 4095.0);
  if (voltage <= 0 || voltage >= Vref) return -999.0;
  return interpolate(voltage);
}

float getWaterTemp() {
  int rawADC = analogRead(analogPin);
  if (rawADC < 0) return -999.0;
  float voltage = rawADC * (Vref / 4095.0);
  if (voltage <= 0 || voltage >= Vref) return -999.0;
  float R = (voltage * R_fixed) / (Vref - voltage);
  for (int i = 0; i < tableSize - 1; i++) {
    if (R <= resistanceTable[i] && R >= resistanceTable[i + 1]) {
      float ratio = (R - resistanceTable[i + 1]) / (resistanceTable[i] - resistanceTable[i + 1]);
      return temperatureTable[i + 1] + ratio * (temperatureTable[i] - temperatureTable[i + 1]);
    }
  }
  return -999.0;
}

// WebSocket event handling
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT: {
      char msg[16];
      snprintf(msg, sizeof(msg), "TURN:%s", currentTurnSignal);
      client->text(msg);
      break;
    }
    case WS_EVT_DISCONNECT: {
      // Optionally log or handle disconnects
      break;
    }
    case WS_EVT_DATA: {
      AwsFrameInfo *info = (AwsFrameInfo*)arg;
      if (info->opcode == WS_TEXT && len < 32) {
        char msg[32] = {0};
        size_t copyLen = len < sizeof(msg) - 1 ? len : sizeof(msg) - 1;
        memcpy(msg, data, copyLen);
        msg[copyLen] = '\0';

        if (strcmp(msg, "ping") == 0) {
          client->text("pong");
        } else if (strncmp(msg, "TURN:", 5) == 0) {
          strncpy(currentTurnSignal, msg + 5, sizeof(currentTurnSignal) - 1);
          currentTurnSignal[sizeof(currentTurnSignal) - 1] = '\0';
          char wsMsg[16];
          snprintf(wsMsg, sizeof(wsMsg), "TURN:%s", currentTurnSignal);
          ws.textAll(wsMsg);
        }
      }
      break;
    }
    default:
      break;
  }
}

// Dashboard data struct
struct DashboardData {
  int rpm;
  int speed;
  int oilPressure;
  float voltage;
  float waterTemp;
  float fuel;
};

DashboardData dashData = {0, 0, 0, 12.0, 20.0, 50.0};
int direction = 1;

// Update dashboard data from sensors (simulate or read real sensors)
void updateDashboardData() {
  dashData.rpm += direction * 100;
  dashData.speed += direction * 1;
  dashData.oilPressure += direction * 3;
  dashData.voltage += direction * 0.1;
  dashData.waterTemp = getWaterTemp();
  dashData.fuel = getFuelLevel();

  if (dashData.rpm > 6000 || dashData.rpm < 0) direction = -direction;
}

// Send dashboard data to all connected WebSocket clients
void sendDashboardData() {
  char msg[128];
  snprintf(msg, sizeof(msg),
    "RPM:%d;SPEED:%d;WATERTEMP:%d;OILPRESSURE:%d;BATTVOLT:%.1f;FUEL:%d",
    dashData.rpm, dashData.speed, (int)dashData.waterTemp,
    dashData.oilPressure, dashData.voltage, (int)dashData.fuel);
  ws.textAll(msg);
}

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin()) {
    Serial.println("Error: Failed to mount LittleFS");
    return;
  }

  // Set WiFi to both AP and STA mode
  WiFi.mode(WIFI_AP);

  // Start SoftAP
  WiFi.softAP(ssid, password);
  Serial.println("AP IP address: " + WiFi.softAPIP().toString());

  // Setup WebSocket and server handlers
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  // Serve main dashboard HTML (gzipped)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!LittleFS.exists("/dash.html.gz")) {
      request->send(404, "text/plain", "Dashboard file not found");
      return;
    }
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/dash.html.gz", "text/html");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  // Serve gauges.js
  server.on("/gauges.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!LittleFS.exists("/gauges.js")) {
      request->send(404, "text/plain", "gauges.js not found");
      return;
    }
    request->send(LittleFS, "/gauges.js", "application/javascript");
  });

  server.begin();
}

void loop() {
  static unsigned long lastSend = 0;
  if (millis() - lastSend > loopTickSpeed) {
    lastSend = millis();
    updateDashboardData();
    sendDashboardData();
  }

  // Handle Serial commands safely
  if (Serial.available()) {
    char command[16] = {0};
    size_t idx = 0;
    while (Serial.available() && idx < sizeof(command) - 1) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') break;
      command[idx++] = c;
    }
    command[idx] = '\0';

    // Remove trailing whitespace
    for (int i = idx - 1; i >= 0; --i) {
      if (command[i] == ' ' || command[i] == '\t') command[i] = '\0';
      else break;
    }

    if (strcmp(command, "left") == 0 || strcmp(command, "right") == 0 ||
        strcmp(command, "hazard") == 0 || strcmp(command, "off") == 0) {
      if (strcmp(command, currentTurnSignal) != 0) {
        strncpy(currentTurnSignal, command, sizeof(currentTurnSignal) - 1);
        currentTurnSignal[sizeof(currentTurnSignal) - 1] = '\0';
        char wsMsg[16];
        snprintf(wsMsg, sizeof(wsMsg), "TURN:%s", currentTurnSignal);
        ws.textAll(wsMsg);
      }
    }
  }

  // Keep WebSocket alive (ping clients)
  ws.cleanupClients();
}
