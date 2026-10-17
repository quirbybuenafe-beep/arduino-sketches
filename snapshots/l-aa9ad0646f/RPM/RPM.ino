#include <ACS712.h>
#include <Wire.h>
#include <LiquidCrystal.h>

// Initialize LCD (LCD Keypad Shield uses these pins)
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// Pin definitions
const int voltagePin = A2;  // Voltage sensor on A2
const int currentPin = A1;  // Current sensor on A1
const int irPin = 2;        // IR sensor output connected to digital pin 2

// Voltage Divider Values for Resistors
const float R1 = 9650.0;
const float R2 = 2900.0;

// Calibration for ACS 712
const float sensitivity = 0.100;
const float vRef = 5;
const float vOffset = 2.485;

// Calibration factors
const float voltageCalibrationFactor = 1.5; // Adjust this to get V = 3.0
const float currentCalibrationFactor = 3.5; // Adjust this to get I = 2.0

// Global Variables for power calculation
float voltage = 0.0;
float current = 0.0;
float power = 0.0;

// RPM Variables
volatile float rev = 0;  // Stores the count of revolutions
int rpm = 0;
unsigned long oldtime = 0;

void isr() // interrupt service routine
{
  rev++;
}

void setup() {
  Serial.begin(9600); // Initialize Serial Communication
  
  // Initialize LCD
  lcd.begin(16, 2); // Set up the LCD's number of columns and rows
  lcd.setCursor(1, 0);
  lcd.print("Power & RPM Meter");
  delay(2000);

  attachInterrupt(digitalPinToInterrupt(irPin), isr, RISING);  // Attach interrupt on pin 2 for RPM counting
}

void loop() {
  // Measure Voltage
  voltage = measureVoltage();

  // Measure Current
  current = measureCurrent();

  // Calculate Power (P = V * I)
  power = voltage * current;

  // Calculate RPM
  delay(1000);                       // Wait for 1 second
  detachInterrupt(digitalPinToInterrupt(irPin)); // Detach interrupt to calculate RPM
  unsigned long time = millis() - oldtime; // Calculate time interval in milliseconds
  rpm = (rev / time) * 60000;        // Calculate RPM
  oldtime = millis();                 // Update oldtime
  rev = 0;                            // Reset revolution count for the next interval
  attachInterrupt(digitalPinToInterrupt(irPin), isr, RISING); // Reattach the interrupt

  // Display data on LCD
  displayData(voltage, current, power, rpm);

  // Debug info in Serial Monitor
  Serial.print("Voltage: ");
  Serial.print(voltage); Serial.println(" V");
  Serial.print("Current: ");
  Serial.print(current); Serial.println(" A");
  Serial.print("Power: ");
  Serial.print(power); Serial.println(" W");
  Serial.print("RPM: ");
  Serial.print(rpm); Serial.println(" RPM");

  Serial.println("---------------------------------------");

  delay(500);
}

// Function to measure voltage using a Voltage Divider
float measureVoltage() {
  int sensorValue = analogRead(voltagePin);
  float adcVoltage = (sensorValue / 1023.0) * 5.0;
  float actualVoltage = (adcVoltage / (R2 / (R1 + R2))) * voltageCalibrationFactor;
  return actualVoltage;
}

// Function to measure current using ACS 712
float measureCurrent() {
  int numReadings = 100;
  float total = 0.0;
  for (int i = 0; i < numReadings; i++) {  // Take 100 samples
    int sensorValue = analogRead(currentPin);
    float adcVoltage = (sensorValue / 1023.0) * 5.0;
    total += ((adcVoltage - vOffset) / sensitivity) * currentCalibrationFactor; // Convert to current in Amps with calibration
    delay(10); // short delay between readings
  }
  return total / numReadings; // Return average current
}

// Function to display data on the LCD
void displayData(float voltage, float current, float power, int rpm) {
  lcd.clear(); // Clear the LCD

  // Display voltage and current on the first line
  lcd.setCursor(0, 0);
  lcd.print("V:"); 
  lcd.print(voltage, 2); 
  lcd.print("V I:"); 
  lcd.print(current, 2); 
  lcd.print("A");

  // Display power and RPM on the second line
  lcd.setCursor(0, 1);
  lcd.print("P:"); 
  lcd.print(power, 2); 
  lcd.print("W RPM:"); 
  lcd.print(rpm);
}
