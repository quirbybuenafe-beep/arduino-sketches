#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Preferences.h>

// ============================================================
// MX CLUB - RHYTHM MEMORY BATTLE V9
//
// V9:
// - NO full-screen framebuffer
// - Small 230x100 buffered renderer
// - Difficulty selector does NOT refresh whole screen
// - Only old/new difficulty cards redraw
// - Animated title/menu enemy showcase
// - Snappy queued PCF8574 input
// - Correct fight direction ALWAYS works
// - Level 1 = UP
// - Every level adds ONE random move
// - Every 5th level = BOSS WAVE
//
// TFT:
// MOSI -> GPIO23
// SCK  -> GPIO18
// CS   -> GPIO5
// DC   -> GPIO27
// RST  -> GPIO33
//
// PCF8574:
// SDA -> GPIO21
// SCL -> GPIO22
//
// P0 START
// P1 STOP
// P2 UP
// P3 LEFT
// P4 DOWN
// P5 RIGHT
//
// PASSIVE BUZZER:
// GPIO15
// ============================================================


// ============================================================
// TFT
// ============================================================

#define TFT_CS   5
#define TFT_DC   27
#define TFT_RST  33
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_MISO -1

Adafruit_ST7789 tft(
  TFT_CS,
  TFT_DC,
  TFT_RST
);


// ============================================================
// SMALL BUFFER
//
// 230 x 100 x 2 = about 46 KB
// Safe compared with a full 240x320 framebuffer.
// ============================================================

#define REGION_W 230
#define REGION_H 100

GFXcanvas16 region(
  REGION_W,
  REGION_H
);


// ============================================================
// SCREEN
// ============================================================

#define SCREEN_W 240
#define SCREEN_H 320


// ============================================================
// I2C / PCF8574
// ============================================================

#define I2C_SDA 21
#define I2C_SCL 22

uint8_t pcfAddress =
  0x27;

bool pcfFound =
  false;


// ============================================================
// PCF PINS
// ============================================================

#define PCF_START 0
#define PCF_STOP  1
#define PCF_UP    2
#define PCF_LEFT  3
#define PCF_DOWN  4
#define PCF_RIGHT 5


// ============================================================
// BUTTON MASKS
// ============================================================

#define BTN_START 0x01
#define BTN_STOP  0x02
#define BTN_UP    0x04
#define BTN_LEFT  0x08
#define BTN_DOWN  0x10
#define BTN_RIGHT 0x20


// ============================================================
// BUZZER
// ============================================================

#define BUZZER_PIN 15


// ============================================================
// COLORS
// ============================================================

#define C_BLACK      0x0000
#define C_WHITE      0xFFFF

#define C_BG         0x000C
#define C_GRID       0x0843

#define C_PANEL      0x10A4
#define C_PANEL2     0x18E6
#define C_DARK       0x0842

#define C_CYAN       0x07FF
#define C_BLUE       0x041F

#define C_PINK       0xF81F
#define C_PURPLE     0xB19C

#define C_GREEN      0x07E0
#define C_RED        0xF800

#define C_YELLOW     0xFFE0
#define C_ORANGE     0xFD20

#define C_GREY       0x8410
#define C_DIM        0x4208

#define C_HPBG       0x2945


const uint16_t directionColor[4] = {

  C_CYAN,

  C_PINK,

  C_YELLOW,

  C_GREEN
};


const uint8_t directionButtonMask[4] = {

  BTN_UP,

  BTN_LEFT,

  BTN_DOWN,

  BTN_RIGHT
};


const char* directionName[4] = {

  "UP",

  "LEFT",

  "DOWN",

  "RIGHT"
};


// ============================================================
// SAVE DATA
// ============================================================

Preferences prefs;


// ============================================================
// GAME STATES
// ============================================================

enum GameState {

  STATE_TITLE,

  STATE_DIFFICULTY,

  STATE_WATCH,

  STATE_PLAY,

  STATE_LEVEL_CLEAR,

  STATE_ENEMY_ATTACK,

  STATE_GAME_OVER,

  STATE_WIN
};


GameState gameState =
  STATE_TITLE;


unsigned long stateStart =
  0;


// ============================================================
// DRAW FLAGS
// ============================================================

bool screenDirty =
  true;

bool enemyDirty =
  false;

bool statusDirty =
  false;

bool dpadDirty =
  false;

bool indicatorDirty =
  false;

bool menuAnimDirty =
  false;


// Bits 0-3 correspond to EASY/NORMAL/HARD/INSANE.

uint8_t difficultyDirtyMask =
  0;


// ============================================================
// DIFFICULTY
// ============================================================

int difficulty =
  1;


const char* difficultyName[4] = {

  "EASY",

  "NORMAL",

  "HARD",

  "INSANE"
};


const char* difficultyDescription[4] = {

  "RELAXED",

  "BALANCED",

  "FAST",

  "BRUTAL"
};


// ============================================================
// PLAYER
// ============================================================

int levelNumber =
  1;


const int FINAL_LEVEL =
  12;


int bestLevel =
  1;


int hearts =
  3;


int combo =
  0;


int bestCombo =
  0;


int misses =
  0;


int wrongThisRound =
  0;


// ============================================================
// ENEMY
//
// 0 SLIME
// 1 CRAB
// 2 GHOST
// 3 ROBOT
// 4 BOSS
// ============================================================

int enemyType =
  0;


int enemyHP =
  1;


int enemyMaxHP =
  1;


unsigned long enemyHitUntil =
  0;


unsigned long lastIdleEnemyFrame =
  0;


unsigned long lastFastEnemyFrame =
  0;


// ============================================================
// MEMORY SEQUENCE
// ============================================================

#define MAX_SEQUENCE 24

uint8_t sequenceDirection[MAX_SEQUENCE];

uint8_t sequenceGapType[MAX_SEQUENCE];

uint32_t sequenceTime[MAX_SEQUENCE];


int sequenceCount =
  0;


int sequenceIndex =
  0;


uint16_t beatLength =
  700;


// ============================================================
// PLAYER RHYTHM TIMER
//
// Timing only changes score feedback.
// It never blocks a correct direction.
// ============================================================

unsigned long playerTurnStarted =
  0;


// ============================================================
// BUTTON READER
// ============================================================

uint8_t stableRaw =
  0xFF;


uint8_t lastSampleRaw =
  0xFF;


uint8_t buttonsHeld =
  0;


uint8_t pendingPresses =
  0;


unsigned long sampleChangedAt =
  0;


unsigned long lastButtonPoll =
  0;


const unsigned long DEBOUNCE_MS =
  7;


// ============================================================
// LAST INPUT
// ============================================================

char lastInputText[18] =
  "NONE";


unsigned long lastInputUntil =
  0;


bool inputWasVisible =
  false;


// ============================================================
// D-PAD VISUALS
// ============================================================

unsigned long padFlashUntil[4] = {

  0,
  0,
  0,
  0
};


uint8_t lastPadVisualMask =
  0;


// ============================================================
// FEEDBACK
// ============================================================

char feedbackText[24] =
  "";


uint16_t feedbackColor =
  C_WHITE;


unsigned long feedbackUntil =
  0;


bool feedbackWasVisible =
  false;


// ============================================================
// ATTACK FX
// ============================================================

bool attackActive =
  false;


unsigned long attackStarted =
  0;


int attackDirection =
  0;


bool attackPerfect =
  false;


// ============================================================
// MENU ANIMATION
// ============================================================

unsigned long lastMenuFrame =
  0;


uint8_t menuEnemyType =
  0;


// ============================================================
// OTHER ANIMATION TIMER
// ============================================================

unsigned long lastAttackFrame =
  0;


// ============================================================
// MELODY
// ============================================================

struct MelodyStep {

  uint16_t frequency;

  uint16_t duration;

  uint16_t gap;
};


const MelodyStep clearSong[] = {

  {523, 70, 15},

  {659, 70, 15},

  {784, 90, 20},

  {1047, 180, 40}
};


const MelodyStep failSong[] = {

  {330, 100, 20},

  {247, 130, 20},

  {180, 180, 30}
};


const MelodyStep winSong[] = {

  {523, 70, 15},

  {659, 70, 15},

  {784, 70, 15},

  {1047, 90, 15},

  {1319, 100, 15},

  {1568, 240, 40}
};


const MelodyStep* activeMelody =
  NULL;


int melodyLength =
  0;


int melodyIndex =
  0;


unsigned long nextMelodyTime =
  0;


// ============================================================
// CENTER TEXT ON ANY GFX TARGET
// ============================================================

void centerTextOn(
  Adafruit_GFX &g,
  const char* text,
  int width,
  int y,
  uint16_t color,
  uint8_t size
) {

  int16_t x1;
  int16_t y1;

  uint16_t w;
  uint16_t h;


  g.setTextSize(
    size
  );


  g.getTextBounds(
    (char*)text,
    0,
    y,
    &x1,
    &y1,
    &w,
    &h
  );


  g.setTextColor(
    color
  );


  g.setCursor(
    (
      width -
      w
    ) /
    2,
    y
  );


  g.print(
    text
  );
}


// ============================================================
// 3D PANEL
// ============================================================

