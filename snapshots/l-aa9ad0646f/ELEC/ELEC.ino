#include <Servo.h>

Servo servo1;

#define ir 5       // IR sensor
#define proxi 6    // Inductive proximity sensor
#define buzzer 12  // Buzzer
#define rainSensor 4  // Rain sensor (D0 pin)

// Stepper motor pins
#define IN1 8
#define IN2 9
#define IN3 10
#define IN4 11

// Stepper sequence for 28BYJ-48
const int stepSequence[4][4] = {
  {1, 0, 0, 1}, // Step 1
  {1, 1, 0, 0}, // Step 2
  {0, 1, 1, 0}, // Step 3
  {0, 0, 1, 1}  // Step 4
};

void setup() {
  Serial.begin(9600);
  
  pinMode(proxi, INPUT_PULLUP);
  pinMode(ir, INPUT);
  pinMode(rainSensor, INPUT);
  pinMode(buzzer, OUTPUT);
  
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  servo1.attach(7);
  servo1.write(180);
  delay(1000);
  servo1.write(70);
  delay(1000);
}

// Function to move the stepper motor
void moveStepper(int steps, int direction) {
  for (int i = 0; i < steps; i++) {
    for (int step = 0; step < 4; step++) {
      int index = direction ? step : 3 - step; // Reverse sequence for CCW
      digitalWrite(IN1, stepSequence[index][0]);
      digitalWrite(IN2, stepSequence[index][1]);
      digitalWrite(IN3, stepSequence[index][2]);
      digitalWrite(IN4, stepSequence[index][3]);
      delay(2); // Adjust speed
    }
  }
}

void loop() {
  int rain = digitalRead(rainSensor); // Read rain sensor (D0 pin)
  int proximity = digitalRead(proxi); // Read inductive proximity sensor
  int irState = digitalRead(ir); // Read IR sensor

  Serial.print("Rain Sensor: ");
  Serial.println(rain);
  
  // If rain is detected, alert and stop operations
  if (rain == LOW) { // LOW = Rain detected
    tone(buzzer, 1000);
    Serial.println("Rain detected! Stopping operations...");
    delay(2000);
    noTone(buzzer);
    return;  // Stop further execution
  }

  Serial.print("Proximity Sensor: ");
  Serial.println(proximity);
  
  Serial.print("IR Sensor: ");
  Serial.println(irState);

  if (proximity == LOW) {
    tone(buzzer, 1000, 500);
    moveStepper(200, 1);  // Move CW
    delay(1000);
    servo1.write(180);
    delay(1000);
    servo1.write(70);
    delay(1000);
    moveStepper(200, 0);  // Move CCW
    delay(1000);
  }

  if (irState == LOW) {
    tone(buzzer, 1000, 500);
    delay(1000);
    moveStepper(100, 1);  // Move CW a smaller distance
    delay(1000);
    servo1.write(180);
    delay(1000);
    servo1.write(70);
    delay(1000);
    moveStepper(100, 0);  // Move CCW
    delay(1000);
  }
}
