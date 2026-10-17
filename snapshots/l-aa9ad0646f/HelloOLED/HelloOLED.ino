#include <Wire.h>
#include <ACROBOTIC_SSD1306.h>

// Create an instance of the OLED display
ACROBOTIC_SSD1306 oled;

void setup() {
  Wire.begin();  // Start I2C communication
  oled.init();   // Initialize the SSD1306 OLED display
  oled.clearDisplay();  // Clear the screen
  
  // Set the text and positions
  oled.setTextXY(0, 0);  // Set cursor to line 0, position 0
  oled.putString("ACROBOTIC");
  
  oled.setTextXY(1, 0);  // Set cursor to line 1, position 0
  oled.putString("industries");
  
  oled.setTextXY(2, 0);  // Set cursor to line 2, position 0
  oled.putString("Pasadena,");
  
  oled.setTextXY(3, 0);  // Set cursor to line 3, position 0
  oled.putString("CA");
}

void loop() {
  // The loop is empty since we just want to display the text once
}