void panel3DOn(
  Adafruit_GFX &g,
  int x,
  int y,
  int w,
  int h,
  uint16_t fill,
  uint16_t edge
) {

  g.fillRoundRect(
    x + 4,
    y + 5,
    w,
    h,
    6,
    C_BLACK
  );


  g.fillRoundRect(
    x,
    y,
    w,
    h,
    6,
    fill
  );


  g.drawRoundRect(
    x,
    y,
    w,
    h,
    6,
    edge
  );


  g.drawFastHLine(
    x + 8,
    y + 2,
    w - 16,
    C_WHITE
  );


  g.drawFastHLine(
    x + 8,
    y + h - 3,
    w - 16,
    C_BLACK
  );
}


// ============================================================
// PIXEL
// ============================================================

void pxOn(
  Adafruit_GFX &g,
  int x,
  int y,
  int size,
  uint16_t color
) {

  g.fillRect(
    x,
    y,
    size,
    size,
    color
  );
}


// ============================================================
// HEART
// ============================================================

void drawHeartOn(
  Adafruit_GFX &g,
  int x,
  int y,
  uint16_t color
) {

  g.fillRect(
    x + 3,
    y,
    3,
    3,
    color
  );


  g.fillRect(
    x + 9,
    y,
    3,
    3,
    color
  );


  g.fillRect(
    x,
    y + 3,
    15,
    3,
    color
  );


  g.fillRect(
    x,
    y + 6,
    15,
    3,
    color
  );


  g.fillRect(
    x + 3,
    y + 9,
    9,
    3,
    color
  );


  g.fillRect(
    x + 6,
    y + 12,
    3,
    3,
    color
  );


  g.drawPixel(
    x + 3,
    y + 3,
    C_WHITE
  );
}


// ============================================================
// I2C ADDRESS CHECK
// ============================================================

bool addressExists(
  uint8_t address
) {

  Wire.beginTransmission(
    address
  );


  return
    Wire.endTransmission() ==
    0;
}


// ============================================================
// FIND PCF8574
// ============================================================

void detectPCF() {

  pcfFound =
    false;


  if (
    addressExists(
      0x27
    )
  ) {

    pcfAddress =
      0x27;


    pcfFound =
      true;


    return;
  }


  for (
    uint8_t address = 0x20;
    address <= 0x27;
    address++
  ) {

    if (
      addressExists(
        address
      )
    ) {

      pcfAddress =
        address;


      pcfFound =
        true;


      return;
    }
  }
}


// ============================================================
// PCF WRITE
// ============================================================

void pcfWrite(
  uint8_t value
) {

  if (
    !pcfFound
  ) {

    return;
  }


  Wire.beginTransmission(
    pcfAddress
  );


  Wire.write(
    value
  );


  Wire.endTransmission();
}


// ============================================================
// PCF READ
// ============================================================

uint8_t readPCFRaw() {

  if (
    !pcfFound
  ) {

    return
      0xFF;
  }


  Wire.requestFrom(
    (uint8_t)pcfAddress,
    (uint8_t)1
  );


  if (
    Wire.available()
  ) {

    return
      Wire.read();
  }


  return
    stableRaw;
}


// ============================================================
// RAW -> BUTTON MASK
// ============================================================

uint8_t rawToButtons(
  uint8_t raw
) {

  uint8_t result =
    0;


  if (
    !(raw & (1 << PCF_START))
  ) {

    result |=
      BTN_START;
  }


  if (
    !(raw & (1 << PCF_STOP))
  ) {

    result |=
      BTN_STOP;
  }


  if (
    !(raw & (1 << PCF_UP))
  ) {

    result |=
      BTN_UP;
  }


  if (
    !(raw & (1 << PCF_LEFT))
  ) {

    result |=
      BTN_LEFT;
  }


  if (
    !(raw & (1 << PCF_DOWN))
  ) {

    result |=
      BTN_DOWN;
  }


  if (
    !(raw & (1 << PCF_RIGHT))
  ) {

    result |=
      BTN_RIGHT;
  }


  return
    result;
}


// ============================================================
// RECORD INPUT
// ============================================================

void recordInput(
  const char* name
) {

  strncpy(
    lastInputText,
    name,
    sizeof(lastInputText) - 1
  );


  lastInputText[
    sizeof(lastInputText) - 1
  ] =
    0;


  lastInputUntil =
    millis() +
    500;


  indicatorDirty =
    true;


  statusDirty =
    true;


  Serial.print(
    "INPUT: "
  );


  Serial.println(
    name
  );
}


// ============================================================
// BUTTON UPDATE
// ============================================================

void updateButtons() {

  if (
    !pcfFound
  ) {

    return;
  }


  if (
    millis() -
    lastButtonPoll <
    2
  ) {

    return;
  }


  lastButtonPoll =
    millis();


  uint8_t sample =
    readPCFRaw();


  if (
    sample !=
    lastSampleRaw
  ) {

    lastSampleRaw =
      sample;


    sampleChangedAt =
      millis();


    return;
  }


  if (
    millis() -
    sampleChangedAt <
    DEBOUNCE_MS
  ) {

    return;
  }


  if (
    sample ==
    stableRaw
  ) {

    return;
  }


  uint8_t previousRaw =
    stableRaw;


  uint8_t previousHeld =
    buttonsHeld;


  stableRaw =
    sample;


  buttonsHeld =
    rawToButtons(
      stableRaw
    );


  if (
    previousHeld !=
    buttonsHeld
  ) {

    dpadDirty =
      true;
  }


  uint8_t physicalPress =
    previousRaw &
    ~stableRaw;


  if (
    physicalPress &
    (1 << PCF_START)
  ) {

    pendingPresses |=
      BTN_START;


    recordInput(
      "START"
    );
  }


  if (
    physicalPress &
    (1 << PCF_STOP)
  ) {

    pendingPresses |=
      BTN_STOP;


    recordInput(
      "STOP"
    );
  }


  if (
    physicalPress &
    (1 << PCF_UP)
  ) {

    pendingPresses |=
      BTN_UP;


    recordInput(
      "UP"
    );
  }


  if (
    physicalPress &
    (1 << PCF_LEFT)
  ) {

    pendingPresses |=
      BTN_LEFT;


    recordInput(
      "LEFT"
    );
  }


  if (
    physicalPress &
    (1 << PCF_DOWN)
  ) {

    pendingPresses |=
      BTN_DOWN;


    recordInput(
      "DOWN"
    );
  }


  if (
    physicalPress &
    (1 << PCF_RIGHT)
  ) {

    pendingPresses |=
      BTN_RIGHT;


    recordInput(
      "RIGHT"
    );
  }
}


// ============================================================
// CONSUME PRESS
// ============================================================

bool consumePress(
  uint8_t mask
) {

  if (
    pendingPresses &
    mask
  ) {

    pendingPresses &=
      ~mask;


    return
      true;
  }


  return
    false;
}


// ============================================================
// HELD
// ============================================================

bool held(
  uint8_t mask
) {

  return
    (
      buttonsHeld &
      mask
    ) !=
    0;
}


// ============================================================
// CLEAR EVENTS
// ============================================================

void clearPending() {

  pendingPresses =
    0;
}


// ============================================================
// SOUND
// ============================================================

void playTone(
  uint16_t frequency,
  uint16_t duration
) {

  tone(
    BUZZER_PIN,
    frequency,
    duration
  );
}


// ============================================================
// DIRECTION NOTES
// ============================================================

uint16_t directionFrequency(
  int direction
) {

  if (
    direction == 0
  ) {

    return
      988;
  }


  if (
    direction == 1
  ) {

    return
      1319;
  }


  if (
    direction == 2
  ) {

    return
      1568;
  }


  return
    1976;
}


// ============================================================
// MELODY
// ============================================================

void startMelody(
  const MelodyStep* melody,
  int length
) {

  activeMelody =
    melody;


  melodyLength =
    length;


  melodyIndex =
    0;


  nextMelodyTime =
    0;
}


void stopMelody() {

  activeMelody =
    NULL;


  melodyLength =
    0;


  melodyIndex =
    0;
}


void updateMelody() {

  if (
    activeMelody ==
    NULL
  ) {

    return;
  }


  if (
    millis() <
    nextMelodyTime
  ) {

    return;
  }


  if (
    melodyIndex >=
    melodyLength
  ) {

    stopMelody();


    return;
  }


  MelodyStep step =
    activeMelody[
      melodyIndex
    ];


  playTone(
    step.frequency,
    step.duration
  );


  nextMelodyTime =
    millis() +
    step.duration +
    step.gap;


  melodyIndex++;
}


// ============================================================
// FEEDBACK
// ============================================================

void showFeedback(
  const char* text,
  uint16_t color,
  unsigned long duration
) {

  strncpy(
    feedbackText,
    text,
    sizeof(feedbackText) - 1
  );


  feedbackText[
    sizeof(feedbackText) - 1
  ] =
    0;


  feedbackColor =
    color;


  feedbackUntil =
    millis() +
    duration;


  statusDirty =
    true;
}


// ============================================================
// ENEMY NAME
// ============================================================

const char* enemyName(
  int type
) {

  if (
    type == 0
  ) {

    return
      "BEAT SLIME";
  }


  if (
    type == 1
  ) {

    return
      "DRUM CRAB";
  }


  if (
    type == 2
  ) {

    return
      "NOTE GHOST";
  }


  if (
    type == 3
  ) {

    return
      "CORE BOT";
  }


  return
    "OMEGA DJ";
}


// ============================================================
// SLIME
// ============================================================

