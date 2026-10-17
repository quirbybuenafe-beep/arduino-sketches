#include <WiFi.h>
#include <WebServer.h>

// ============================================================
// MX CLUB - R.E.N.E.
// Rescue Extraction & Navigation Expert
//
// 2 PLAYER BUTTON MASH GAME
//
// PLAYER 1:
// GPIO25 -> BUTTON -> GND
//
// PLAYER 2:
// GPIO26 -> BUTTON -> GND
//
// WIFI:
// SSID: MX-CLUB-RESCUE
// PASSWORD: mxclub123
//
// OPEN:
// http://192.168.4.1
//
// FIRST TO 60 PRESSES RESCUES R.E.N.E.
// ============================================================


// ============================================================
// WIFI
// ============================================================

const char* AP_SSID = "REDACTED_CONFIG_VALUE";
const char* AP_PASSWORD = "REDACTED_CONFIG_VALUE";

IPAddress localIP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);


// ============================================================
// BUTTONS
// ============================================================

#define P1_BUTTON_PIN 25
#define P2_BUTTON_PIN 26

const unsigned long DEBOUNCE_MS = 16;


// ============================================================
// GAME
// ============================================================

const int TARGET_PRESSES = 60;

enum GameState {
  GAME_WAITING,
  GAME_COUNTDOWN,
  GAME_PLAYING,
  GAME_FINISHED
};

GameState gameState = GAME_WAITING;

int p1Score = 0;
int p2Score = 0;
int winner = 0;

unsigned long countdownStartedAt = 0;


// ============================================================
// BUTTON FILTER
// ============================================================

struct GameButton {
  uint8_t pin;

  bool rawPressed;
  bool stablePressed;

  unsigned long changedAt;
};

GameButton p1Button = {
  P1_BUTTON_PIN,
  false,
  false,
  0
};

GameButton p2Button = {
  P2_BUTTON_PIN,
  false,
  false,
  0
};


// ============================================================
// WEB PAGE
// ============================================================

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>

<head>

<meta charset="UTF-8">

<meta
  name="viewport"
  content="width=device-width,initial-scale=1,user-scalable=no"
>

<title>R.E.N.E.</title>

<style>

* {
  box-sizing: border-box;
}

html,
body {
  margin: 0;
  min-height: 100%;
  background:
    radial-gradient(
      circle at 50% 15%,
      #172b4d 0%,
      #080d19 46%,
      #02040a 100%
    );
  color: white;
  font-family: monospace;
}

body {
  min-height: 100vh;
}

.scanlines {
  position: fixed;
  inset: 0;
  z-index: 100;
  pointer-events: none;
  opacity: .10;

  background:
    repeating-linear-gradient(
      to bottom,
      rgba(255,255,255,.06) 0px,
      rgba(255,255,255,.06) 1px,
      transparent 1px,
      transparent 4px
    );
}

.wrap {
  width: min(980px, 96vw);
  margin: auto;
  padding: 18px 8px 40px;
}


/* ============================================================
   TITLE
   ============================================================ */

.logo {
  text-align: center;
}

.club {
  color: #39eaff;
  font-size: 13px;
  letter-spacing: 5px;
  text-shadow:
    0 0 9px #39eaff;
}

.title {
  margin-top: 7px;
  font-size: clamp(48px, 11vw, 94px);
  line-height: .95;
  font-weight: 900;
  letter-spacing: 3px;

  color: #ffdf43;

  text-shadow:
    4px 5px 0 #6f3500,
    0 0 12px #ff9b00,
    0 0 30px rgba(255,200,0,.35);
}

.meaning {
  margin-top: 8px;

  color: #d6e8ff;

  font-size: clamp(10px, 2.5vw, 15px);

  letter-spacing: 1px;
}

.subtitle {
  margin-top: 10px;

  color: #7f91ad;

  font-size: 11px;
}


/* ============================================================
   MAIN ARENA
   ============================================================ */

.arena {
  position: relative;

  margin-top: 18px;

  padding: 15px;

  border: 3px solid #29e5ff;
  border-radius: 14px;

  background:
    linear-gradient(
      #0d1830,
      #060a14
    );

  box-shadow:
    0 8px 0 #010207,
    0 0 30px rgba(41,229,255,.20),
    inset 0 0 25px rgba(0,0,0,.8);
}

