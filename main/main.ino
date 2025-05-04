#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
#include <LittleFS.h>

// WiFi and server setup
const char* ssid = "ESP-Dashboard";
const char* password = "12345678";
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Turn signal & client activity
unsigned long clientTimeout = 30000;
unsigned long lastActivityTime = 0;
String currentTurnSignal = "off";

// Thermistor settings
const int analogPin = A0;
const float Vref = 3.3, R_fixed = 2000.0;
const float resistanceTable[] = {7000, 5000, 3500, 2000, 1300, 900, 650, 470, 350, 270, 200, 150};
const float temperatureTable[] = {-10, 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
const int tableSize = sizeof(resistanceTable) / sizeof(resistanceTable[0]);
float smoothedTemp = 60.0;  
const float tempAlpha = 0.1;  // Smoothing factor (0.0 - 1.0)

float getWaterTemp() {
  int rawADC = analogRead(analogPin);
  if (rawADC == -1) {
    Serial.println(F("Error: Failed to read analog pin"));
    return -999.0;
  }

  float voltage = rawADC * (Vref / 1024.0);
  if (voltage <= 0 || voltage >= Vref) {
    Serial.println(F("Error: Voltage out of range or sensor disconnected"));
    return -999.0;
  }

  float R = (voltage * R_fixed) / (Vref - voltage);

  for (int i = 0; i < tableSize - 1; i++) {
    if (R <= resistanceTable[i] && R >= resistanceTable[i + 1]) {
      float ratio = (R - resistanceTable[i + 1]) / (resistanceTable[i] - resistanceTable[i + 1]);
      return temperatureTable[i + 1] + ratio * (temperatureTable[i] - temperatureTable[i + 1]);
    }
  }

  Serial.println(F("Error: Water temperature out of range"));
  return -999.0;
}

// WebSocket event handling
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (!client) {
    Serial.println(F("Error: WebSocket client is null"));
    return;
  }

  switch (type) {
    case WS_EVT_CONNECT:
      Serial.println(F("Client connected"));
      client->text("TURN:" + currentTurnSignal);
      break;
    case WS_EVT_DISCONNECT:
      Serial.println(F("Client disconnected"));
      break;
    case WS_EVT_DATA: {
      if (!arg || !data) {
        Serial.println(F("Error: WebSocket data or arg is null"));
        return;
      }

      AwsFrameInfo *info = (AwsFrameInfo*)arg;
      if (info->opcode == WS_TEXT) {
        String msg;
        for (size_t i = 0; i < len; i++) msg += (char)data[i];
        Serial.print(F("Data received: "));
        Serial.println(msg);

        if (msg == "ping") {
          client->text("pong");
        } else if (msg.startsWith("TURN:")) {
          currentTurnSignal = msg.substring(5);
          Serial.println("Turn signal updated to: " + currentTurnSignal);
          ws.textAll("TURN:" + currentTurnSignal);
        }
      }
      break;
    }
    default:
      Serial.println(F("Unhandled WebSocket event"));
      break;
  }
}

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin()) {
    Serial.println("Error: Failed to mount LittleFS");
    return;
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
    Serial.println(F("WiFi AP setup failed!"));
    return;
  }

  Serial.println(F("Access Point Started"));
  Serial.print(F("IP Address: "));
  Serial.println(WiFi.softAPIP());

  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request) {
      Serial.println(F("Error: Null HTTP request"));
      return;
    }

    if (!LittleFS.exists("/dash.html.gz")) {
      Serial.println(F("Error: Missing dash.html.gz file in LittleFS"));
      request->send(404, "text/plain", "Dashboard file not found");
      return;
    }

    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/dash.html.gz", "text/html");
    if (!response) {
      Serial.println(F("Error: Failed to create HTTP response"));
      return;
    }

    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.begin();
}

void loop() {
  static unsigned long lastTime = 0;
  static int rpm = 0, speed = 0, oilPressure = 0, voltage = 120, fuel = 20, direction = 1;
  static const int rpmStep = 100, rpmMax = 6000, rpmMin = 0;
  static const int speedStep = 1, speedMax = 240, speedMin = 0;
  static const int pressureStep = 3, pressureMin = 0, pressureMax = 80;
  static const int voltageStep = 1, voltageMin = 100, voltageMax = 140;
  static const int fuelStep = 1, fuelMin = 0, fuelMax = 100;

  if (millis() - lastTime > 80) {
    lastTime = millis();

    rpm += direction * rpmStep;
    speed += direction * speedStep;
    oilPressure += direction * pressureStep;
    voltage += direction * voltageStep;
    fuel += direction * fuelStep;

    if (rpm >= rpmMax || rpm <= rpmMin) direction = -direction;

    rpm = constrain(rpm, rpmMin, rpmMax);
    speed = constrain(speed, speedMin, speedMax);
    oilPressure = constrain(oilPressure, pressureMin, pressureMax);
    voltage = constrain(voltage, voltageMin, voltageMax);
    fuel = constrain(fuel, fuelMin, fuelMax);

    float temp = getWaterTemp();
    if (temp == -999.0) {
      Serial.println(F("Warning: Invalid water temperature value"));
      temp = smoothedTemp;  // Use last known good value
    }

    smoothedTemp = smoothedTemp * (1.0 - tempAlpha) + temp * tempAlpha;

    if (ws.count() > 0) {
      String msg = "RPM:" + String(rpm) +
                  ";SPEED:" + String(speed) +
                  ";WATERTEMP:" + String((int)smoothedTemp) +
                  ";OILPRESSURE:" + String(oilPressure) +
                  ";BATTVOLT:" + String(voltage / 10.0, 1) +
                  ";FUEL:" + String(fuel);
      ws.textAll(msg);
    }

    yield();
  }

  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    Serial.print(F("Serial command received: "));
    Serial.println(command);

    if (command == "left" || command == "right" || command == "hazard" || command == "off") {
      if (command != currentTurnSignal) {
        currentTurnSignal = command;
        Serial.println("Triggering " + command + " turn signal");
        ws.textAll("TURN:" + command);
      }
    } else {
      Serial.println(F("Unknown command"));
    }
  }

  if (ESP.getFreeHeap() < 512) {
    Serial.println(F("Warning: Low memory available!"));
  }

  if (millis() - lastActivityTime > clientTimeout) {
    for (int i = 0; i < ws.count(); i++) {
      AsyncWebSocketClient *client = ws.client(i);
      if (client && client->status() == WS_CONNECTED) {
        client->close();
        Serial.println(F("Client disconnected due to timeout"));
      }
    }
  }
}