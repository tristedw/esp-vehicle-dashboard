const int analogPin = 34;
const float Vref = 3.3;
const int ADCmax = 4095;
// 220 ohm resistor 
// Calibration table
const int numPoints = 5;
float voltages[numPoints] = {0.00, 0.07, 0.30, 0.60, 0.85};
float percentages[numPoints] = {0, 25, 50, 75, 100};

void setup() {
  Serial.begin(19200);
}

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

  return 0; // fallback
}

void loop() {
  int raw = analogRead(analogPin);
  float voltage = raw * Vref / ADCmax;
  float fuelPercent = interpolate(voltage);

  Serial.print("ADC Raw: ");
  Serial.print(raw);
  Serial.print(" | Voltage: ");
  Serial.print(voltage, 3);
  Serial.print(" V | Fuel Level: ");
  Serial.print(fuelPercent, 1);
  Serial.println(" %");

  delay(200);
}