.arena::before {
  content: "";

  position: absolute;

  inset: 7px;

  border:
    1px solid
    rgba(255,255,255,.08);

  border-radius: 9px;

  pointer-events: none;
}

.status {
  text-align: center;

  min-height: 38px;

  color: #ffdf43;

  font-size: clamp(18px, 4vw, 30px);

  font-weight: 900;

  text-shadow:
    0 0 10px rgba(255,223,67,.7);

  margin-bottom: 10px;
}

.battle {
  display: grid;

  grid-template-columns:
    1fr
    230px
    1fr;

  gap: 14px;

  align-items: center;
}


/* ============================================================
   PLAYERS
   ============================================================ */

.player {
  position: relative;

  min-height: 260px;

  overflow: hidden;

  padding: 15px;

  border-radius: 12px;
}

.p1 {
  border: 3px solid #22e6ff;

  background:
    linear-gradient(
      135deg,
      #0b2937,
      #07111c
    );

  box-shadow:
    0 7px 0 #02131a,
    0 0 22px rgba(34,230,255,.18);
}

.p2 {
  border: 3px solid #ff3f8d;

  background:
    linear-gradient(
      225deg,
      #35101f,
      #160811
    );

  box-shadow:
    0 7px 0 #1b030c,
    0 0 22px rgba(255,63,141,.18);
}

.player-label {
  font-size: 16px;
  letter-spacing: 2px;
  font-weight: 900;
}

.p1 .player-label {
  color: #22e6ff;
}

.p2 .player-label {
  color: #ff5da0;
}

.score {
  font-size: clamp(48px, 8vw, 86px);

  font-weight: 900;

  line-height: 1;

  margin: 18px 0;

  text-shadow:
    4px 4px 0 rgba(0,0,0,.55);
}

.press-text {
  font-size: 12px;
  color: #aab6cb;
  margin-bottom: 9px;
}

.meter {
  height: 27px;

  padding: 4px;

  border: 2px solid #dbe8ff;

  background: #04070d;

  box-shadow:
    inset 0 0 8px black;
}

.fill {
  height: 100%;

  width: 0%;

  transition:
    width .055s linear;
}

.p1 .fill {
  background:
    linear-gradient(
      90deg,
      #006f81,
      #22e6ff,
      white
    );

  box-shadow:
    0 0 12px #22e6ff;
}

.p2 .fill {
  background:
    linear-gradient(
      90deg,
      #750037,
      #ff3f8d,
      white
    );

  box-shadow:
    0 0 12px #ff3f8d;
}

.target {
  margin-top: 8px;

  color: #697892;

  font-size: 11px;
}


/* ============================================================
   R.E.N.E. STAGE
   ============================================================ */

.middle {
  text-align: center;
}

.sprite-stage {
  position: relative;

  width: 210px;
  height: 230px;

  max-width: 100%;

  margin: auto;

  overflow: hidden;

  border: 3px solid #596a85;

  border-radius: 11px;

  background:
    linear-gradient(
      #192944 0%,
      #101a30 52%,
      #080e1b 53%,
      #070910 100%
    );

  box-shadow:
    inset 0 0 25px #000,
    0 6px 0 #02040a;
}

.wall {
  position: absolute;

  left: 0;
  right: 0;

  height: 2px;

  background:
    rgba(255,255,255,.05);
}

.floor {
  position: absolute;

  left: 0;
  right: 0;
  bottom: 0;

  height: 48px;

  border-top:
    3px solid #33425d;

  background:
    repeating-linear-gradient(
      90deg,
      #111827 0px,
      #111827 25px,
      #17213a 25px,
      #17213a 27px
    );
}

#reneCanvas {
  position: absolute;

  width: 144px;
  height: 144px;

  left: 50%;
  top: 45%;

  transform:
    translate(-50%,-50%);

  image-rendering: pixelated;
  image-rendering: crisp-edges;
}

.cage {
  position: absolute;

  left: 33px;
  right: 33px;

  top: 18px;

  height: 176px;

  border:
    6px solid #8c9aad;

  border-radius:
    5px 5px 12px 12px;

  box-shadow:
    inset 0 0 10px black,
    0 4px 0 #3b4659;

  transition:
    transform .45s cubic-bezier(.2,.8,.2,1.2),
    opacity .25s ease;
}

.cage.open {
  transform:
    translateY(-210px);

  opacity: 0;
}