void drawSlime(
  Adafruit_GFX &g,
  int ox,
  int oy,
  int s,
  bool anim,
  bool hurt
) {

  uint16_t body =
    hurt ?
    C_WHITE :
    C_GREEN;


  for (
    int x = 2;
    x <= 5;
    x++
  ) {

    pxOn(
      g,
      ox + x * s,
      oy,
      s,
      body
    );
  }


  for (
    int x = 1;
    x <= 6;
    x++
  ) {

    pxOn(
      g,
      ox + x * s,
      oy + s,
      s,
      body
    );
  }


  for (
    int y = 2;
    y <= 5;
    y++
  ) {

    for (
      int x = 0;
      x <= 7;
      x++
    ) {

      pxOn(
        g,
        ox + x * s,
        oy + y * s,
        s,
        body
      );
    }
  }


  pxOn(
    g,
    ox + 2 * s,
    oy + 3 * s,
    s,
    C_BLACK
  );


  pxOn(
    g,
    ox + 5 * s,
    oy + 3 * s,
    s,
    C_BLACK
  );


  if (
    anim
  ) {

    pxOn(
      g,
      ox + 3 * s,
      oy + 5 * s,
      s,
      C_BLACK
    );


    pxOn(
      g,
      ox + 4 * s,
      oy + 5 * s,
      s,
      C_BLACK
    );
  }


  else {

    pxOn(
      g,
      ox + 3 * s,
      oy + 4 * s,
      s,
      C_BLACK
    );
  }
}


// ============================================================
// CRAB
// ============================================================

void drawCrab(
  Adafruit_GFX &g,
  int ox,
  int oy,
  int s,
  bool anim,
  bool hurt
) {

  uint16_t body =
    hurt ?
    C_WHITE :
    C_RED;


  for (
    int y = 2;
    y <= 5;
    y++
  ) {

    for (
      int x = 1;
      x <= 6;
      x++
    ) {

      pxOn(
        g,
        ox + x * s,
        oy + y * s,
        s,
        body
      );
    }
  }


  pxOn(
    g,
    ox + 2 * s,
    oy + 2 * s,
    s,
    C_WHITE
  );


  pxOn(
    g,
    ox + 5 * s,
    oy + 2 * s,
    s,
    C_WHITE
  );


  pxOn(
    g,
    ox + 2 * s,
    oy + 3 * s,
    s,
    C_BLACK
  );


  pxOn(
    g,
    ox + 5 * s,
    oy + 3 * s,
    s,
    C_BLACK
  );


  int movement =
    anim ?
    1 :
    0;


  pxOn(
    g,
    ox,
    oy +
    (
      2 +
      movement
    ) *
    s,
    s,
    body
  );


  pxOn(
    g,
    ox + 7 * s,
    oy +
    (
      2 -
      movement
    ) *
    s,
    s,
    body
  );


  pxOn(
    g,
    ox + s,
    oy + 6 * s,
    s,
    body
  );


  pxOn(
    g,
    ox + 6 * s,
    oy + 6 * s,
    s,
    body
  );
}


// ============================================================
// GHOST
// ============================================================

void drawGhost(
  Adafruit_GFX &g,
  int ox,
  int oy,
  int s,
  bool anim,
  bool hurt
) {

  uint16_t body =
    hurt ?
    C_WHITE :
    C_PURPLE;


  for (
    int y = 1;
    y <= 6;
    y++
  ) {

    int first =
      y < 3 ?
      2 :
      1;


    int last =
      y < 3 ?
      4 :
      5;


    for (
      int x = first;
      x <= last;
      x++
    ) {

      pxOn(
        g,
        ox + x * s,
        oy + y * s,
        s,
        body
      );
    }
  }


  pxOn(
    g,
    ox + s,
    oy + 7 * s,
    s,
    body
  );


  pxOn(
    g,
    ox + 3 * s,
    oy +
    (
      anim ?
      8 :
      7
    ) *
    s,
    s,
    body
  );


  pxOn(
    g,
    ox + 5 * s,
    oy + 7 * s,
    s,
    body
  );


  pxOn(
    g,
    ox + 2 * s,
    oy + 4 * s,
    s,
    C_WHITE
  );


  pxOn(
    g,
    ox + 4 * s,
    oy + 4 * s,
    s,
    C_WHITE
  );


  pxOn(
    g,
    ox + 2 * s,
    oy + 5 * s,
    s,
    C_BLACK
  );


  pxOn(
    g,
    ox + 4 * s,
    oy + 5 * s,
    s,
    C_BLACK
  );
}


// ============================================================
// ROBOT
// ============================================================

void drawRobot(
  Adafruit_GFX &g,
  int ox,
  int oy,
  int s,
  bool anim,
  bool hurt
) {

  uint16_t body =
    hurt ?
    C_WHITE :
    C_GREY;


  uint16_t light =
    anim ?
    C_CYAN :
    C_YELLOW;


  for (
    int y = 2;
    y <= 6;
    y++
  ) {

    for (
      int x = 1;
      x <= 6;
      x++
    ) {

      pxOn(
        g,
        ox + x * s,
        oy + y * s,
        s,
        body
      );
    }
  }


  pxOn(
    g,
    ox + 4 * s,
    oy,
    s,
    light
  );


  pxOn(
    g,
    ox + 4 * s,
    oy + s,
    s,
    light
  );


  pxOn(
    g,
    ox + 2 * s,
    oy + 4 * s,
    s,
    C_BLACK
  );


  pxOn(
    g,
    ox + 5 * s,
    oy + 4 * s,
    s,
    C_BLACK
  );


  pxOn(
    g,
    ox + 3 * s,
    oy + 5 * s,
    s,
    C_CYAN
  );


  pxOn(
    g,
    ox + 4 * s,
    oy + 5 * s,
    s,
    C_CYAN
  );
}


// ============================================================
// BOSS
// ============================================================

void drawBoss(
  Adafruit_GFX &g,
  int ox,
  int oy,
  int s,
  bool anim,
  bool hurt
) {

  uint16_t body =
    hurt ?
    C_WHITE :
    C_ORANGE;


  for (
    int y = 2;
    y <= 7;
    y++
  ) {

    for (
      int x = 1;
      x <= 6;
      x++
    ) {

      pxOn(
        g,
        ox + x * s,
        oy + y * s,
        s,
        body
      );
    }
  }


  // horns

  pxOn(
    g,
    ox,
    oy,
    s,
    C_YELLOW
  );


  pxOn(
    g,
    ox + s,
    oy + s,
    s,
    C_YELLOW
  );


  pxOn(
    g,
    ox + 7 * s,
    oy,
    s,
    C_YELLOW
  );


  pxOn(
    g,
    ox + 6 * s,
    oy + s,
    s,
    C_YELLOW
  );


  // eyes

  pxOn(
    g,
    ox + 2 * s,
    oy + 4 * s,
    s,
    C_BLACK
  );


  pxOn(
    g,
    ox + 5 * s,
    oy + 4 * s,
    s,
    C_BLACK
  );


  if (
    anim
  ) {

    pxOn(
      g,
      ox + 3 * s,
      oy + 6 * s,
      s,
      C_BLACK
    );


    pxOn(
      g,
      ox + 4 * s,
      oy + 6 * s,
      s,
      C_BLACK
    );
  }


  else {

    pxOn(
      g,
      ox + 3 * s,
      oy + 5 * s,
      s,
      C_BLACK
    );


    pxOn(
      g,
      ox + 4 * s,
      oy + 5 * s,
      s,
      C_BLACK
    );
  }
}


// ============================================================
// DRAW ENEMY
// ============================================================

void drawEnemyOn(
  Adafruit_GFX &g,
  int centerX,
  int centerY,
  int type
) {

  bool hurt =
    millis() <
    enemyHitUntil;


  bool anim =
    (
      millis() /
      240
    ) %
    2;


  int bob =
    anim ?
    2 :
    0;


  int shake =
    0;


  if (
    hurt
  ) {

    shake =
      (
        millis() /
        28
      ) %
      2 ?
      4 :
      -4;
  }


  int ox =
    centerX -
    20 +
    shake;


  int oy =
    centerY -
    18 +
    bob;


  g.fillRoundRect(
    centerX - 28,
    centerY + 29,
    56,
    7,
    3,
    C_BLACK
  );


  if (
    type == 0
  ) {

    drawSlime(
      g,
      ox,
      oy,
      5,
      anim,
      hurt
    );
  }


  else if (
    type == 1
  ) {

    drawCrab(
      g,
      ox,
      oy,
      5,
      anim,
      hurt
    );
  }


  else if (
    type == 2
  ) {

    drawGhost(
      g,
      ox,
      oy,
      5,
      anim,
      hurt
    );
  }


  else if (
    type == 3
  ) {

    drawRobot(
      g,
      ox,
      oy,
      5,
      anim,
      hurt
    );
  }


  else {

    drawBoss(
      g,
      ox,
      oy,
      5,
      anim,
      hurt
    );
  }
}


// ============================================================
// HP BAR
// ============================================================

void drawHPBarOn(
  Adafruit_GFX &g
) {

  const int x =
    34;


  const int y =
    82;


  const int w =
    162;


  const int h =
    10;


  g.fillRoundRect(
    x,
    y,
    w,
    h,
    3,
    C_HPBG
  );


  g.drawRoundRect(
    x,
    y,
    w,
    h,
    3,
    C_WHITE
  );


  if (
    enemyMaxHP <=
    0
  ) {

    return;
  }


  int fillWidth =
    (
      enemyHP *
      (
        w -
        4
      )
    ) /
    enemyMaxHP;


  if (
    fillWidth <
    0
  ) {

    fillWidth =
      0;
  }


  uint16_t color =
    C_GREEN;


  if (
    levelNumber %
    5 ==
    0
  ) {

    color =
      C_ORANGE;
  }


  else if (
    enemyHP *
    2 <=
    enemyMaxHP
  ) {

    color =
      C_YELLOW;
  }


  if (
    enemyHP *
    3 <=
      enemyMaxHP &&
    levelNumber %
      5 !=
      0
  ) {

    color =
      C_RED;
  }


  if (
    fillWidth >
    0
  ) {

    g.fillRoundRect(
      x + 2,
      y + 2,
      fillWidth,
      h - 4,
      2,
      color
    );
  }
}


