#include <LiquidCrystal.h>

// Define pins
const int currentPin = A1; // Current sensor output pin (SCT)
const int voltagePin = A2; // Voltage sensor output pin (if used)

// Define sensor parameters
const float mVperAmpValue = 66.0; // SCT-013-000 sensitivity (66 mV per amp)
const float moduleSupplyVoltage = 5.0; // Arduino supply voltage (5V)
const float voltageConversionFactor = 0.707; // RMS conversion factor for voltage

// LCD setup
LiquidCrystal lcd(8, 9, 4, 5, 6, 7); // Initialize the LCD with Arduino pins

// Variables
float currentReading = 0;
float voltageReading = 0;
float rmsCurrent = 0;
float rmsVoltage = 0;
float power = 0;
float kWh = 0;
unsigned long previousMillis = 0;

void setup() {
  Serial.begin(9600);  // Begin serial communication
  lcd.begin(16, 2);    // Initialize the 16x2 LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Energy Meter");
  delay(2000);         // Display welcome message for 2 seconds
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Read current and voltage sensor values
  currentReading = analogRead(currentPin); // Read from A1
  voltageReading = analogRead(voltagePin); // Read from A2 (if voltage sensor is used)

  // Calculate RMS current and voltage
  rmsCurrent = calculateCurrentRMS(currentReading);
  rmsVoltage = calculateVoltageRMS(voltageReading);

  // Calculate power in watts
  power = rmsVoltage * rmsCurrent;

  // Accumulate energy in kilowatt-hours (kWh)
  if (currentMillis - previousMillis >= 1000) { // Every 1 second
    kWh += (power / 1000.0) / 3600.0; // kWh calculation
    previousMillis = currentMillis;
  }

  // Display values on the LCD without spaces between the units and numbers
  lcd.clear();
  
  // Display current and voltage on the first row
  lcd.setCursor(0, 0);
  lcd.print("I:"); lcd.print(rmsCurrent, 2); // Display current (amps) without space
  lcd.print("A"); 
  
  lcd.setCursor(9, 0); // Move cursor to display voltage on the right side
  lcd.print("V:"); lcd.print(rmsVoltage, 2); // Display voltage (volts) without space

  // Display power and kWh on the second row
  lcd.setCursor(0, 1);
  lcd.print("P:"); lcd.print(power, 2); // Display power (watts) without space
  lcd.print("W");

  // Ensure there's enough space for kWh on the right side
  lcd.setCursor(9, 1);
  lcd.print("kWh:"); lcd.print(kWh, 4); // Display energy (kWh) without space

  // Print calculated values to Serial Monitor
  Serial.print("I:"); Serial.print(rmsCurrent, 2); Serial.print("A  ");
  Serial.print("V:"); Serial.print(rmsVoltage, 2); Serial.print("V  ");
  Serial.print("P:"); Serial.print(power, 2); Serial.print("W  ");
  Serial.print("kWh:"); Serial.println(kWh, 4);

  // Print raw SCT sensor value for debugging
  int sensorValue = analogRead(A1); // Read raw SCT value from A1
  Serial.print("SCT Raw Sensor Reading: ");
  Serial.println(sensorValue);

  delay(2000); // Delay for 2 seconds
}

// Function to calculate RMS current
float calculateCurrentRMS(float reading) {
  float currentVoltage = ((reading / 1024.0) * moduleSupplyVoltage); // Convert to voltage
  float current = currentVoltage / mVperAmpValue; // Convert voltage to current
  return (current < 0) ? 0 : current; // Ensure no negative current
}

// Function to calculate RMS voltage
float calculateVoltageRMS(float reading) {
  float voltage = ((reading / 1024.0) * moduleSupplyVoltage) * voltageConversionFactor; // Convert to voltage
  return (voltage < 0) ? 0 : voltage; // Ensure no negative voltage
}