.bar {
  position: absolute;

  top: 0;
  bottom: 0;

  width: 6px;

  background:
    linear-gradient(
      90deg,
      #3d4858,
      #d6e1ed,
      #66758b
    );
}

.b1 {
  left: 20%;
}

.b2 {
  left: 40%;
}

.b3 {
  left: 60%;
}

.b4 {
  left: 80%;
}

.help {
  position: absolute;

  left: 0;
  right: 0;

  bottom: 7px;

  color: #ffdf38;

  font-size: 11px;

  text-shadow:
    0 0 6px #ffdf38;
}


/* ============================================================
   BUTTONS
   ============================================================ */

.controls {
  margin-top: 18px;

  display: flex;

  justify-content: center;

  gap: 12px;

  flex-wrap: wrap;
}

button {
  appearance: none;

  border: 0;

  padding:
    14px 26px;

  border-radius: 8px;

  cursor: pointer;

  font-family: monospace;

  font-size: 17px;

  font-weight: 900;

  color: #04100b;

  background:
    linear-gradient(
      #7dff93,
      #17dd64
    );

  box-shadow:
    0 6px 0 #08732f,
    0 0 18px rgba(23,221,100,.25);
}

button:active {
  transform:
    translateY(4px);

  box-shadow:
    0 2px 0 #08732f;
}

.reset {
  color: white;

  background:
    linear-gradient(
      #536582,
      #29364c
    );

  box-shadow:
    0 6px 0 #101827;
}


/* ============================================================
   WIN SCREEN
   ============================================================ */

.winner-screen {
  display: none;

  position: fixed;

  inset: 0;

  z-index: 80;

  align-items: center;
  justify-content: center;

  background:
    radial-gradient(
      circle at 50% 40%,
      rgba(20,50,80,.93),
      rgba(0,0,0,.95)
    );
}

.winner-card {
  width:
    min(600px, 93vw);

  padding:
    20px 16px 28px;

  text-align: center;

  border:
    5px solid #ffdf38;

  border-radius: 18px;

  background:
    linear-gradient(
      #111d35,
      #060912
    );

  box-shadow:
    0 10px 0 #010207,
    0 0 45px rgba(255,223,56,.50);

  animation:
    winPop .28s ease-out;
}

.rescued-text {
  color: #56f2ff;

  font-size: 13px;

  letter-spacing: 4px;
}

.winner-title {
  margin-top: 5px;

  color: #ffdf38;

  font-size:
    clamp(31px, 8vw, 62px);

  font-weight: 900;

  text-shadow:
    4px 4px 0 #703700,
    0 0 15px rgba(255,223,56,.7);
}

.winner-rene-box {
  position: relative;

  width: 220px;
  height: 190px;

  margin:
    8px auto;

  border-radius: 50%;

  background:
    radial-gradient(
      circle,
      rgba(46,230,255,.30),
      rgba(46,230,255,.06) 55%,
      transparent 70%
    );
}

#winnerReneCanvas {
  width: 180px;
  height: 180px;

  image-rendering: pixelated;
  image-rendering: crisp-edges;
}

.mama {
  position: absolute;

  top: 7px;
  right: -4px;

  padding:
    7px 11px;

  border:
    3px solid white;

  border-radius: 12px;

  background: white;

  color: #101522;

  font-weight: 900;

  font-size: 22px;

  transform:
    rotate(5deg);

  animation:
    mamaPop .7s infinite alternate;
}

.winner-sub {
  color: white;

  margin-top: 2px;

  font-size: 16px;
}

.winner-meaning {
  color: #8294b0;

  font-size: 10px;

  margin-top: 9px;

  letter-spacing: 1px;
}

@keyframes winPop {

  from {
    transform:
      scale(.4);
  }

  to {
    transform:
      scale(1);
  }
}

@keyframes mamaPop {

  from {
    transform:
      rotate(4deg)
      scale(1);
  }

  to {
    transform:
      rotate(-3deg)
      scale(1.12);
  }
}

@keyframes hitPulse {

  0% {
    transform:
      scale(1);
  }

  40% {
    transform:
      scale(1.07);
  }

  100% {
    transform:
      scale(1);
  }
}

.hit {
  animation:
    hitPulse .10s ease;
}


/* ============================================================
   PHONE
   ============================================================ */