// ============================================================
// LEVEL SPEED
// ============================================================

void updateLevelTiming() {

  int bpm =
    68 +
    difficulty *
    10 +
    (
      levelNumber -
      1
    ) *
    4;


  if (
    bpm >
    160
  ) {

    bpm =
      160;
  }


  beatLength =
    60000 /
    bpm;
}


// ============================================================
// RANDOM GAP TYPE
// ============================================================

uint8_t randomGapType() {

  int r =
    random(
      100
    );


  if (
    difficulty ==
    0
  ) {

    return
      0;
  }


  if (
    difficulty ==
    1
  ) {

    if (
      r <
      80
    ) {

      return
        0;
    }


    return
      1;
  }


  if (
    difficulty ==
    2
  ) {

    if (
      r <
      65
    ) {

      return
        0;
    }


    if (
      r <
      90
    ) {

      return
        2;
    }


    return
      1;
  }


  if (
    r <
    50
  ) {

    return
      0;
  }


  if (
    r <
    80
  ) {

    return
      2;
  }


  return
    3;
}


// ============================================================
// GAP TIME
// ============================================================

uint16_t gapTime(
  uint8_t type
) {

  if (
    type ==
    1
  ) {

    return
      beatLength +
      beatLength /
      2;
  }


  if (
    type ==
    2
  ) {

    return
      beatLength /
      2;
  }


  if (
    type ==
    3
  ) {

    return
      (
        beatLength *
        3
      ) /
      4;
  }


  return
    beatLength;
}


// ============================================================
// REBUILD SEQUENCE TIMES
// ============================================================

void rebuildSequenceTimes() {

  if (
    sequenceCount <=
    0
  ) {

    return;
  }


  sequenceTime[0] =
    1000;


  for (
    int i = 1;
    i < sequenceCount;
    i++
  ) {

    sequenceTime[i] =
      sequenceTime[
        i - 1
      ] +
      gapTime(
        sequenceGapType[i]
      );
  }
}


// ============================================================
// FIRST MOVE
// ============================================================

void createFirstMove() {

  sequenceCount =
    1;


  sequenceDirection[0] =
    0;


  sequenceGapType[0] =
    0;


  rebuildSequenceTimes();
}


// ============================================================
// ADD ONE RANDOM MOVE
// ============================================================

void appendOneRandomMove() {

  if (
    sequenceCount >=
    MAX_SEQUENCE
  ) {

    return;
  }


  int direction =
    random(
      4
    );


  // Repeats are allowed.
  // Only block three identical inputs in a row.

  if (
    sequenceCount >=
    2
  ) {

    int last =
      sequenceDirection[
        sequenceCount -
        1
      ];


    int before =
      sequenceDirection[
        sequenceCount -
        2
      ];


    if (
      last ==
        before &&
      direction ==
        last
    ) {

      direction =
        (
          direction +
          random(
            1,
            4
          )
        ) %
        4;
    }
  }


  sequenceDirection[
    sequenceCount
  ] =
    direction;


  sequenceGapType[
    sequenceCount
  ] =
    randomGapType();


  sequenceCount++;


  rebuildSequenceTimes();
}


// ============================================================
// CONFIGURE ENEMY
//
// EVERY 5TH LEVEL IS A BOSS.
// ============================================================

void configureEnemy() {

  if (
    levelNumber %
      5 ==
    0
  ) {

    enemyType =
      4;
  }


  else if (
    levelNumber <=
    2
  ) {

    enemyType =
      0;
  }


  else if (
    levelNumber <=
    4
  ) {

    enemyType =
      1;
  }


  else if (
    levelNumber <=
    7
  ) {

    enemyType =
      2;
  }


  else {

    enemyType =
      3;
  }


  enemyMaxHP =
    sequenceCount;


  enemyHP =
    enemyMaxHP;


  enemyDirty =
    true;
}


// ============================================================
// DEBUG SEQUENCE
// ============================================================

void printSequence() {

  Serial.print(
    "LEVEL "
  );


  Serial.print(
    levelNumber
  );


  if (
    levelNumber %
      5 ==
    0
  ) {

    Serial.print(
      " BOSS"
    );
  }


  Serial.print(
    ": "
  );


  for (
    int i = 0;
    i < sequenceCount;
    i++
  ) {

    Serial.print(
      directionName[
        sequenceDirection[i]
      ]
    );


    if (
      i <
      sequenceCount -
      1
    ) {

      Serial.print(
        " -> "
      );
    }
  }


  Serial.println();
}


// ============================================================
// BATTLE STATE?
// ============================================================

bool isBattleState(
  GameState state
) {

  return
    state ==
      STATE_WATCH ||
    state ==
      STATE_PLAY ||
    state ==
      STATE_LEVEL_CLEAR ||
    state ==
      STATE_ENEMY_ATTACK;
}


// ============================================================
// CHANGE STATE
// ============================================================

void changeState(
  GameState newState
) {

  GameState oldState =
    gameState;


  gameState =
    newState;


  stateStart =
    millis();


  clearPending();


  if (
    isBattleState(
      oldState
    ) &&
    isBattleState(
      newState
    )
  ) {

    enemyDirty =
      true;


    statusDirty =
      true;


    dpadDirty =
      true;
  }


  else {

    screenDirty =
      true;
  }


  if (
    newState ==
    STATE_TITLE
  ) {

    menuAnimDirty =
      true;
  }


  if (
    newState ==
    STATE_DIFFICULTY
  ) {

    difficultyDirtyMask =
      0x0F;
  }
}


// ============================================================
// START GAME
// ============================================================

void startGame() {

  levelNumber =
    1;


  hearts =
    3;


  combo =
    0;


  bestCombo =
    0;


  misses =
    0;


  wrongThisRound =
    0;


  updateLevelTiming();


  createFirstMove();


  configureEnemy();


  sequenceIndex =
    0;


  printSequence();


  changeState(
    STATE_WATCH
  );
}


// ============================================================
// START WATCH
// ============================================================

void startWatch() {

  updateLevelTiming();


  rebuildSequenceTimes();


  sequenceIndex =
    0;


  combo =
    0;


  misses =
    0;


  wrongThisRound =
    0;


  enemyHP =
    enemyMaxHP;


  printSequence();


  changeState(
    STATE_WATCH
  );
}


// ============================================================
// START PLAYER TURN
// ============================================================

void startPlayerTurn() {

  sequenceIndex =
    0;


  combo =
    0;


  misses =
    0;


  wrongThisRound =
    0;


  enemyHP =
    enemyMaxHP;


  playerTurnStarted =
    millis();


  changeState(
    STATE_PLAY
  );
}


// ============================================================
// WATCH PHASE
// ============================================================

void updateWatch() {

  uint32_t elapsed =
    millis() -
    stateStart;


  if (
    sequenceIndex <
    sequenceCount
  ) {

    if (
      elapsed >=
      sequenceTime[
        sequenceIndex
      ]
    ) {

      int direction =
        sequenceDirection[
          sequenceIndex
        ];


      padFlashUntil[
        direction
      ] =
        millis() +
        220;


      dpadDirty =
        true;


      playTone(
        directionFrequency(
          direction
        ),
        120
      );


      sequenceIndex++;


      statusDirty =
        true;
    }
  }


  else {

    uint32_t finalTime =
      sequenceTime[
        sequenceCount -
        1
      ];


    if (
      elapsed >
      finalTime +
      650
    ) {

      startPlayerTurn();
    }
  }
}


// ============================================================
// GET DIRECTION
// ============================================================

int getDirectionPress() {

  if (
    consumePress(
      BTN_UP
    )
  ) {

    return
      0;
  }


  if (
    consumePress(
      BTN_LEFT
    )
  ) {

    return
      1;
  }


  if (
    consumePress(
      BTN_DOWN
    )
  ) {

    return
      2;
  }


  if (
    consumePress(
      BTN_RIGHT
    )
  ) {

    return
      3;
  }


  return
    -1;
}


// ============================================================
// START PLAYER ATTACK
// ============================================================

void startPlayerAttack(
  int direction,
  bool perfect
) {

  attackActive =
    true;


  attackStarted =
    millis();


  attackDirection =
    direction;


  attackPerfect =
    perfect;


  enemyDirty =
    true;
}


// ============================================================
// CORRECT INPUT
// ============================================================

