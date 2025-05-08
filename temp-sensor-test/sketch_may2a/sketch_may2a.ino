const int analogPin = A0;
const float Vref = 3.3, R_fixed = 2200.0;

const float resistanceTable[] = {7000, 5000, 3500, 2000, 1300, 900, 650, 470, 350, 270, 200, 150};
const float temperatureTable[] = {-10, 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
const int tableSize = sizeof(resistanceTable) / sizeof(resistanceTable[0]);

void setup() {
  Serial.begin(115200);
}

void loop() {
  float voltage = analogRead(analogPin) * (Vref / 1024.0);
  if (voltage <= 0 || voltage >= Vref) {
    Serial.println("Sensor disconnected or out of range");
  } else {
    float R = (voltage * R_fixed) / (Vref - voltage);
    Serial.print("Temperature: ");
    Serial.print(interpolateTemperature(R));
    Serial.println(" °C");
  }
  delay(200);
}

float interpolateTemperature(float R) {
  for (int i = 0; i < tableSize - 1; i++) {
    if (R <= resistanceTable[i] && R >= resistanceTable[i + 1]) {
      float ratio = (R - resistanceTable[i + 1]) / (resistanceTable[i] - resistanceTable[i + 1]);
      return temperatureTable[i + 1] + ratio * (temperatureTable[i] - temperatureTable[i + 1]);
    }
  }
  return -999.0;
}