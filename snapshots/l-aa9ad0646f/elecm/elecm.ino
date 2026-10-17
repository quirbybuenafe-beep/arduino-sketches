#include <LiquidCrystal.h>

int currentPin = 1;                  // Assign CT input to pin 1
double kilos = 0;
int peakPower = 0;
LiquidCrystal lcd(8, 9, 4, 5, 6, 7); // Assign LCD screen pins, as per LCD shield requirements

void setup()
{
  lcd.begin(16, 2);                  // Initialize the LCD
  lcd.clear();
  lcd.setCursor(0, 0);               // Set cursor to column 0, row 0 (the first row)
  lcd.print("Running...");
}

void loop()
{
  int current = 0;
  int maxCurrent = 0;
  int minCurrent = 1000;
  
  // Monitor and log the current input for 200 cycles
  for (int i = 0; i < 200; i++) {
    current = analogRead(currentPin); // Read current input
    if (current >= maxCurrent) 
      maxCurrent = current;
    else if (current <= minCurrent)
      minCurrent = current;
  }

  if (maxCurrent >= 517) {
    maxCurrent = 516;
  }

  double RMSCurrent = ((maxCurrent - 516) * 0.707) / 11.8337;    // Calculate RMS current
  int RMSPower = 220 * RMSCurrent;                                 // Calculate RMS Power
  if (RMSPower > peakPower) {
    peakPower = RMSPower;                                         // Update peak power
  }

  kilos += (RMSPower * (2.05 / 60 / 60 / 1000));                 // Calculate kilowatt hours used
  delay(2000);                                                    // Delay before next reading

  lcd.clear();
  
  // Display RMS Current on first row
  lcd.setCursor(0, 0);
  lcd.print("I: ");
  lcd.print(RMSCurrent, 2);                                      // Print current with 2 decimal places
  lcd.print("A");                                                // Display 'A'

  // Display Kilowatt hours and Peak Power on second row
  lcd.setCursor(0, 1);
  lcd.print("kWh: ");
  lcd.print(kilos, 2);                                          // Print kWh with 2 decimal places
  lcd.print(" P: ");
  lcd.print(peakPower);                                          // Display peak power in Watts
  lcd.print("W");
}
