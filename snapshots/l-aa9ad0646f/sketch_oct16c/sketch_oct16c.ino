#include <LiquidCrystal.h>

// Define pins for the LCD
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// Lyrics of the song
const char* lyrics[] = {
  "Your called her a hoe?",
  "Because she said no?",
  "Buddy you’re letting all",
  "your fractal masculinity show,",
  "I mean, (dude)",
  "Oh my! (She just not)",
  "she just not into you!",
  "(into you)",
  "Doesn't matter how much",
  "your eyes reciprocate,",
  "(You)",
  "You can buy her a drink",
  "but she doesn’t have to",
  "reciprocate.",
  "(Gotta move on)",
  "If you pay for the day",
  "she doesn't have ...",
  // Add more lines as needed
};

const int numLyrics = sizeof(lyrics) / sizeof(lyrics[0]);
int currentLine = 0;

void setup() {
  lcd.begin(16, 2); // Set up the LCD's number of columns and rows
  lcd.clear();
}

void loop() {
  // Clear the LCD and print the current line of lyrics
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(lyrics[currentLine]);

  // Move to the next line of lyrics
  currentLine++;
  if (currentLine >= numLyrics) {
    currentLine = 0; // Loop back to the start
  }

  delay(3000); // Delay for 3 seconds before showing the next line
}