@media (
  max-width: 760px
) {

  .battle {
    grid-template-columns:
      1fr 1fr;
  }

  .middle {
    grid-column:
      1 / 3;

    grid-row: 1;
  }

  .player {
    min-height:
      205px;
  }

  .sprite-stage {
    width: 190px;
    height: 208px;
  }

  #reneCanvas {
    width: 130px;
    height: 130px;
  }

  .cage {
    left: 29px;
    right: 29px;
    height: 160px;
  }
}

</style>

</head>


<body>

<div class="scanlines"></div>


<div class="wrap">

  <div class="logo">

    <div class="club">
      MX CLUB ARCADE
    </div>

    <div class="title">
      R.E.N.E.
    </div>

    <div class="meaning">
      RESCUE EXTRACTION &amp; NAVIGATION EXPERT
    </div>

    <div class="subtitle">
      TWO PLAYERS • ONE RESCUE • FIRST TO 60
    </div>

  </div>


  <div class="arena">

    <div
      class="status"
      id="status"
    >
      PRESS START
    </div>


    <div class="battle">


      <!-- PLAYER 1 -->

      <div
        class="player p1"
        id="p1Box"
      >

        <div class="player-label">
          PLAYER 1
        </div>

        <div
          class="score"
          id="p1Score"
        >
          0
        </div>

        <div class="press-text">
          GPIO25
        </div>

        <div class="meter">

          <div
            class="fill"
            id="p1Fill"
          ></div>

        </div>

        <div class="target">
          RESCUE POWER
        </div>

      </div>


      <!-- RENE -->

      <div class="middle">

        <div class="sprite-stage">

          <div
            class="wall"
            style="top:38px"
          ></div>

          <div
            class="wall"
            style="top:77px"
          ></div>

          <div
            class="wall"
            style="top:116px"
          ></div>


          <div class="floor"></div>


          <canvas
            id="reneCanvas"
            width="24"
            height="24"
          ></canvas>


          <div
            class="cage"
            id="cage"
          >

            <div class="bar b1"></div>
            <div class="bar b2"></div>
            <div class="bar b3"></div>
            <div class="bar b4"></div>

          </div>


          <div
            class="help"
            id="reneText"
          >
            R.E.N.E. IS TRAPPED!
          </div>

        </div>

      </div>


      <!-- PLAYER 2 -->

      <div
        class="player p2"
        id="p2Box"
      >

        <div class="player-label">
          PLAYER 2
        </div>

        <div
          class="score"
          id="p2Score"
        >
          0
        </div>

        <div class="press-text">
          GPIO26
        </div>

        <div class="meter">

          <div
            class="fill"
            id="p2Fill"
          ></div>

        </div>

        <div class="target">
          RESCUE POWER
        </div>

      </div>

    </div>


    <div class="controls">

      <button
        onclick="startGame()"
      >
        START ROUND
      </button>


      <button
        class="reset"
        onclick="resetGame()"
      >
        RESET
      </button>

    </div>

  </div>

</div>


<!-- =========================================================
     WIN SCREEN
     ========================================================= -->

<div
  class="winner-screen"
  id="winnerScreen"
>

  <div class="winner-card">

    <div class="rescued-text">
      RESCUE COMPLETE
    </div>


    <div
      class="winner-title"
      id="winnerTitle"
    >
      PLAYER 1 WINS!
    </div>


    <div class="winner-rene-box">

      <canvas
        id="winnerReneCanvas"
        width="24"
        height="24"
      ></canvas>

      <div class="mama">
        MAMA!
      </div>

    </div>


    <div class="winner-sub">
      R.E.N.E. IS FREE!
    </div>


    <div class="winner-meaning">
      RESCUE EXTRACTION &amp; NAVIGATION EXPERT
    </div>

  </div>

</div>


<script>

// ============================================================
// R.E.N.E. PIXEL SPRITE
// ============================================================

const reneSprite = [

"000000001111111100000000",
"000000112222222211000000",
"000001222222222222100000",
"000012222222222222210000",
"000122223333332222221000",
"000122233333333222221000",
"001222333333333322222100",
"001223336333633332222100",
"001223333333333332222100",
"001222333111333322222100",
"000122233333333222221000",
"000012223333332222210000",
"000001144444444411000000",
"000011444444444441100000",
"000114444444444444110000",
"001144444444444444110000",
"001144444444444444110000",
"001114444444444441110000",
"000011555555555511000000",
"000011555555555511000000",
"000011555005555511000000",
"000011550005505511000000",
"000115500005500551100000",
"000111100001100011100000"

];


