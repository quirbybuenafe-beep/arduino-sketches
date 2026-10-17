// === Pin Definitions ===
const int redLED = 13;
const int yellowLED = 12;
const int greenLED = 11;
const int blueLED = 10;

const int redButton = 9;
const int yellowButton = 8;
const int greenButton = 7;
const int blueButton = 6;

const int buzzerPin = 5;

// === Game Setup ===
const int maxLevel = 50;
int sequence[maxLevel];
int level = 1;

// === Pin Arrays ===
int ledPins[] = {redLED, yellowLED, greenLED, blueLED};
int buttonPins[] = {redButton, yellowButton, greenButton, blueButton};
int tones[] = {262, 294, 330, 349}; // C4, D4, E4, F4

// === Setup ===
void setup() {
  for (int i = 0; i < 4; i++) {
    pinMode(ledPins[i], OUTPUT);
    pinMode(buttonPins[i], INPUT_PULLUP); // Buttons connect to GND
  }
  pinMode(buzzerPin, OUTPUT);

  randomSeed(analogRead(A0)); // Randomize sequence

  // Start sound
  tone(buzzerPin, 523, 150);
  delay(200);
  tone(buzzerPin, 659, 150);
  delay(200);
  tone(buzzerPin, 784, 150);
  delay(200);
  noTone(buzzerPin);
}

// === Main Loop ===
void loop() {
  generateSequence();
  playSequence();
  if (getUserInput()) {
    levelUp();
  } else {
    gameOver();
  }
}

// === Generate Random Sequence ===
void generateSequence() {
  sequence[level - 1] = random(0, 4);
}

// === Play LED & Sound Sequence ===
void playSequence() {
  for (int i = 0; i < level; i++) {
    int led = ledPins[sequence[i]];
    int toneValue = tones[sequence[i]];

    digitalWrite(led, HIGH);
    tone(buzzerPin, toneValue, 300);
    delay(400);
    digitalWrite(led, LOW);
    noTone(buzzerPin);
    delay(200);
  }
}

// === Get User Button Sequence ===
bool getUserInput() {
  for (int i = 0; i < level; i++) {
    int pressed = waitForButton();
    if (pressed != sequence[i]) {
      return false; // Wrong button
    }
  }
  return true; // Correct sequence
}

// === Wait for a Button Press ===
int waitForButton() {
  while (true) {
    for (int i = 0; i < 4; i++) {
      if (digitalRead(buttonPins[i]) == LOW) {
        flashLED(i);
        return i;
      }
    }
  }
}

// === Flash LED & Play Tone for Button ===
void flashLED(int index) {
  digitalWrite(ledPins[index], HIGH);
  tone(buzzerPin, tones[index], 200);
  delay(250);
  digitalWrite(ledPins[index], LOW);
  noTone(buzzerPin);
}

// === Level Up ===
void levelUp() {
  tone(buzzerPin, 800, 150);
  delay(200);
  tone(buzzerPin, 1000, 150);
  delay(200);
  noTone(buzzerPin);

  level++;
  if (level > maxLevel) level = maxLevel;
  delay(1000);
}

// === Game Over Sequence ===
void gameOver() {
  for (int i = 0; i < 3; i++) {
    tone(buzzerPin, 200, 300);
    for (int j = 0; j < 4; j++) digitalWrite(ledPins[j], HIGH);
    delay(300);
    for (int j = 0; j < 4; j++) digitalWrite(ledPins[j], LOW);
    delay(300);
  }
  noTone(buzzerPin);
  level = 1; // Restart game
  delay(1500);
}
