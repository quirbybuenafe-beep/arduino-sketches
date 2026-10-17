#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

#define TRIG_PIN 7   // Ultrasonic Sensor Trigger Pin
#define ECHO_PIN 6   // Ultrasonic Sensor Echo Pin
#define SERVO_PIN 9   // Servo Motor Pin
#define BUZZER_PIN 10 // Buzzer Pin

LiquidCrystal_I2C lcd(0x27, 16, 2);  // Change 0x27 to 0x3F if needed
Servo servo;

int bottleCount = 0;  // Start count at 0

void setup() {
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    
    lcd.init();
    lcd.backlight();

    lcd.setCursor(0, 0);
    lcd.print("Bottle Counter");
    lcd.setCursor(0, 1);
    lcd.print("Bottles: 0");

    servo.attach(SERVO_PIN);
    servo.write(0); // Default position (Closed)

    Serial.begin(9600); // Debugging
}

void loop() {
    int distance = getFilteredDistance(); // Get stable distance reading

    Serial.println("Filtered Distance: " + String(distance) + " cm"); // Debugging

    if (distance > 0 && distance <= 3) {  // Count bottles only within 5 cm
        bottleCount++;  
        lcd.setCursor(9, 1);
        lcd.print("   "); // Clear previous number
        lcd.setCursor(9, 1);
        lcd.print(bottleCount);
        Serial.println("Bottle counted: " + String(bottleCount));
        delay(1000);  // Prevent multiple counts for the same bottle

        if (bottleCount == 3) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Claim 1 Peso!");
            delay(3000); // Show message for 3 seconds

            // Reset count
            bottleCount = 0;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Bottle Counter");
            lcd.setCursor(0, 1);
            lcd.print("Bottles: 0");

            delay(2000); // **Wait 2 seconds before counting again**
        }
    }
}

// Function to get a stable filtered distance from the ultrasonic sensor
int getFilteredDistance() {
    int sum = 0;
    int samples = 5;  // Take 5 readings to smooth noise

    for (int i = 0; i < samples; i++) {
        sum += getDistance();
        delay(10); // Small delay between readings
    }

    return sum / samples; // Return average distance
}

// Function to get raw distance from ultrasonic sensor
int getDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    long duration = pulseIn(ECHO_PIN, HIGH);
    int distance = duration * 0.034 / 2; // Convert time to distance in cm
    return distance;
}