const palette = {

  "1": "#17121f",
  "2": "#573026",
  "3": "#e7a06c",
  "4": "#24d6df",
  "5": "#27385d",
  "6": "#101218",
  "7": "#ffffff"

};


// ============================================================
// DRAW SPRITE
// ============================================================

function drawRene(
  canvasId,
  frame,
  rescued
) {

  const canvas =
    document.getElementById(
      canvasId
    );


  const ctx =
    canvas.getContext(
      "2d"
    );


  ctx.imageSmoothingEnabled =
    false;


  ctx.clearRect(
    0,
    0,
    24,
    24
  );


  let bob =
    frame %
    2;


  for (
    let y = 0;
    y < reneSprite.length;
    y++
  ) {

    const row =
      reneSprite[y];


    for (
      let x = 0;
      x < row.length;
      x++
    ) {

      const pixel =
        row[x];


      if (
        pixel !==
        "0"
      ) {

        ctx.fillStyle =
          palette[
            pixel
          ];


        let drawY =
          y;


        if (
          y <
          19
        ) {

          drawY +=
            bob;
        }


        ctx.fillRect(
          x,
          drawY,
          1,
          1
        );
      }
    }
  }


  // Shirt highlight

  ctx.fillStyle =
    "#8affff";


  ctx.fillRect(
    9,
    14 + bob,
    2,
    1
  );


  if (
    rescued
  ) {

    // Raised arms

    ctx.fillStyle =
      "#e7a06c";


    ctx.fillRect(
      3,
      10,
      2,
      6
    );


    ctx.fillRect(
      19,
      10,
      2,
      6
    );


    ctx.fillRect(
      2,
      8,
      2,
      3
    );


    ctx.fillRect(
      20,
      8,
      2,
      3
    );


    // Sparkles

    ctx.fillStyle =
      "#fff36b";


    ctx.fillRect(
      1,
      4,
      1,
      5
    );


    ctx.fillRect(
      0,
      6,
      3,
      1
    );


    ctx.fillRect(
      22,
      3,
      1,
      5
    );


    ctx.fillRect(
      21,
      5,
      3,
      1
    );


    ctx.fillRect(
      4,
      2,
      1,
      3
    );


    ctx.fillRect(
      3,
      3,
      3,
      1
    );
  }
}


// ============================================================
// ANIMATE R.E.N.E.
// ============================================================

let spriteFrame = 0;


setInterval(
  () => {

    spriteFrame++;


    drawRene(
      "reneCanvas",
      spriteFrame,
      false
    );


    drawRene(
      "winnerReneCanvas",
      spriteFrame,
      true
    );

  },
  260
);


// ============================================================
// AUDIO / "MAMA!"
//
// This uses the browser voice.
// It does NOT copy audio from the linked YouTube Short.
// ============================================================

function playMama() {

  if (
    !(
      "speechSynthesis"
      in window
    )
  ) {

    return;
  }


  window.speechSynthesis.cancel();


  const voice =
    new SpeechSynthesisUtterance(
      "MAMA!"
    );


  voice.rate =
    0.82;


  voice.pitch =
    1.35;


  voice.volume =
    1.0;


  window.speechSynthesis.speak(
    voice
  );
}


// ============================================================
// GAME PAGE STATE
// ============================================================

let previousP1 = -1;
let previousP2 = -1;

let winnerShown = false;

let lastGameState =
  "";


// ============================================================
// PLAYER HIT ANIMATION
// ============================================================

function pulseBox(
  id
) {

  const box =
    document.getElementById(
      id
    );


  box.classList.remove(
    "hit"
  );


  void box.offsetWidth;


  box.classList.add(
    "hit"
  );
}


// ============================================================
// SHOW WIN SCREEN
// ============================================================

function showWinner(
  winner
) {

  if (
    winnerShown
  ) {

    return;
  }


  winnerShown =
    true;


  document.getElementById(
    "winnerTitle"
  ).textContent =
    winner ===
      1 ?
    "PLAYER 1 WINS!" :
    "PLAYER 2 WINS!";


  document.getElementById(
    "winnerScreen"
  ).style.display =
    "flex";


  drawRene(
    "winnerReneCanvas",
    spriteFrame,
    true
  );


  playMama();


  setTimeout(
    () => {

      document.getElementById(
        "winnerScreen"
      ).style.display =
        "none";

    },
    4200
  );
}


// ============================================================
// HIDE WIN
// ============================================================

