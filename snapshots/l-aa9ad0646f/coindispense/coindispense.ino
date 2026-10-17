#include <Servo.h>

Servo myServo;

void setup() {
  myServo.attach(9);  // Attach the servo to pin 9
  myServo.write(0);   // Start at 0 degrees
  delay(250);         // Short delay for stability

  myServo.write(90);  // Move to 90 degrees
  delay(250);         // Minimal wait for movement

  myServo.write(0);   // Instantly move back to 0 degrees
}

void loop() {
  // Do nothing after executing once
}
