#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
#include <LittleFS.h>

const char* ssid = "ESP-Dashboard";
const char* password = "12345678";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

unsigned long clientTimeout = 30000;  // Timeout duration (in ms) for checking client activity
unsigned long lastActivityTime = 0;
String currentTurnSignal = "off";  // Global turn signal state

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {

  switch (type) {
    case WS_EVT_CONNECT:
      Serial.println(F("Client connected"));
      client->text("TURN:" + currentTurnSignal); // Send saved turn signal state
      break;

    case WS_EVT_DISCONNECT:
      Serial.println(F("Client disconnected"));
      break;

    case WS_EVT_DATA: {
      AwsFrameInfo *info = (AwsFrameInfo*)arg;
      if (info->opcode == WS_TEXT) {
        String msg;
        for (size_t i = 0; i < len; i++) {
          msg += (char)data[i];
        }
        Serial.print(F("Data received: "));
        Serial.println(msg);

        if (msg == "ping") {
          Serial.println(F("Ping received, sending Pong..."));
          client->text("pong");
        } else if (msg.startsWith("TURN:")) {
          currentTurnSignal = msg.substring(5);  // Update global state
          Serial.println("Turn signal updated to: " + currentTurnSignal);
          ws.textAll("TURN:" + currentTurnSignal);  // Broadcast to all clients
        }
      }
      break;
    }

    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin()) {
  Serial.println("An error occurred while mounting LittleFS");
  return;
}
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);  // Disable WiFi power saving for stability
  
  if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
    Serial.println(F("WiFi AP setup failed!"));
    return;  // Early exit if AP setup failed
  }

  Serial.println(F("Access Point Started"));
  Serial.print(F("IP Address: "));
  Serial.println(WiFi.softAPIP());

  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    //if (request->hasHeader("Accept-Encoding") && request->getHeader("Accept-Encoding")->value().indexOf("gzip") >= 0) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/dash.html.gz", "text/html");
    response->addHeader("Content-Encoding", "gzip"); // manually set gzip
    request->send(response);
    //}
    //else {
      // If the client doesn't support gzip, serve the regular file
      //request->send(LittleFS, "/dash.html", "text/html");
    //}
  });

  server.begin();
}

void loop() {
  static unsigned long lastTime = 0;
  static int rpm = 0;
  static int speed = 0;
  static int waterTemp = 60;
  static int oilPressure = 0;
  static int voltage = 120;          // 12.0V x 10 to send as integer
  static int fuel = 20;
  static int direction = 1;

  static const int rpmStep = 100;
  static const int rpmMax = 6000;
  static const int rpmMin = 0;

  static const int speedStep = 1;
  static const int speedMax = 240;
  static const int speedMin = 0;

  static const int tempStep = 1;
  static const int tempMin = 60;
  static const int tempMax = 120;

  static const int pressureStep = 3;
  static const int pressureMin = 0;
  static const int pressureMax = 80;

  static const int voltageStep = 1;
  static const int voltageMin = 100;   // 10.0V
  static const int voltageMax = 140;   // 14.0V

  static const int fuelStep = 1;
  static const int fuelMin = 0;
  static const int fuelMax = 100;

  String turnSignal = "";

  if (millis() - lastTime > 80) {
    lastTime = millis();

    rpm += direction * rpmStep;
    speed += direction * speedStep;
    waterTemp += direction * tempStep;
    oilPressure += direction * pressureStep;
    voltage += direction * voltageStep;
    fuel += direction * fuelStep;

    if (rpm >= rpmMax || rpm <= rpmMin) direction = -direction;

    speed = constrain(speed, speedMin, speedMax);
    waterTemp = constrain(waterTemp, tempMin, tempMax);
    oilPressure = constrain(oilPressure, pressureMin, pressureMax);
    voltage = constrain(voltage, voltageMin, voltageMax);
    fuel = constrain(fuel, fuelMin, fuelMax);

    // Send main data
    if (ws.count() > 0) {
      String combinedMessage = "RPM:" + String(rpm) +
                              ";SPEED:" + String(speed) +
                              ";WATERTEMP:" + String(waterTemp) +
                              ";OILPRESSURE:" + String(oilPressure) +
                              ";BATTVOLT:" + String(voltage / 10.0, 1) +
                              ";FUEL:" + String(fuel);

      ws.textAll(combinedMessage);
    }

    yield();
  }

  // Read serial command
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    Serial.print(F("Serial command received: "));
    Serial.println(command);

    if (command == "left" || command == "right" || command == "hazard" || command == "off") {
      turnSignal = command;
      
      // Only change the signal state if it's different from the current one
      if (turnSignal != currentTurnSignal) {
        currentTurnSignal = turnSignal;  // Update current state
        Serial.println("Triggering " + command + " turn signal");
        ws.textAll("TURN:" + turnSignal);  // Send updated signal to all clients
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