function hideWinner() {

  winnerShown =
    false;


  document.getElementById(
    "winnerScreen"
  ).style.display =
    "none";


  if (
    "speechSynthesis"
    in window
  ) {

    window.speechSynthesis.cancel();
  }
}


// ============================================================
// FETCH ESP32 GAME STATE
// ============================================================

async function getState() {

  try {

    const response =
      await fetch(
        "/state?t=" +
        Date.now()
      );


    const data =
      await response.json();


    const p1 =
      data.p1;


    const p2 =
      data.p2;


    const target =
      data.target;


    document.getElementById(
      "p1Score"
    ).textContent =
      p1;


    document.getElementById(
      "p2Score"
    ).textContent =
      p2;


    document.getElementById(
      "p1Fill"
    ).style.width =
      Math.min(
        100,
        p1 /
        target *
        100
      ) +
      "%";


    document.getElementById(
      "p2Fill"
    ).style.width =
      Math.min(
        100,
        p2 /
        target *
        100
      ) +
      "%";


    if (
      previousP1 >=
        0 &&
      p1 >
        previousP1
    ) {

      pulseBox(
        "p1Box"
      );
    }


    if (
      previousP2 >=
        0 &&
      p2 >
        previousP2
    ) {

      pulseBox(
        "p2Box"
      );
    }


    previousP1 =
      p1;


    previousP2 =
      p2;


    if (
      data.state ===
      "waiting"
    ) {

      document.getElementById(
        "status"
      ).textContent =
        "PRESS START";


      document.getElementById(
        "reneText"
      ).textContent =
        "R.E.N.E. IS TRAPPED!";


      document.getElementById(
        "cage"
      ).classList.remove(
        "open"
      );


      if (
        lastGameState !==
        "waiting"
      ) {

        hideWinner();
      }
    }


    else if (
      data.state ===
      "countdown"
    ) {

      document.getElementById(
        "status"
      ).textContent =
        data.countdown >
          0 ?
        data.countdown :
        "GO!";


      document.getElementById(
        "reneText"
      ).textContent =
        "GET READY!";


      document.getElementById(
        "cage"
      ).classList.remove(
        "open"
      );
    }


    else if (
      data.state ===
      "playing"
    ) {

      document.getElementById(
        "status"
      ).textContent =
        "MASH! MASH! MASH!";


      document.getElementById(
        "reneText"
      ).textContent =
        "RESCUE R.E.N.E.!";


      document.getElementById(
        "cage"
      ).classList.remove(
        "open"
      );
    }


    else if (
      data.state ===
      "finished"
    ) {

      document.getElementById(
        "status"
      ).textContent =
        data.winner ===
          1 ?
        "PLAYER 1 RESCUED R.E.N.E.!" :
        "PLAYER 2 RESCUED R.E.N.E.!";


      document.getElementById(
        "reneText"
      ).textContent =
        "R.E.N.E. IS FREE!";


      document.getElementById(
        "cage"
      ).classList.add(
        "open"
      );


      if (
        lastGameState !==
        "finished"
      ) {

        showWinner(
          data.winner
        );
      }
    }


    lastGameState =
      data.state;

  }

  catch (
    error
  ) {

    document.getElementById(
      "status"
    ).textContent =
      "ESP32 CONNECTION LOST";
  }
}


// ============================================================
// START GAME
// ============================================================

async function startGame() {

  hideWinner();


  // Calling cancel here helps initialize
  // speech synthesis on some phones.

  if (
    "speechSynthesis"
    in window
  ) {

    window.speechSynthesis.cancel();
  }


  await fetch(
    "/start?t=" +
    Date.now()
  );


  previousP1 =
    0;


  previousP2 =
    0;


  lastGameState =
    "";


  getState();
}


// ============================================================
// RESET GAME
// ============================================================

async function resetGame() {

  hideWinner();


  await fetch(
    "/reset?t=" +
    Date.now()
  );


  previousP1 =
    0;


  previousP2 =
    0;


  lastGameState =
    "";


  getState();
}


// ============================================================
// INITIAL DRAW
// ============================================================

drawRene(
  "reneCanvas",
  0,
  false
);


drawRene(
  "winnerReneCanvas",
  0,
  true
);


getState();


setInterval(
  getState,
  65
);

</script>

</body>
</html>
)rawliteral";


// ============================================================
// STATE NAME
// ============================================================

