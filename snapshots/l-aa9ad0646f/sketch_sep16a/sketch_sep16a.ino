#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// =====================================================
// WIFI - YOUR NETWORK
// =====================================================

const char* WIFI_SSID = "REDACTED_CONFIG_VALUE";
const char* WIFI_PASSWORD = "REDACTED_CONFIG_VALUE";

// =====================================================
// TFT
// =====================================================

#define TFT_CS    5
#define TFT_DC    27
#define TFT_RST   4
#define TFT_MOSI  23
#define TFT_SCLK  18

Adafruit_ST7789 tft =
  Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// =====================================================
// PCF8574
// =====================================================

#define I2C_SDA 21
#define I2C_SCL 22

#define BUTTON_A   0   // P0 = PLAY / PAUSE
#define BUTTON_B   1   // P1 = NEXT
#define BUTTON_UP  2   // P2 = PREVIOUS

uint8_t PCF_ADDRESS = 0x20;
bool pcfFound = false;

// =====================================================
// BUZZER
// =====================================================

#define BUZZER 26

// =====================================================
// SERVER
// =====================================================

WebServer server(80);

// =====================================================
// MEDIA DATA
// =====================================================

String songTitle = "Nothing Playing";
String songArtist = "";
String playbackState = "STOPPED";

unsigned long positionSec = 0;
unsigned long durationSec = 0;

unsigned long lastPCUpdate = 0;

String pendingCommand = "NONE";

// =====================================================
// BUTTON STATE
// =====================================================

bool lastA = HIGH;
bool lastB = HIGH;
bool lastUp = HIGH;

unsigned long lastButtonTime = 0;

// =====================================================
// ANIMATION
// =====================================================

unsigned long lastDiscAnimation = 0;
int discAngle = 0;

// =====================================================
// COLORS
// =====================================================

#define BG        0x0841
#define PANEL     0x10A2
#define PANEL2    0x18E3

#define WHITE     0xFFFF
#define BLACK     0x0000
#define CYAN      0x07FF
#define GREEN     0x07E0
#define RED       0xF800
#define YELLOW    0xFFE0
#define GRAY      0x8410
#define DARKGRAY  0x4208
#define PINK      0xF81F

// =====================================================
// PCF8574
// =====================================================

bool findPCF8574() {

  for (uint8_t address = 0x20; address <= 0x27; address++) {

    Wire.beginTransmission(address);

    if (Wire.endTransmission() == 0) {

      PCF_ADDRESS = address;

      Serial.print("PCF8574 FOUND: 0x");
      Serial.println(address, HEX);

      return true;
    }
  }

  for (uint8_t address = 0x38; address <= 0x3F; address++) {

    Wire.beginTransmission(address);

    if (Wire.endTransmission() == 0) {

      PCF_ADDRESS = address;

      Serial.print("PCF8574A FOUND: 0x");
      Serial.println(address, HEX);

      return true;
    }
  }

  return false;
}

void setupPCF() {

  Wire.beginTransmission(PCF_ADDRESS);

  // All HIGH = inputs
  Wire.write(0xFF);

  Wire.endTransmission();
}

uint8_t readPCF() {

  Wire.requestFrom(
    PCF_ADDRESS,
    (uint8_t)1
  );

  if (Wire.available()) {
    return Wire.read();
  }

  return 0xFF;
}

// =====================================================
// SOUNDS
// =====================================================

void soundPlay() {

  tone(BUZZER, 1200);
  delay(30);

  noTone(BUZZER);
}

void soundNext() {

  tone(BUZZER, 900);
  delay(30);

  tone(BUZZER, 1500);
  delay(40);

  noTone(BUZZER);
}

void soundPrevious() {

  tone(BUZZER, 1500);
  delay(30);

  tone(BUZZER, 900);
  delay(40);

  noTone(BUZZER);
}

void soundConnected() {

  tone(BUZZER, 900);
  delay(50);

  tone(BUZZER, 1400);
  delay(50);

  tone(BUZZER, 1900);
  delay(80);

  noTone(BUZZER);
}

// =====================================================
// HELPERS
// =====================================================

String shortText(String text, int maximum) {

  if (text.length() <= maximum)
    return text;

  return text.substring(0, maximum - 3) + "...";
}