void registerCorrectMove(
  int direction
) {

  unsigned long elapsed =
    millis() -
    playerTurnStarted;


  long timingError =
    (
      long
    )elapsed -
    (
      long
    )sequenceTime[
      sequenceIndex
    ];


  long absError =
    labs(
      timingError
    );


  bool perfect =
    absError <=
    120;


  combo++;


  if (
    combo >
    bestCombo
  ) {

    bestCombo =
      combo;
  }


  enemyHP--;


  if (
    enemyHP <
    0
  ) {

    enemyHP =
      0;
  }


  padFlashUntil[
    direction
  ] =
    millis() +
    160;


  enemyHitUntil =
    millis() +
    220;


  dpadDirty =
    true;


  enemyDirty =
    true;


  statusDirty =
    true;


  startPlayerAttack(
    direction,
    perfect
  );


  if (
    absError <=
    120
  ) {

    showFeedback(
      "PERFECT!",
      C_CYAN,
      450
    );


    playTone(
      directionFrequency(
        direction
      ) +
      120,
      90
    );
  }


  else if (
    absError <=
    500
  ) {

    showFeedback(
      "GOOD!",
      C_GREEN,
      420
    );


    playTone(
      directionFrequency(
        direction
      ),
      85
    );
  }


  else {

    showFeedback(
      "OFFBEAT!",
      C_YELLOW,
      420
    );


    playTone(
      directionFrequency(
        direction
      ),
      75
    );
  }


  sequenceIndex++;


  statusDirty =
    true;


  if (
    sequenceIndex >=
      sequenceCount &&
    enemyHP <=
      0
  ) {

    if (
      levelNumber >
      bestLevel
    ) {

      bestLevel =
        levelNumber;


      prefs.putInt(
        "best",
        bestLevel
      );
    }


    startMelody(
      clearSong,
      sizeof(clearSong) /
      sizeof(clearSong[0])
    );


    changeState(
      STATE_LEVEL_CLEAR
    );
  }
}


// ============================================================
// WRONG INPUT
// ============================================================

void registerWrongMove(
  int direction
) {

  misses++;


  wrongThisRound++;


  combo =
    0;


  padFlashUntil[
    direction
  ] =
    millis() +
    160;


  dpadDirty =
    true;


  statusDirty =
    true;


  showFeedback(
    "WRONG!",
    C_RED,
    450
  );


  playTone(
    160,
    100
  );


  // Wrong button DOES NOT advance the sequence.


  if (
    wrongThisRound >=
    3
  ) {

    hearts--;


    enemyDirty =
      true;


    if (
      hearts <=
      0
    ) {

      hearts =
        0;


      startMelody(
        failSong,
        sizeof(failSong) /
        sizeof(failSong[0])
      );


      changeState(
        STATE_GAME_OVER
      );
    }


    else {

      startMelody(
        failSong,
        sizeof(failSong) /
        sizeof(failSong[0])
      );


      changeState(
        STATE_ENEMY_ATTACK
      );
    }
  }
}


// ============================================================
// PLAYER TURN
// ============================================================

void updatePlayerTurn() {

  if (
    consumePress(
      BTN_STOP
    )
  ) {

    changeState(
      STATE_TITLE
    );


    return;
  }


  if (
    sequenceIndex >=
    sequenceCount
  ) {

    return;
  }


  int pressed =
    getDirectionPress();


  if (
    pressed <
    0
  ) {

    return;
  }


  int expected =
    sequenceDirection[
      sequenceIndex
    ];


  if (
    pressed ==
    expected
  ) {

    registerCorrectMove(
      pressed
    );
  }


  else {

    registerWrongMove(
      pressed
    );
  }
}


// ============================================================
// UPDATE GAME
// ============================================================

void updateGame() {

  // ----------------------------------------------------------
  // TITLE
  // ----------------------------------------------------------

  if (
    gameState ==
    STATE_TITLE
  ) {

    if (
      consumePress(
        BTN_START
      )
    ) {

      changeState(
        STATE_DIFFICULTY
      );
    }
  }


  // ----------------------------------------------------------
  // DIFFICULTY
  // ----------------------------------------------------------

  else if (
    gameState ==
    STATE_DIFFICULTY
  ) {

    if (
      consumePress(
        BTN_UP
      )
    ) {

      int oldDifficulty =
        difficulty;


      difficulty--;


      if (
        difficulty <
        0
      ) {

        difficulty =
          3;
      }


      difficultyDirtyMask |=
        (
          1 <<
          oldDifficulty
        );


      difficultyDirtyMask |=
        (
          1 <<
          difficulty
        );


      playTone(
        650,
        35
      );
    }


    if (
      consumePress(
        BTN_DOWN
      )
    ) {

      int oldDifficulty =
        difficulty;


      difficulty++;


      if (
        difficulty >
        3
      ) {

        difficulty =
          0;
      }


      difficultyDirtyMask |=
        (
          1 <<
          oldDifficulty
        );


      difficultyDirtyMask |=
        (
          1 <<
          difficulty
        );


      playTone(
        750,
        35
      );
    }


    if (
      consumePress(
        BTN_START
      )
    ) {

      startGame();
    }


    if (
      consumePress(
        BTN_STOP
      )
    ) {

      changeState(
        STATE_TITLE
      );
    }
  }


  // ----------------------------------------------------------
  // WATCH
  // ----------------------------------------------------------

  else if (
    gameState ==
    STATE_WATCH
  ) {

    if (
      consumePress(
        BTN_STOP
      )
    ) {

      changeState(
        STATE_TITLE
      );
    }


    else {

      updateWatch();
    }
  }


  // ----------------------------------------------------------
  // PLAY
  // ----------------------------------------------------------

  else if (
    gameState ==
    STATE_PLAY
  ) {

    updatePlayerTurn();
  }


  // ----------------------------------------------------------
  // LEVEL CLEAR
  // ----------------------------------------------------------

  else if (
    gameState ==
    STATE_LEVEL_CLEAR
  ) {

    if (
      millis() -
      stateStart >
      1200
    ) {

      levelNumber++;


      if (
        levelNumber >
        FINAL_LEVEL
      ) {

        if (
          FINAL_LEVEL >
          bestLevel
        ) {

          bestLevel =
            FINAL_LEVEL;


          prefs.putInt(
            "best",
            bestLevel
          );
        }


        startMelody(
          winSong,
          sizeof(winSong) /
          sizeof(winSong[0])
        );


        changeState(
          STATE_WIN
        );
      }


      else {

        updateLevelTiming();


        appendOneRandomMove();


        rebuildSequenceTimes();


        configureEnemy();


        if (
          levelNumber %
            4 ==
          0 &&
          hearts <
          5
        ) {

          hearts++;
        }


        startWatch();
      }
    }
  }


  // ----------------------------------------------------------
  // ENEMY ATTACK
  // ----------------------------------------------------------

  else if (
    gameState ==
    STATE_ENEMY_ATTACK
  ) {

    if (
      millis() -
      stateStart >
      1200
    ) {

      startWatch();
    }
  }


  // ----------------------------------------------------------
  // GAME OVER
  // ----------------------------------------------------------

  else if (
    gameState ==
    STATE_GAME_OVER
  ) {

    if (
      consumePress(
        BTN_START
      )
    ) {

      changeState(
        STATE_TITLE
      );
    }
  }


  // ----------------------------------------------------------
  // WIN
  // ----------------------------------------------------------

  else if (
    gameState ==
    STATE_WIN
  ) {

    if (
      consumePress(
        BTN_START
      )
    ) {

      changeState(
        STATE_TITLE
      );
    }
  }
}


// ============================================================
// ARROWS
// ============================================================

void arrowUpOn(
  Adafruit_GFX &g,
  int cx,
  int cy,
  uint16_t color
) {

  g.fillTriangle(
    cx,
    cy - 12,
    cx - 11,
    cy,
    cx + 11,
    cy,
    color
  );


  g.fillRect(
    cx - 4,
    cy,
    8,
    12,
    color
  );
}


void arrowDownOn(
  Adafruit_GFX &g,
  int cx,
  int cy,
  uint16_t color
) {

  g.fillRect(
    cx - 4,
    cy - 12,
    8,
    12,
    color
  );


  g.fillTriangle(
    cx - 11,
    cy,
    cx + 11,
    cy,
    cx,
    cy + 12,
    color
  );
}


void arrowLeftOn(
  Adafruit_GFX &g,
  int cx,
  int cy,
  uint16_t color
) {

  g.fillTriangle(
    cx - 12,
    cy,
    cx,
    cy - 11,
    cx,
    cy + 11,
    color
  );


  g.fillRect(
    cx,
    cy - 4,
    12,
    8,
    color
  );
}


void arrowRightOn(
  Adafruit_GFX &g,
  int cx,
  int cy,
  uint16_t color
) {

  g.fillRect(
    cx - 12,
    cy - 4,
    12,
    8,
    color
  );


  g.fillTriangle(
    cx,
    cy - 11,
    cx + 12,
    cy,
    cx,
    cy + 11,
    color
  );
}


// ============================================================
// D-PAD BUTTON
// ============================================================

void drawPadButtonOn(
  Adafruit_GFX &g,
  int direction,
  int x,
  int y
) {

  bool active =
    millis() <
    padFlashUntil[
      direction
    ];


  if (
    held(
      directionButtonMask[
        direction
      ]
    )
  ) {

    active =
      true;
  }


  uint16_t color =
    directionColor[
      direction
    ];


  g.fillRoundRect(
    x + 4,
    y + 5,
    46,
    42,
    7,
    C_BLACK
  );


  g.fillRoundRect(
    x,
    y,
    46,
    42,
    7,
    active ?
    color :
    C_PANEL
  );


  g.drawRoundRect(
    x,
    y,
    46,
    42,
    7,
    active ?
    C_WHITE :
    color
  );


  g.drawFastHLine(
    x + 8,
    y + 3,
    30,
    C_WHITE
  );


  g.drawFastHLine(
    x + 8,
    y + 38,
    30,
    C_BLACK
  );


  int cx =
    x +
    23;


  int cy =
    y +
    20;


  uint16_t arrowColor =
    active ?
    C_BLACK :
    color;


  if (
    direction ==
    0
  ) {

    arrowUpOn(
      g,
      cx,
      cy,
      arrowColor
    );
  }


  else if (
    direction ==
    1
  ) {

    arrowLeftOn(
      g,
      cx,
      cy,
      arrowColor
    );
  }


  else if (
    direction ==
    2
  ) {

    arrowDownOn(
      g,
      cx,
      cy,
      arrowColor
    );
  }


  else {

    arrowRightOn(
      g,
      cx,
      cy,
      arrowColor
    );
  }
}