const char* getStateName() {

  if (
    gameState ==
    GAME_WAITING
  ) {

    return
      "waiting";
  }


  if (
    gameState ==
    GAME_COUNTDOWN
  ) {

    return
      "countdown";
  }


  if (
    gameState ==
    GAME_PLAYING
  ) {

    return
      "playing";
  }


  return
    "finished";
}


// ============================================================
// RESET
// ============================================================

void resetGame() {

  p1Score =
    0;


  p2Score =
    0;


  winner =
    0;


  gameState =
    GAME_WAITING;


  Serial.println(
    "R.E.N.E. GAME RESET"
  );
}


// ============================================================
// START
// ============================================================

void startRound() {

  p1Score =
    0;


  p2Score =
    0;


  winner =
    0;


  countdownStartedAt =
    millis();


  gameState =
    GAME_COUNTDOWN;


  Serial.println();
  Serial.println(
    "=============================="
  );

  Serial.println(
    "R.E.N.E. RESCUE STARTING"
  );

  Serial.println(
    "3... 2... 1..."
  );

  Serial.println(
    "=============================="
  );
}


// ============================================================
// FINISH GAME
// ============================================================

void finishGame(
  int winningPlayer
) {

  if (
    gameState !=
    GAME_PLAYING
  ) {

    return;
  }


  winner =
    winningPlayer;


  gameState =
    GAME_FINISHED;


  Serial.println();
  Serial.println(
    "R.E.N.E. RESCUED!"
  );


  Serial.print(
    "WINNER = PLAYER "
  );


  Serial.println(
    winner
  );
}


// ============================================================
// PLAYER PRESS
// ============================================================

void playerPressed(
  int player
) {

  if (
    gameState !=
    GAME_PLAYING
  ) {

    return;
  }


  if (
    player ==
    1
  ) {

    if (
      p1Score <
      TARGET_PRESSES
    ) {

      p1Score++;
    }


    Serial.print(
      "P1 = "
    );


    Serial.println(
      p1Score
    );


    if (
      p1Score >=
      TARGET_PRESSES
    ) {

      finishGame(
        1
      );
    }
  }


  else {

    if (
      p2Score <
      TARGET_PRESSES
    ) {

      p2Score++;
    }


    Serial.print(
      "P2 = "
    );


    Serial.println(
      p2Score
    );


    if (
      p2Score >=
      TARGET_PRESSES
    ) {

      finishGame(
        2
      );
    }
  }
}


// ============================================================
// BUTTON DEBOUNCE
// ============================================================

void updateButton(
  GameButton &button,
  int player
) {

  unsigned long now =
    millis();


  bool pressedNow =
    digitalRead(
      button.pin
    ) ==
    LOW;


  if (
    pressedNow !=
    button.rawPressed
  ) {

    button.rawPressed =
      pressedNow;


    button.changedAt =
      now;
  }


  if (
    button.rawPressed !=
    button.stablePressed
  ) {

    if (
      now -
      button.changedAt >=
      DEBOUNCE_MS
    ) {

      button.stablePressed =
        button.rawPressed;


      if (
        button.stablePressed
      ) {

        playerPressed(
          player
        );
      }
    }
  }
}


// ============================================================
// BUTTON UPDATE
// ============================================================

void updateButtons() {

  updateButton(
    p1Button,
    1
  );


  updateButton(
    p2Button,
    2
  );
}


// ============================================================
// COUNTDOWN
// ============================================================

void updateGame() {

  if (
    gameState !=
    GAME_COUNTDOWN
  ) {

    return;
  }


  unsigned long elapsed =
    millis() -
    countdownStartedAt;


  if (
    elapsed >=
    3000
  ) {

    gameState =
      GAME_PLAYING;


    Serial.println(
      "GO!"
    );
  }
}


// ============================================================
// COUNTDOWN NUMBER
// ============================================================

int countdownValue() {

  if (
    gameState !=
    GAME_COUNTDOWN
  ) {

    return
      0;
  }


  unsigned long elapsed =
    millis() -
    countdownStartedAt;


  if (
    elapsed <
    1000
  ) {

    return
      3;
  }


  if (
    elapsed <
    2000
  ) {

    return
      2;
  }


  if (
    elapsed <
    3000
  ) {

    return
      1;
  }


  return
    0;
}


// ============================================================
// HOME PAGE
// ============================================================

void handleRoot() {

  server.send_P(
    200,
    "text/html",
    INDEX_HTML
  );
}


