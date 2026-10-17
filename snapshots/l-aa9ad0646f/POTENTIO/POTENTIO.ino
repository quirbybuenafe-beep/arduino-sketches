#include <CheapStepper.h>
#include <Servo.h>

Servo servo1;

#define IR_SENSOR 5
#define PROXIMITY_SENSOR 6
#define RAIN_SENSOR 4   // Rain detection
#define BUZZER 12

CheapStepper stepper(8, 9, 10, 11);  // Stepper motor pins (ULN2003)

// Stepper movement values (adjusted for correct positions)
#define DRY_BUCKET_STEPS 1200  // Adjusted so Dry is under PVC
#define METAL_BUCKET_STEPS 1200
#define WET_BUCKET_STEPS 1200  // Adjusted so Wet is moved away from PVC

bool processing = false;  // Prevents multiple activations
bool objectDetected = false; // Ensures objects are only detected once
bool atRestingPosition = false;  // Tracks if at Dry bucket (under PVC)

void setup() {
  Serial.begin(9600);

  pinMode(PROXIMITY_SENSOR, INPUT_PULLUP);
  pinMode(IR_SENSOR, INPUT);
  pinMode(RAIN_SENSOR, INPUT);
  pinMode(BUZZER, OUTPUT);

  servo1.attach(7);
  stepper.setRpm(17);

  Serial.println("System Booting...");
  servo1.write(70);  // Keep lid closed initially
  delay(1000);

  // Move to resting position (Dry bucket under PVC) on startup
  Serial.println("Moving to Dry bucket for resting position...");
  moveToBucket(DRY_BUCKET_STEPS, "DRY", false);
  atRestingPosition = true;
  
  Serial.println("System Ready...");
}

void moveToBucket(int steps, const char* wasteType, bool returnToStart) {
  if (processing) return;

  processing = true;
  objectDetected = true;
  atRestingPosition = false;  // Since we're moving away from Dry bucket

  Serial.print("Moving to ");
  Serial.print(wasteType);
  Serial.println(" bucket...");

  int stepCount = 0;
  while (stepCount < steps) {
    stepper.stepCW();
    stepCount++;
    delay(5);
  }

  Serial.println("Bucket reached! Opening lid...");
  servo1.write(180);
  delay(2000);

  tone(BUZZER, 1000, 500);  // Beep at 1000Hz for 500ms
  delay(3000);

  Serial.println("Closing lid...");
  servo1.write(70);
  delay(1000);

  if (returnToStart) {
    Serial.println("Returning to start position...");
    while (stepCount > 0) {
      stepper.stepCCW();
      stepCount--;
      delay(5);
    }
    atRestingPosition = true;  // Now back at Dry bucket under PVC
  }

  processing = false;
}

void loop() {
  int proximity = digitalRead(PROXIMITY_SENSOR);
  int irState = digitalRead(IR_SENSOR);
  int rainDetected = digitalRead(RAIN_SENSOR);

  Serial.print("Proximity: "); Serial.print(proximity);
  Serial.print(" | IR: "); Serial.print(irState);
  Serial.print(" | Rain: "); Serial.println(rainDetected);

  if (objectDetected && proximity == HIGH && irState == HIGH && rainDetected == HIGH) {
    Serial.println("Object removed, ready for next detection.");
    objectDetected = false;
  }

  // Do nothing if already at resting position and no sensor is triggered
  if (atRestingPosition && proximity == HIGH && irState == HIGH && rainDetected == HIGH) {
    return;
  }

  if (!processing && !objectDetected) {
    if (irState == LOW) {  // Dry waste detected
      Serial.println("IR detected! Sorting dry waste...");
      tone(BUZZER, 1000, 500);
      delay(3000);

      moveToBucket(WET_BUCKET_STEPS, "WET", false);  // Move to wet
      moveToBucket(METAL_BUCKET_STEPS, "METAL", false);  // Move to metal

      moveToBucket(DRY_BUCKET_STEPS, "DRY", false);  // Return to Dry bucket under PVC
      atRestingPosition = true;
    } 
    else if (proximity == LOW) {  // Metal object detected
      Serial.println("Metal detected! Moving to METAL bucket...");
      tone(BUZZER, 1000, 500);
      delay(3000);
      moveToBucket(METAL_BUCKET_STEPS, "METAL", false);

      moveToBucket(DRY_BUCKET_STEPS, "DRY", false);  // Return to Dry bucket under PVC
      atRestingPosition = true;
    } 
    else if (rainDetected == LOW) {  // Wet waste detected
      Serial.println("Wet waste detected! Moving to WET bucket...");
      tone(BUZZER, 1000, 500);
      delay(3000);
      moveToBucket(WET_BUCKET_STEPS, "WET", false);

      moveToBucket(DRY_BUCKET_STEPS, "DRY", false);  // Return to Dry bucket under PVC
      atRestingPosition = true;
    }
  }
}  