// ============================================================
// PLAYER ATTACK IN ENEMY REGION
// ============================================================

void drawAttackInEnemyRegion() {

  if (
    !attackActive
  ) {

    return;
  }


  unsigned long age =
    millis() -
    attackStarted;


  if (
    age >
    300
  ) {

    attackActive =
      false;


    enemyDirty =
      true;


    return;
  }


  int targetX =
    115;


  int targetY =
    50;


  int startX =
    115;


  int startY =
    94;


  if (
    attackDirection ==
    1
  ) {

    startX =
      12;


    startY =
      82;
  }


  else if (
    attackDirection ==
    3
  ) {

    startX =
      218;


    startY =
      82;
  }


  int progress =
    age >=
      170 ?
    255 :
    (
      age *
      255
    ) /
    170;


  int x =
    startX +
    (
      (
        targetX -
        startX
      ) *
      progress
    ) /
    255;


  int y =
    startY +
    (
      (
        targetY -
        startY
      ) *
      progress
    ) /
    255;


  uint16_t color =
    directionColor[
      attackDirection
    ];


  region.drawLine(
    startX,
    startY,
    x,
    y,
    color
  );


  region.drawLine(
    startX + 1,
    startY,
    x + 1,
    y,
    C_WHITE
  );


  region.fillCircle(
    x,
    y,
    attackPerfect ?
    6 :
    4,
    color
  );


  region.fillCircle(
    x,
    y,
    2,
    C_WHITE
  );


  if (
    age >
    150
  ) {

    int spark =
      (
        age -
        150
      ) /
      7;


    if (
      spark >
      15
    ) {

      spark =
        15;
    }


    region.drawLine(
      targetX - spark,
      targetY,
      targetX + spark,
      targetY,
      C_YELLOW
    );


    region.drawLine(
      targetX,
      targetY - spark,
      targetX,
      targetY + spark,
      C_CYAN
    );


    region.drawLine(
      targetX - spark,
      targetY - spark,
      targetX + spark,
      targetY + spark,
      C_WHITE
    );
  }
}


// ============================================================
// ENEMY REGION
// ============================================================

void renderEnemyRegion() {

  region.fillScreen(
    C_DARK
  );


  region.setTextSize(
    1
  );


  region.setTextColor(
    C_WHITE
  );


  region.setCursor(
    8,
    5
  );


  region.print(
    "LV "
  );


  region.print(
    levelNumber
  );


  region.setCursor(
    46,
    5
  );


  region.print(
    "SEQ "
  );


  region.print(
    sequenceCount
  );


  if (
    levelNumber %
      5 ==
    0
  ) {

    region.fillRoundRect(
      84,
      1,
      70,
      15,
      3,
      C_RED
    );


    region.drawRoundRect(
      84,
      1,
      70,
      15,
      3,
      C_YELLOW
    );


    region.setTextColor(
      C_WHITE
    );


    region.setCursor(
      96,
      5
    );


    region.print(
      "BOSS WAVE"
    );
  }


  else if (
    gameState ==
    STATE_PLAY
  ) {

    region.setCursor(
      92,
      5
    );


    region.print(
      "STEP "
    );


    region.print(
      sequenceIndex +
      1
    );


    region.print(
      "/"
    );


    region.print(
      sequenceCount
    );
  }


  for (
    int i = 0;
    i < hearts;
    i++
  ) {

    drawHeartOn(
      region,
      158 +
      i *
      15,
      2,
      C_RED
    );
  }


  centerTextOn(
    region,
    enemyName(
      enemyType
    ),
    REGION_W,
    18,
    levelNumber %
      5 ==
      0 ?
    C_ORANGE :
    C_WHITE,
    1
  );


  drawEnemyOn(
    region,
    115,
    52,
    enemyType
  );


  drawHPBarOn(
    region
  );


  drawAttackInEnemyRegion();


  tft.drawRGBBitmap(
    5,
    20,
    region.getBuffer(),
    REGION_W,
    95
  );


  updateButtons();
}


// ============================================================
// STATUS REGION
// ============================================================

void renderStatusRegion() {

  region.fillScreen(
    C_PANEL
  );


  if (
    gameState ==
    STATE_WATCH
  ) {

    if (
      levelNumber %
        5 ==
      0
    ) {

      centerTextOn(
        region,
        "BOSS RHYTHM!",
        REGION_W,
        4,
        C_ORANGE,
        2
      );
    }


    else {

      centerTextOn(
        region,
        "WATCH!",
        REGION_W,
        4,
        C_YELLOW,
        2
      );
    }


    char text[30];


    snprintf(
      text,
      sizeof(text),
      "MEMORIZE %d MOVE%s",
      sequenceCount,
      sequenceCount ==
        1 ?
      "" :
      "S"
    );


    centerTextOn(
      region,
      text,
      REGION_W,
      27,
      C_WHITE,
      1
    );
  }


  else if (
    gameState ==
    STATE_PLAY
  ) {

    centerTextOn(
      region,
      "YOUR TURN!",
      REGION_W,
      4,
      C_GREEN,
      2
    );


    char text[30];


    snprintf(
      text,
      sizeof(text),
      "STEP %d OF %d",
      sequenceIndex +
      1,
      sequenceCount
    );


    centerTextOn(
      region,
      text,
      REGION_W,
      27,
      C_WHITE,
      1
    );
  }


  else if (
    gameState ==
    STATE_LEVEL_CLEAR
  ) {

    centerTextOn(
      region,
      levelNumber %
        5 ==
        0 ?
      "BOSS DEFEATED!" :
      "ENEMY DOWN!",
      REGION_W,
      4,
      C_GREEN,
      2
    );


    centerTextOn(
      region,
      "+1 RANDOM MOVE",
      REGION_W,
      27,
      C_YELLOW,
      1
    );
  }


  else if (
    gameState ==
    STATE_ENEMY_ATTACK
  ) {

    centerTextOn(
      region,
      "ENEMY ATTACK!",
      REGION_W,
      4,
      C_RED,
      2
    );


    centerTextOn(
      region,
      "RETRY THE CHAIN",
      REGION_W,
      27,
      C_WHITE,
      1
    );


    if (
      (
        millis() /
        90
      ) %
      2
    ) {

      region.drawRect(
        2,
        2,
        226,
        66,
        C_RED
      );
    }
  }


  if (
    millis() <
    feedbackUntil
  ) {

    centerTextOn(
      region,
      feedbackText,
      REGION_W,
      43,
      feedbackColor,
      1
    );
  }


  else {

    char text[36];


    snprintf(
      text,
      sizeof(text),
      "COMBO %d   WRONG %d/3",
      combo,
      wrongThisRound
    );


    centerTextOn(
      region,
      text,
      REGION_W,
      43,
      C_WHITE,
      1
    );
  }


  int visible =
    sequenceCount;


  if (
    visible >
    12
  ) {

    visible =
      12;
  }


  if (
    visible >
    0
  ) {

    int spacing =
      190 /
      visible;


    int startX =
      (
        REGION_W -
        spacing *
        visible
      ) /
      2 +
      spacing /
      2;


    for (
      int i = 0;
      i < visible;
      i++
    ) {

      uint16_t color =
        C_DIM;


      if (
        i <
        sequenceIndex
      ) {

        color =
          C_GREEN;
      }


      int x =
        startX +
        i *
        spacing;


      region.fillCircle(
        x,
        61,
        3,
        color
      );


      region.drawCircle(
        x,
        61,
        4,
        C_WHITE
      );
    }
  }


  if (
    millis() <
    lastInputUntil
  ) {

    char inputLine[28];


    snprintf(
      inputLine,
      sizeof(inputLine),
      "INPUT %s",
      lastInputText
    );


    centerTextOn(
      region,
      inputLine,
      REGION_W,
      70,
      C_CYAN,
      1
    );
  }


  tft.drawRGBBitmap(
    5,
    125,
    region.getBuffer(),
    REGION_W,
    74
  );


  updateButtons();
}


// ============================================================
// D-PAD REGION
// ============================================================

void renderDPadRegion() {

  region.fillScreen(
    0x0008
  );


  drawPadButtonOn(
    region,
    0,
    92,
    0
  );


  drawPadButtonOn(
    region,
    1,
    43,
    45
  );


  drawPadButtonOn(
    region,
    2,
    92,
    45
  );


  drawPadButtonOn(
    region,
    3,
    141,
    45
  );


  region.setTextSize(
    1
  );


  region.setTextColor(
    C_DIM
  );


  region.setCursor(
    3,
    89
  );


  region.print(
    "START"
  );


  region.setCursor(
    194,
    89
  );


  region.print(
    "STOP"
  );


  if (
    gameState ==
    STATE_ENEMY_ATTACK
  ) {

    if (
      (
        millis() /
        80
      ) %
      2
    ) {

      region.drawRoundRect(
        37,
        0,
        160,
        94,
        8,
        C_RED
      );


      region.drawRoundRect(
        38,
        1,
        158,
        92,
        8,
        C_ORANGE
      );
    }
  }


  tft.drawRGBBitmap(
    5,
    210,
    region.getBuffer(),
    REGION_W,
    100
  );


  updateButtons();
}