String formatTime(unsigned long seconds) {

  unsigned int minutes = seconds / 60;
  unsigned int secs = seconds % 60;

  char buffer[10];

  sprintf(
    buffer,
    "%02u:%02u",
    minutes,
    secs
  );

  return String(buffer);
}

bool pcConnected() {

  return (
    millis() - lastPCUpdate
    < 5000
  );
}

// =====================================================
// HEADER
// =====================================================

void drawHeader() {

  tft.fillRoundRect(
    8,
    7,
    224,
    35,
    8,
    PANEL
  );

  tft.setTextSize(2);
  tft.setTextColor(CYAN);

  tft.setCursor(
    16,
    18
  );

  if (playbackState == "PLAYING")
    tft.print("NOW PLAYING");
  else
    tft.print("MEDIA PLAYER");

  // WiFi status
  tft.fillCircle(
    214,
    24,
    5,
    WiFi.status() == WL_CONNECTED
      ? GREEN
      : RED
  );
}

// =====================================================
// DISC
// =====================================================

void drawDisc() {

  const int cx = 120;
  const int cy = 105;

  // Clear disc section
  tft.fillRect(
    55,
    48,
    130,
    112,
    BG
  );

  tft.fillCircle(
    cx,
    cy,
    51,
    BLACK
  );

  tft.drawCircle(
    cx,
    cy,
    51,
    CYAN
  );

  tft.drawCircle(
    cx,
    cy,
    42,
    DARKGRAY
  );

  tft.drawCircle(
    cx,
    cy,
    33,
    DARKGRAY
  );

  tft.drawCircle(
    cx,
    cy,
    24,
    DARKGRAY
  );

  // Label
  tft.fillCircle(
    cx,
    cy,
    15,
    PINK
  );

  tft.fillCircle(
    cx,
    cy,
    5,
    BLACK
  );

  // Rotating marker
  float angle =
    discAngle * 0.0174532925;

  int markerX =
    cx + cos(angle) * 40;

  int markerY =
    cy + sin(angle) * 40;

  tft.fillCircle(
    markerX,
    markerY,
    4,
    WHITE
  );
}

// =====================================================
// TRACK INFO
// =====================================================

void drawSongInfo() {

  tft.fillRect(
    8,
    163,
    224,
    63,
    BG
  );

  String title =
    shortText(songTitle, 18);

  String artist =
    shortText(songArtist, 25);

  tft.setTextColor(WHITE);
  tft.setTextSize(2);

  tft.setCursor(
    14,
    168
  );

  tft.print(title);

  tft.setTextColor(GRAY);
  tft.setTextSize(1);

  tft.setCursor(
    15,
    194
  );

  if (artist.length() > 0)
    tft.print(artist);
  else
    tft.print("Windows Media");

  tft.setCursor(
    15,
    213
  );

  if (playbackState == "PLAYING") {

    tft.setTextColor(GREEN);
    tft.print("> PLAYING");

  }

  else if (playbackState == "PAUSED") {

    tft.setTextColor(YELLOW);
    tft.print("|| PAUSED");

  }

  else {

    tft.setTextColor(GRAY);
    tft.print("STOPPED");
  }
}

// =====================================================
// PROGRESS
// =====================================================

void drawProgress() {

  tft.fillRoundRect(
    10,
    230,
    220,
    44,
    7,
    PANEL
  );

  tft.setTextColor(WHITE);
  tft.setTextSize(1);

  tft.setCursor(
    18,
    240
  );

  tft.print(
    formatTime(positionSec)
  );

  tft.setCursor(
    187,
    240
  );

  tft.print(
    formatTime(durationSec)
  );

  tft.drawRoundRect(
    17,
    257,
    206,
    9,
    4,
    DARKGRAY
  );

  int width = 0;

  if (durationSec > 0) {

    width =
      (198UL * positionSec)
      / durationSec;

    if (width > 198)
      width = 198;
  }

  if (width > 0) {

    tft.fillRoundRect(
      21,
      260,
      width,
      3,
      2,
      CYAN
    );
  }
}

// =====================================================
// CONTROLS
// =====================================================

