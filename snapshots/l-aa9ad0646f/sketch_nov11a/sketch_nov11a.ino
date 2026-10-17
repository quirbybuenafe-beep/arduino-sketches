#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <CheapStepper.h>
#include <Servo.h>

Servo servo1;

// Sensor & Buzzer Pins
#define IR_SENSOR 5
#define BUZZER 12

// I2C LCD Setup
LiquidCrystal_I2C lcd(0x27, 16, 2); // Try 0x3F if 0x27 does not work
CheapStepper stepper(8, 9, 10, 11); 

// Stepper movement values
#define WET_BUCKET_STEPS 240
#define METAL_BUCKET_STEPS 240

bool systemActivated = false;  // Ensures IR starts the loop only once

void setup() {
  Serial.begin(9600);

  pinMode(IR_SENSOR, INPUT);
  pinMode(BUZZER, OUTPUT);

  // LCD Initialization - FIXED
  lcd.begin(16, 2);  // ✅ Replaces lcd.init()
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Ready...");

  servo1.attach(7);
  stepper.setRpm(20);  // Increased speed
  servo1.write(70);  
  delay(1000);
}

void moveToBucket(int steps, const char* bucketName) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Moving to:");
  lcd.setCursor(0, 1);
  lcd.print(bucketName);
  
  Serial.print("Moving to ");
  Serial.print(bucketName);
  Serial.println(" bucket...");

  for (int i = 0; i < steps; i++) {
    stepper.stepCW();
    delay(3);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(bucketName);
  lcd.setCursor(0, 1);
  lcd.print("Bucket reached!");

  Serial.println("Bucket reached! Opening lid...");
  tone(BUZZER, 1000, 500);
  servo1.write(180);
  delay(2000);

  Serial.println("Closing lid...");
  servo1.write(70);
  delay(1000);

  Serial.println("Returning to start...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Returning Home...");

  for (int i = 0; i < steps; i++) {
    stepper.stepCCW();
    delay(3);
  }

  delay(1000);
}

void loop() {
  int irState = digitalRead(IR_SENSOR);

  if (!systemActivated && irState == LOW) {
    systemActivated = true; // IR detected, start loop
    Serial.println("IR detected! Starting cycle...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("IR Detected!");
    delay(1000);
  }

  if (systemActivated) {
    moveToBucket(WET_BUCKET_STEPS, "Wet Bucket");
    moveToBucket(METAL_BUCKET_STEPS, "Metal Bucket");
  }
}