// ============================================================
// BACKGROUND
// ============================================================

void drawBackground() {

  tft.fillScreen(
    C_BG
  );


  for (
    int x = 0;
    x < SCREEN_W;
    x += 24
  ) {

    tft.drawFastVLine(
      x,
      0,
      SCREEN_H,
      C_GRID
    );
  }


  for (
    int y = 0;
    y < SCREEN_H;
    y += 24
  ) {

    tft.drawFastHLine(
      0,
      y,
      SCREEN_W,
      C_GRID
    );
  }
}


// ============================================================
// TITLE BASE
//
// Animation panel is NOT drawn here.
// It gets its own buffered renderer.
// ============================================================

void drawTitleBase() {

  drawBackground();


  panel3DOn(
    tft,
    10,
    10,
    220,
    300,
    C_DARK,
    C_CYAN
  );


  centerTextOn(
    tft,
    "MX CLUB",
    SCREEN_W,
    23,
    C_CYAN,
    2
  );


  centerTextOn(
    tft,
    "RHYTHM",
    SCREEN_W,
    50,
    C_WHITE,
    3
  );


  centerTextOn(
    tft,
    "MEMORY BATTLE",
    SCREEN_W,
    81,
    C_WHITE,
    2
  );


  panel3DOn(
    tft,
    42,
    211,
    156,
    38,
    C_PANEL,
    C_GREEN
  );


  centerTextOn(
    tft,
    "PRESS START",
    SCREEN_W,
    223,
    C_GREEN,
    2
  );


  centerTextOn(
    tft,
    "BUILD THE CHAIN",
    SCREEN_W,
    263,
    C_YELLOW,
    1
  );


  centerTextOn(
    tft,
    "BOSS EVERY 5 LEVELS",
    SCREEN_W,
    279,
    C_WHITE,
    1
  );


  menuAnimDirty =
    true;


  indicatorDirty =
    true;
}


// ============================================================
// ANIMATED MENU SHOWCASE
//
// Full animation is rendered OFF-SCREEN first.
// ============================================================

void renderMenuAnimation() {

  region.fillScreen(
    C_DARK
  );


  panel3DOn(
    region,
    5,
    2,
    220,
    94,
    C_PANEL,
    C_PURPLE
  );


  unsigned long now =
    millis();


  int pulse =
    (
      now /
      80
    ) %
    12;


  if (
    pulse >
    6
  ) {

    pulse =
      12 -
      pulse;
  }


  region.drawCircle(
    115,
    50,
    31 +
    pulse,
    C_PURPLE
  );


  region.drawCircle(
    115,
    50,
    24 +
    pulse,
    C_CYAN
  );


  // Flying pixel particles

  for (
    int i = 0;
    i < 10;
    i++
  ) {

    int x =
      (
        i *
        29 +
        now /
        18
      ) %
      214 +
      8;


    int y =
      12 +
      (
        i *
        17
      ) %
      72;


    region.fillRect(
      x,
      y,
      2,
      2,
      i %
        2 ?
      C_CYAN :
      C_PINK
    );
  }


  menuEnemyType =
    (
      now /
      1800
    ) %
    5;


  // No hurt/shake on menu enemy.

  unsigned long savedHit =
    enemyHitUntil;


  enemyHitUntil =
    0;


  drawEnemyOn(
    region,
    115,
    49,
    menuEnemyType
  );


  enemyHitUntil =
    savedHit;


  region.fillRoundRect(
    13,
    74,
    65,
    14,
    3,
    C_BLACK
  );


  region.drawRoundRect(
    13,
    74,
    65,
    14,
    3,
    C_CYAN
  );


  region.setTextSize(
    1
  );


  region.setTextColor(
    C_WHITE
  );


  region.setCursor(
    20,
    78
  );


  region.print(
    enemyName(
      menuEnemyType
    )
  );


  tft.drawRGBBitmap(
    5,
    103,
    region.getBuffer(),
    REGION_W,
    100
  );


  updateButtons();
}


// ============================================================
// TITLE INPUT INDICATOR
// ============================================================

void drawTitleInputIndicator() {

  region.fillScreen(
    C_DARK
  );


  if (
    !pcfFound
  ) {

    centerTextOn(
      region,
      "PCF NOT FOUND",
      REGION_W,
      3,
      C_RED,
      1
    );
  }


  else if (
    millis() <
    lastInputUntil
  ) {

    char text[28];


    snprintf(
      text,
      sizeof(text),
      "INPUT: %s",
      lastInputText
    );


    centerTextOn(
      region,
      text,
      REGION_W,
      3,
      C_GREEN,
      1
    );
  }


  else {

    char text[28];


    snprintf(
      text,
      sizeof(text),
      "PCF 0x%02X READY",
      pcfAddress
    );


    centerTextOn(
      region,
      text,
      REGION_W,
      3,
      C_CYAN,
      1
    );
  }


  tft.drawRGBBitmap(
    5,
    296,
    region.getBuffer(),
    REGION_W,
    16
  );
}


// ============================================================
// DIFFICULTY BASE
//
// Drawn only ONCE when entering this menu.
// ============================================================

void drawDifficultyBase() {

  drawBackground();


  panel3DOn(
    tft,
    10,
    10,
    220,
    300,
    C_DARK,
    C_PURPLE
  );


  centerTextOn(
    tft,
    "SELECT MODE",
    SCREEN_W,
    22,
    C_CYAN,
    2
  );


  centerTextOn(
    tft,
    "NO SCREEN REFRESH",
    SCREEN_W,
    44,
    C_DIM,
    1
  );


  centerTextOn(
    tft,
    "UP / DOWN SELECT",
    SCREEN_W,
    252,
    C_WHITE,
    1
  );


  centerTextOn(
    tft,
    "START = PLAY",
    SCREEN_W,
    267,
    C_GREEN,
    1
  );


  centerTextOn(
    tft,
    "STOP = BACK",
    SCREEN_W,
    282,
    C_RED,
    1
  );


  difficultyDirtyMask =
    0x0F;


  indicatorDirty =
    true;
}


// ============================================================
// ONE DIFFICULTY CARD
//
// Only this row gets redrawn.
// No fillScreen.
// ============================================================

void renderDifficultyOption(
  int index
) {

  region.fillScreen(
    C_DARK
  );


  bool selected =
    index ==
    difficulty;


  uint16_t edge =
    selected ?
    C_WHITE :
    C_DIM;


  uint16_t fill =
    selected ?
    C_CYAN :
    C_PANEL;


  panel3DOn(
    region,
    28,
    2,
    174,
    38,
    fill,
    edge
  );


  if (
    selected
  ) {

    region.fillTriangle(
      12,
      21,
      21,
      14,
      21,
      28,
      C_YELLOW
    );
  }


  region.setTextSize(
    2
  );


  region.setTextColor(
    selected ?
    C_BLACK :
    C_WHITE
  );


  region.setCursor(
    43,
    8
  );


  region.print(
    difficultyName[
      index
    ]
  );


  region.setTextSize(
    1
  );


  region.setTextColor(
    selected ?
    C_BLUE :
    C_DIM
  );


  region.setCursor(
    43,
    27
  );


  region.print(
    difficultyDescription[
      index
    ]
  );


  if (
    selected
  ) {

    region.setTextColor(
      C_BLACK
    );


    region.setCursor(
      164,
      16
    );


    region.print(
      ">"
    );
  }


  int screenY =
    67 +
    index *
    44;


  tft.drawRGBBitmap(
    5,
    screenY,
    region.getBuffer(),
    REGION_W,
    42
  );


  updateButtons();
}


// ============================================================
// BATTLE BASE
// ============================================================

void drawBattleBase() {

  drawBackground();


  panel3DOn(
    tft,
    5,
    5,
    230,
    112,
    C_DARK,
    levelNumber %
      5 ==
      0 ?
    C_ORANGE :
    C_CYAN
  );


  tft.setTextSize(
    1
  );


  tft.setTextColor(
    C_CYAN
  );


  tft.setCursor(
    12,
    12
  );


  tft.print(
    "MX CLUB"
  );


  tft.setTextColor(
    C_WHITE
  );


  tft.setCursor(
    67,
    12
  );


  tft.print(
    levelNumber %
      5 ==
      0 ?
    "BOSS BATTLE" :
    "MEMORY BATTLE"
  );


  panel3DOn(
    tft,
    5,
    120,
    230,
    80,
    C_PANEL,
    C_PURPLE
  );


  panel3DOn(
    tft,
    5,
    205,
    230,
    108,
    0x0008,
    C_CYAN
  );


  enemyDirty =
    true;


  statusDirty =
    true;


  dpadDirty =
    true;
}


// ============================================================
// GAME OVER
// ============================================================