// ============================================================
// START ROUTE
// ============================================================

void handleStart() {

  startRound();


  server.send(
    200,
    "text/plain",
    "OK"
  );
}


// ============================================================
// RESET ROUTE
// ============================================================

void handleReset() {

  resetGame();


  server.send(
    200,
    "text/plain",
    "OK"
  );
}


// ============================================================
// STATE ROUTE
// ============================================================

void handleState() {

  char json[220];


  snprintf(
    json,
    sizeof(json),

    "{"
      "\"state\":\"%s\","
      "\"p1\":%d,"
      "\"p2\":%d,"
      "\"target\":%d,"
      "\"winner\":%d,"
      "\"countdown\":%d"
    "}",

    getStateName(),
    p1Score,
    p2Score,
    TARGET_PRESSES,
    winner,
    countdownValue()
  );


  server.sendHeader(
    "Cache-Control",
    "no-store"
  );


  server.send(
    200,
    "application/json",
    json
  );
}


// ============================================================
// NOT FOUND
// ============================================================

void handleNotFound() {

  server.send(
    404,
    "text/plain",
    "R.E.N.E. PAGE NOT FOUND"
  );
}


// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(
    115200
  );


  delay(
    300
  );


  // ----------------------------------------------------------
  // PLAYER BUTTONS
  // ----------------------------------------------------------

  pinMode(
    P1_BUTTON_PIN,
    INPUT_PULLUP
  );


  pinMode(
    P2_BUTTON_PIN,
    INPUT_PULLUP
  );


  p1Button.rawPressed =
    digitalRead(
      P1_BUTTON_PIN
    ) ==
    LOW;


  p1Button.stablePressed =
    p1Button.rawPressed;


  p1Button.changedAt =
    millis();


  p2Button.rawPressed =
    digitalRead(
      P2_BUTTON_PIN
    ) ==
    LOW;


  p2Button.stablePressed =
    p2Button.rawPressed;


  p2Button.changedAt =
    millis();


  // ----------------------------------------------------------
  // WIFI ACCESS POINT
  // ----------------------------------------------------------

  WiFi.mode(
    WIFI_AP
  );


  WiFi.softAPConfig(
    localIP,
    gateway,
    subnet
  );


  bool apStarted =
    WiFi.softAP(
      AP_SSID,
      AP_PASSWORD
    );


  // ----------------------------------------------------------
  // WEB ROUTES
  // ----------------------------------------------------------

  server.on(
    "/",
    HTTP_GET,
    handleRoot
  );


  server.on(
    "/start",
    HTTP_GET,
    handleStart
  );


  server.on(
    "/reset",
    HTTP_GET,
    handleReset
  );


  server.on(
    "/state",
    HTTP_GET,
    handleState
  );


  server.onNotFound(
    handleNotFound
  );


  server.begin();


  // ----------------------------------------------------------
  // SERIAL
  // ----------------------------------------------------------

  Serial.println();
  Serial.println(
    "========================================"
  );

  Serial.println(
    "MX CLUB - R.E.N.E."
  );

  Serial.println(
    "RESCUE EXTRACTION & NAVIGATION EXPERT"
  );

  Serial.println(
    "========================================"
  );


  if (
    apStarted
  ) {

    Serial.println(
      "WIFI AP STARTED"
    );
  }


  else {

    Serial.println(
      "WIFI AP FAILED"
    );
  }


  Serial.print(
    "SSID: "
  );


  Serial.println(
    AP_SSID
  );


  Serial.print(
    "PASSWORD: "
  );


  Serial.println(
    AP_PASSWORD
  );


  Serial.print(
    "OPEN: http://"
  );


  Serial.println(
    WiFi.softAPIP()
  );


  Serial.println();
  Serial.println(
    "PLAYER 1 BUTTON = GPIO25 -> GND"
  );

  Serial.println(
    "PLAYER 2 BUTTON = GPIO26 -> GND"
  );

  Serial.println();
  Serial.println(
    "FIRST TO 60 RESCUES R.E.N.E."
  );

  Serial.println(
    "========================================"
  );
}


// ============================================================
// LOOP
// ============================================================

void loop() {

  // Physical buttons get priority.

  updateButtons();


  // Countdown.

  updateGame();


  // Handle browser.

  server.handleClient();


  // Check buttons again after network work.

  updateButtons();


  delay(
    1
  );
}
