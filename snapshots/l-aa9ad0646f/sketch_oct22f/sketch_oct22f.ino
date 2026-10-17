#include <Wire.h>
#include <LiquidCrystal.h>

// Initialize LCD (LCD Keypad Shield uses these pins)
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// Pin definitions
const int voltagePin = A2;  // Voltage sensor on A2
const int currentPin = A1;  // Current sensor on A1

// Voltage Divider Values for Resistors
const float R1 = 9650.0;
const float R2 = 2900.0;

// Calibration for ACS 712
const float sensitivity = 0.100;
float vOffset = 1.0;  // We'll calculate this dynamically

// Global Variables
float voltage = 0.0;
float current = 0.0;
float power = 2.08;   // Set power to a constant value of 2.08 watts
float energy = 0.0;
unsigned long lastTime = 0;
unsigned long interval = 1000;

void setup () {
  Serial.begin(9600); // Initialize Serial Communication

  // Initialize LCD
  lcd.begin(16, 2);

  // Display header on LCD
  lcd.setCursor(1, 0);
  lcd.print("Power & Energy");
  lcd.setCursor(5, 1);
  lcd.print("Meter");
  delay(2000);

  // Measure and set vOffset when no current is flowing (in setup)
  vOffset = measureZeroCurrentOffset();
  Serial.print("Calculated vOffset: ");
  Serial.println(vOffset, 4);
}

void loop () {
  // Measure Voltage
  voltage = measureVoltage();

  // Measure Current
  current = measureCurrent();

  // Since power is constant, we do not calculate it from voltage and current
  // power = voltage * current;  // Comment this out

  // Update Energy (Energy = Power * time in Watt-hours)
  unsigned long currentTime = millis();
  if (currentTime - lastTime >= interval) {
    float timeInHours = (interval / 3600000.0);
    energy += power * timeInHours; // Energy in Watt-hour (Wh)
    lastTime = currentTime;
  }

  // Display data on LCD
  displayData(voltage, current, power, energy);

  // Debug info in Serial Monitor
  Serial.print("Voltage: ");
  Serial.print(voltage); Serial.println(" V");
  Serial.print("Current: ");
  Serial.print(current); Serial.println(" A");
  Serial.print("Power: ");
  Serial.print(power); Serial.println(" W ");
  Serial.print("Energy: ");
  Serial.print(energy); Serial.println(" Wh");

  Serial.println("---------------------------------------");

  delay(500);
}

// Function to measure voltage using a Voltage Divider
float measureVoltage() {
  int sensorValue = analogRead(voltagePin);
  float adcVoltage = (sensorValue / 1023.0) * 5.0;
  float actualVoltage = adcVoltage / (R2 / (R1 + R2));
  return actualVoltage;
}

// Function to measure current using ACS 712
float measureCurrent() {
    int numReadings = 100;
    float total = 0.0;
    for (int i = 0; i < numReadings; i++) {  // Take 100 samples
        int sensorValue = analogRead(currentPin);
        float adcVoltage = (sensorValue / 1023.0) * 5.0;
        total += (adcVoltage - vOffset) / sensitivity; // Convert to current in Amps
        delay(10); // short delay between readings
    }
    return total / numReadings; // Return average current
}

// Function to calculate the zero current offset (vOffset) during setup
float measureZeroCurrentOffset() {
    int numReadings = 100;
    float total = 0.0;
    for (int i = 0; i < numReadings; i++) {
        int sensorValue = analogRead(currentPin);
        float adcVoltage = (sensorValue / 1023.0) * 5.0;
        total += adcVoltage;  // Sum up the ADC voltage readings
        delay(10); // short delay between readings
    }
    return total / numReadings;  // Return the average voltage as the offset
}

// Function to display data on the LCD
void displayData(float voltage, float current, float power, float energy) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("V:"); lcd.print(voltage, 2);
  lcd.print("V");
  lcd.print(" I:"); lcd.print(current, 2);
  lcd.print("A");

  lcd.setCursor(0, 1);
  lcd.print("P:"); lcd.print(power, 2);
  lcd.print("W");
  lcd.print(" E:"); lcd.print(energy, 2);
  lcd.print("Wh");
}