void drawGameOver() {

  drawBackground();


  panel3DOn(
    tft,
    10,
    10,
    220,
    300,
    C_DARK,
    C_RED
  );


  centerTextOn(
    tft,
    "GAME OVER",
    SCREEN_W,
    42,
    C_RED,
    3
  );


  drawEnemyOn(
    tft,
    120,
    130,
    enemyType
  );


  char text[30];


  snprintf(
    text,
    sizeof(text),
    "LEVEL %d",
    levelNumber
  );


  centerTextOn(
    tft,
    text,
    SCREEN_W,
    190,
    C_YELLOW,
    2
  );


  snprintf(
    text,
    sizeof(text),
    "%d MOVE CHAIN",
    sequenceCount
  );


  centerTextOn(
    tft,
    text,
    SCREEN_W,
    220,
    C_CYAN,
    1
  );


  centerTextOn(
    tft,
    "PRESS START",
    SCREEN_W,
    273,
    C_GREEN,
    2
  );
}


// ============================================================
// WIN
// ============================================================

void drawWin() {

  drawBackground();


  panel3DOn(
    tft,
    10,
    10,
    220,
    300,
    C_DARK,
    C_GREEN
  );


  centerTextOn(
    tft,
    "RAID CLEAR!",
    SCREEN_W,
    42,
    C_GREEN,
    3
  );


  drawEnemyOn(
    tft,
    120,
    130,
    4
  );


  centerTextOn(
    tft,
    "MEMORY MASTER",
    SCREEN_W,
    196,
    C_CYAN,
    2
  );


  centerTextOn(
    tft,
    "PRESS START",
    SCREEN_W,
    273,
    C_GREEN,
    2
  );
}


// ============================================================
// WHOLE SCREEN
// ============================================================

void drawWholeScreen() {

  if (
    gameState ==
    STATE_TITLE
  ) {

    drawTitleBase();
  }


  else if (
    gameState ==
    STATE_DIFFICULTY
  ) {

    drawDifficultyBase();
  }


  else if (
    isBattleState(
      gameState
    )
  ) {

    drawBattleBase();
  }


  else if (
    gameState ==
    STATE_GAME_OVER
  ) {

    drawGameOver();
  }


  else {

    drawWin();
  }
}


// ============================================================
// UPDATE VISUAL FLAGS
// ============================================================

void updateVisualDirtyFlags() {

  unsigned long now =
    millis();


  // ----------------------------------------------------------
  // TITLE ANIMATION
  // ----------------------------------------------------------

  if (
    gameState ==
    STATE_TITLE
  ) {

    if (
      now -
      lastMenuFrame >=
      150
    ) {

      lastMenuFrame =
        now;


      menuAnimDirty =
        true;
    }
  }


  // ----------------------------------------------------------
  // D-PAD VISUAL
  // ----------------------------------------------------------

  uint8_t padMask =
    0;


  for (
    int i = 0;
    i < 4;
    i++
  ) {

    bool active =
      now <
      padFlashUntil[
        i
      ];


    if (
      held(
        directionButtonMask[
          i
        ]
      )
    ) {

      active =
        true;
    }


    if (
      active
    ) {

      padMask |=
        (
          1 <<
          i
        );
    }
  }


  if (
    padMask !=
    lastPadVisualMask
  ) {

    lastPadVisualMask =
      padMask;


    dpadDirty =
      true;
  }


  // ----------------------------------------------------------
  // FEEDBACK DISAPPEARS
  // ----------------------------------------------------------

  bool feedbackVisible =
    now <
    feedbackUntil;


  if (
    feedbackVisible !=
    feedbackWasVisible
  ) {

    feedbackWasVisible =
      feedbackVisible;


    statusDirty =
      true;
  }


  // ----------------------------------------------------------
  // INPUT INDICATOR
  // ----------------------------------------------------------

  bool inputVisible =
    now <
    lastInputUntil;


  if (
    inputVisible !=
    inputWasVisible
  ) {

    inputWasVisible =
      inputVisible;


    indicatorDirty =
      true;


    statusDirty =
      true;
  }


  // ----------------------------------------------------------
  // IDLE ENEMY
  // ----------------------------------------------------------

  if (
    isBattleState(
      gameState
    ) &&
    !attackActive &&
    now >=
      enemyHitUntil
  ) {

    if (
      now -
      lastIdleEnemyFrame >=
      240
    ) {

      lastIdleEnemyFrame =
        now;


      enemyDirty =
        true;
    }
  }


  // ----------------------------------------------------------
  // FAST ATTACK/HIT
  // ----------------------------------------------------------

  if (
    attackActive ||
    now <
      enemyHitUntil
  ) {

    if (
      now -
      lastFastEnemyFrame >=
      28
    ) {

      lastFastEnemyFrame =
        now;


      enemyDirty =
        true;
    }
  }


  // ----------------------------------------------------------
  // ENEMY ATTACK PULSE
  // ----------------------------------------------------------

  if (
    gameState ==
    STATE_ENEMY_ATTACK
  ) {

    if (
      now -
      lastAttackFrame >=
      80
    ) {

      lastAttackFrame =
        now;


      statusDirty =
        true;


      dpadDirty =
        true;
    }
  }
}


// ============================================================
// FLUSH DIFFICULTY ROWS
// ============================================================

void flushDifficultyCards() {

  if (
    gameState !=
    STATE_DIFFICULTY
  ) {

    return;
  }


  for (
    int i = 0;
    i < 4;
    i++
  ) {

    if (
      difficultyDirtyMask &
      (
        1 <<
        i
      )
    ) {

      difficultyDirtyMask &=
        ~(
          1 <<
          i
        );


      renderDifficultyOption(
        i
      );
    }
  }
}


// ============================================================
// FLUSH BATTLE REGIONS
// ============================================================

void flushBattleRegions() {

  if (
    !isBattleState(
      gameState
    )
  ) {

    return;
  }


  if (
    enemyDirty
  ) {

    enemyDirty =
      false;


    renderEnemyRegion();
  }


  if (
    statusDirty
  ) {

    statusDirty =
      false;


    renderStatusRegion();
  }


  if (
    dpadDirty
  ) {

    dpadDirty =
      false;


    renderDPadRegion();
  }
}


// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(
    115200
  );


  delay(
    250
  );


  randomSeed(
    micros()
  );


  // ----------------------------------------------------------
  // I2C
  // ----------------------------------------------------------

  Wire.begin(
    I2C_SDA,
    I2C_SCL
  );


  Wire.setClock(
    100000
  );


  detectPCF();


  if (
    pcfFound
  ) {

    Serial.print(
      "PCF8574 FOUND AT 0x"
    );


    Serial.println(
      pcfAddress,
      HEX
    );


    pcfWrite(
      0xFF
    );


    delay(
      20
    );


    stableRaw =
      readPCFRaw();


    lastSampleRaw =
      stableRaw;


    buttonsHeld =
      rawToButtons(
        stableRaw
      );
  }


  else {

    Serial.println(
      "PCF8574 NOT FOUND"
    );
  }


  sampleChangedAt =
    millis();


  // ----------------------------------------------------------
  // BUZZER
  // ----------------------------------------------------------

  pinMode(
    BUZZER_PIN,
    OUTPUT
  );


  noTone(
    BUZZER_PIN
  );


  // ----------------------------------------------------------
  // TFT
  // ----------------------------------------------------------

  SPI.begin(
    TFT_SCLK,
    TFT_MISO,
    TFT_MOSI,
    TFT_CS
  );


  tft.init(
    240,
    320
  );


  tft.setRotation(
    2
  );


  tft.setTextWrap(
    false
  );


  tft.fillScreen(
    C_BLACK
  );


  region.setTextWrap(
    false
  );


  // ----------------------------------------------------------
  // SAVE
  // ----------------------------------------------------------

  prefs.begin(
    "mxrhythm",
    false
  );


  bestLevel =
    prefs.getInt(
      "best",
      1
    );


  gameState =
    STATE_TITLE;


  stateStart =
    millis();


  screenDirty =
    true;


  Serial.println();

  Serial.println(
    "=================================="
  );

  Serial.println(
    "MX CLUB RHYTHM MEMORY V9"
  );

  Serial.println(
    "NO-FLICKER MENU + BOSS WAVES"
  );

  Serial.println(
    "BOSS LEVELS: 5, 10"
  );

  Serial.println(
    "=================================="
  );
}


// ============================================================
// LOOP
// ============================================================

void loop() {

  // INPUT HAS FIRST PRIORITY

  updateButtons();


  updateMelody();


  updateGame();


  updateVisualDirtyFlags();


  // ----------------------------------------------------------
  // WHOLE SCREEN ONLY ON TRUE SCREEN TRANSITION
  // ----------------------------------------------------------

  if (
    screenDirty
  ) {

    screenDirty =
      false;


    drawWholeScreen();


    updateButtons();


    updateGame();
  }


  // ----------------------------------------------------------
  // TITLE ANIMATION
  // ----------------------------------------------------------

  if (
    gameState ==
      STATE_TITLE &&
    menuAnimDirty
  ) {

    menuAnimDirty =
      false;


    renderMenuAnimation();
  }


  // ----------------------------------------------------------
  // DIFFICULTY
  //
  // Only changed cards redraw.
  // NO fillScreen while selecting.
  // ----------------------------------------------------------

  flushDifficultyCards();


  // ----------------------------------------------------------
  // MENU INPUT INDICATOR
  // ----------------------------------------------------------

  if (
    indicatorDirty
  ) {

    indicatorDirty =
      false;


    if (
      gameState ==
        STATE_TITLE ||
      gameState ==
        STATE_DIFFICULTY
    ) {

      drawTitleInputIndicator();
    }
  }


  // ----------------------------------------------------------
  // BATTLE
  // ----------------------------------------------------------

  flushBattleRegions();


  // Catch button presses immediately after graphics.

  updateButtons();


  updateGame();


  delay(
    1
  );
}