void drawControls() {

  tft.fillRoundRect(
    9,
    281,
    222,
    31,
    7,
    PANEL2
  );

  tft.setTextSize(1);

  tft.setTextColor(YELLOW);
  tft.setCursor(16, 293);
  tft.print("UP PREV");

  tft.setTextColor(CYAN);
  tft.setCursor(88, 293);
  tft.print("A PLAY");

  tft.setTextColor(GREEN);
  tft.setCursor(168, 293);
  tft.print("B NEXT");
}

// =====================================================
// MEDIA SCREEN
// =====================================================

void drawMediaScreen() {

  tft.fillScreen(BG);

  drawHeader();
  drawDisc();
  drawSongInfo();
  drawProgress();
  drawControls();
}

// =====================================================
// WAITING SCREEN
// =====================================================

void drawWaiting() {

  tft.fillScreen(BG);

  drawHeader();

  tft.drawCircle(
    120,
    103,
    46,
    DARKGRAY
  );

  tft.drawCircle(
    120,
    103,
    29,
    DARKGRAY
  );

  tft.setTextColor(CYAN);
  tft.setTextSize(4);

  tft.setCursor(
    108,
    86
  );

  tft.print("?");

  tft.setTextColor(WHITE);
  tft.setTextSize(2);

  tft.setCursor(
    25,
    169
  );

  tft.print(
    "WAITING FOR PC"
  );

  tft.setTextColor(GRAY);
  tft.setTextSize(1);

  tft.setCursor(
    31,
    201
  );

  tft.print(
    "Run media_companion.py"
  );

  tft.setCursor(
    46,
    219
  );

  tft.print(
    "on Windows"
  );

  tft.setTextColor(GREEN);

  tft.setCursor(
    42,
    248
  );

  tft.print(
    "WiFi connected"
  );

  drawControls();
}

// =====================================================
// HTTP UPDATE
// =====================================================

void handleUpdate() {

  bool previouslyConnected =
    pcConnected();

  if (server.hasArg("title"))
    songTitle =
      server.arg("title");

  if (server.hasArg("artist"))
    songArtist =
      server.arg("artist");

  if (server.hasArg("state"))
    playbackState =
      server.arg("state");

  if (server.hasArg("position"))
    positionSec =
      server.arg("position").toInt();

  if (server.hasArg("duration"))
    durationSec =
      server.arg("duration").toInt();

  lastPCUpdate =
    millis();

  if (!previouslyConnected)
    soundConnected();

  drawMediaScreen();

  server.send(
    200,
    "text/plain",
    "OK"
  );
}

// =====================================================
// COMMAND REQUEST
// =====================================================

void handleCommand() {

  server.send(
    200,
    "text/plain",
    pendingCommand
  );

  pendingCommand =
    "NONE";
}

// =====================================================
// ROOT
// =====================================================

void handleRoot() {

  String response;

  response +=
    "ESP32 Media Player\n";

  response +=
    "IP: ";

  response +=
    WiFi.localIP().toString();

  response +=
    "\nPC: ";

  response +=
    pcConnected()
      ? "CONNECTED"
      : "DISCONNECTED";

  response +=
    "\nTrack: ";

  response +=
    songTitle;

  response +=
    "\nArtist: ";

  response +=
    songArtist;

  server.send(
    200,
    "text/plain",
    response
  );
}

// =====================================================
// BUTTON INPUT
// =====================================================

void checkButtons() {

  if (!pcfFound)
    return;

  uint8_t data =
    readPCF();

  bool A =
    (data & (1 << BUTTON_A))
      ? HIGH
      : LOW;

  bool B =
    (data & (1 << BUTTON_B))
      ? HIGH
      : LOW;

  bool UP =
    (data & (1 << BUTTON_UP))
      ? HIGH
      : LOW;

  if (
    millis() - lastButtonTime
    > 150
  ) {

    if (
      A == LOW &&
      lastA == HIGH
    ) {

      pendingCommand =
        "PLAY_PAUSE";

      soundPlay();

      lastButtonTime =
        millis();
    }

    if (
      B == LOW &&
      lastB == HIGH
    ) {

      pendingCommand =
        "NEXT";

      soundNext();

      lastButtonTime =
        millis();
    }

    if (
      UP == LOW &&
      lastUp == HIGH
    ) {

      pendingCommand =
        "PREVIOUS";

      soundPrevious();

      lastButtonTime =
        millis();
    }
  }

  lastA = A;
  lastB = B;
  lastUp = UP;
}

// =====================================================
// DISC ANIMATION
// =====================================================

void animateDisc() {

  if (!pcConnected())
    return;

  if (playbackState != "PLAYING")
    return;

  if (
    millis() - lastDiscAnimation
    >= 180
  ) {

    lastDiscAnimation =
      millis();

    discAngle += 18;

    if (discAngle >= 360)
      discAngle = 0;

    drawDisc();
  }
}

// =====================================================
// CONNECTION WATCH
// =====================================================

void checkPC() {

  static bool previous =
    false;

  bool current =
    pcConnected();

  if (current != previous) {

    previous =
      current;

    if (current)
      drawMediaScreen();
    else
      drawWaiting();
  }
}

// =====================================================
// WIFI CONNECTING SCREEN
// =====================================================

void wifiScreen() {

  tft.fillScreen(BG);

  tft.setTextColor(CYAN);
  tft.setTextSize(3);

  tft.setCursor(
    38,
    87
  );

  tft.print(
    "MEDIA"
  );

  tft.setCursor(
    38,
    125
  );

  tft.print(
    "PLAYER"
  );

  tft.setTextColor(WHITE);
  tft.setTextSize(1);

  tft.setCursor(
    42,
    184
  );

  tft.print(
    "Connecting to:"
  );

  tft.setCursor(
    28,
    203
  );

  tft.print(
    WIFI_SSID
  );
}

// =====================================================
// WIFI ERROR
// =====================================================

void wifiError() {

  tft.fillScreen(RED);

  tft.setTextColor(WHITE);
  tft.setTextSize(2);

  tft.setCursor(
    35,
    105
  );

  tft.print(
    "WIFI ERROR"
  );

  tft.setTextSize(1);

  tft.setCursor(
    20,
    150
  );

  tft.print(
    "Could not connect to"
  );

  tft.setCursor(
    20,
    170
  );

  tft.print(
    WIFI_SSID
  );
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(
    115200
  );

  // BUZZER
  pinMode(
    BUZZER,
    OUTPUT
  );

  digitalWrite(
    BUZZER,
    LOW
  );

  // TFT
  SPI.begin(
    TFT_SCLK,
    -1,
    TFT_MOSI,
    TFT_CS
  );

  tft.init(
    240,
    320
  );

  // Vertical and flipped
  tft.setRotation(2);

  tft.invertDisplay(true);

  wifiScreen();

  // PCF8574
  Wire.begin(
    I2C_SDA,
    I2C_SCL
  );

  Wire.setClock(
    100000
  );

  delay(100);

  pcfFound =
    findPCF8574();

  if (pcfFound) {

    setupPCF();

  } else {

    Serial.println(
      "WARNING: PCF8574 NOT FOUND"
    );
  }

  // ===================================================
  // WIFI
  // ===================================================

  WiFi.mode(
    WIFI_STA
  );

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  int attempts = 0;

  while (
    WiFi.status() != WL_CONNECTED &&
    attempts < 60
  ) {

    delay(500);

    Serial.print(".");

    attempts++;
  }

  Serial.println();

  if (
    WiFi.status() != WL_CONNECTED
  ) {

    Serial.println(
      "WIFI FAILED"
    );

    wifiError();

    return;
  }

  Serial.println(
    "================================"
  );

  Serial.println(
    "WIFI CONNECTED!"
  );

  Serial.print(
    "ESP32 IP ADDRESS: "
  );

  Serial.println(
    WiFi.localIP()
  );

  Serial.println(
    "================================"
  );

  // ===================================================
  // HTTP SERVER
  // ===================================================

  server.on(
    "/",
    HTTP_GET,
    handleRoot
  );

  server.on(
    "/update",
    HTTP_POST,
    handleUpdate
  );

  server.on(
    "/command",
    HTTP_GET,
    handleCommand
  );

  server.begin();

  drawWaiting();
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  if (
    WiFi.status() != WL_CONNECTED
  ) {

    delay(100);
    return;
  }

  server.handleClient();

  checkButtons();

  animateDisc();

  checkPC();

  delay(5);
}
