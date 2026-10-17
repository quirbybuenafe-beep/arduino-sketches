#include <WiFi.h>
#include <WebServer.h>

// ============================================================
// MX CLUB - RESCUE RENE: OCEAN RESCUE + MAMA SOUND
//
// P1: GPIO25 -> button -> GND
// P2: GPIO26 -> button -> GND
//
// WiFi: MX-CLUB-RESCUE
// Pass: mxclub123
// Open: http://192.168.4.1
//
// First to 60 presses rescues Rene.
// Uploaded MAMA sound plays:
//   1) when START RESCUE MISSION is pressed
//   2) when Rene is rescued
// ============================================================

const char* WIFI_NAME = "MX-CLUB-RESCUE";
const char* WIFI_PASSWORD = "REDACTED_CONFIG_VALUE";

IPAddress localIP(192,168,4,1);
IPAddress gateway(192,168,4,1);
IPAddress subnet(255,255,255,0);

WebServer server(80);

#define PLAYER1_PIN 25
#define PLAYER2_PIN 26
#define TARGET_DISTANCE 60

const unsigned long PRESS_DEBOUNCE_MS = 7;
const unsigned long RELEASE_DEBOUNCE_MS = 9;

enum GameState {
  GAME_IDLE,
  GAME_COUNTDOWN,
  GAME_RACING,
  GAME_FINISHED
};

GameState gameState = GAME_IDLE;

int player1Score = 0;
int player2Score = 0;
int winner = 0;

unsigned long countdownStarted = 0;

struct RaceButton {
  uint8_t pin;
  bool rawPressed;
  bool stablePressed;
  unsigned long changedAt;
};

RaceButton p1Button = {PLAYER1_PIN,false,false,0};
RaceButton p2Button = {PLAYER2_PIN,false,false,0};

void resetButton(RaceButton &b) {
  bool p = digitalRead(b.pin) == LOW;
  b.rawPressed = p;
  b.stablePressed = p;
  b.changedAt = millis();
}

void resetGame() {
  player1Score = 0;
  player2Score = 0;
  winner = 0;
  gameState = GAME_IDLE;
  resetButton(p1Button);
  resetButton(p2Button);
}

void startRace() {
  player1Score = 0;
  player2Score = 0;
  winner = 0;
  countdownStarted = millis();
  gameState = GAME_COUNTDOWN;
  resetButton(p1Button);
  resetButton(p2Button);
  Serial.println("3... 2... 1...");
}

void finishRace(int player) {
  if (gameState != GAME_RACING) return;
  winner = player;
  gameState = GAME_FINISHED;
  Serial.print("PLAYER ");
  Serial.print(player);
  Serial.println(" RESCUED RENE!");
}

void playerPressed(int player) {
  if (gameState != GAME_RACING) return;

  if (player == 1) {
    if (player1Score < TARGET_DISTANCE) player1Score++;
    if (player1Score >= TARGET_DISTANCE) finishRace(1);
  } else {
    if (player2Score < TARGET_DISTANCE) player2Score++;
    if (player2Score >= TARGET_DISTANCE) finishRace(2);
  }
}

void updateOneButton(RaceButton &b, int player) {
  unsigned long now = millis();
  bool p = digitalRead(b.pin) == LOW;

  if (p != b.rawPressed) {
    b.rawPressed = p;
    b.changedAt = now;
  }

  if (b.rawPressed != b.stablePressed) {
    unsigned long waitMs = b.rawPressed ? PRESS_DEBOUNCE_MS : RELEASE_DEBOUNCE_MS;

    if (now - b.changedAt >= waitMs) {
      b.stablePressed = b.rawPressed;
      if (b.stablePressed) playerPressed(player);
    }
  }
}

void updateButtons() {
  updateOneButton(p1Button,1);
  updateOneButton(p2Button,2);
}

void updateGame() {
  if (gameState == GAME_COUNTDOWN && millis() - countdownStarted >= 3000) {
    gameState = GAME_RACING;
    resetButton(p1Button);
    resetButton(p2Button);
    Serial.println("RESCUE!");
  }
}

int countdownValue() {
  if (gameState != GAME_COUNTDOWN) return 0;
  unsigned long e = millis() - countdownStarted;
  if (e < 1000) return 3;
  if (e < 2000) return 2;
  if (e < 3000) return 1;
  return 0;
}

unsigned long lastWiFiHealthCheck = 0;

void maintainWiFiAP() {
  unsigned long now = millis();

  if (now - lastWiFiHealthCheck < 5000) {
    return;
  }

  lastWiFiHealthCheck = now;

  wifi_mode_t mode = WiFi.getMode();

  if (
    mode != WIFI_AP &&
    mode != WIFI_AP_STA
  ) {
    Serial.println("WIFI AP MODE LOST - RECOVERING");

    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    WiFi.softAPConfig(localIP,gateway,subnet);
    WiFi.softAP(WIFI_NAME,WIFI_PASSWORD);
  }
}

const char PAGE[] PROGMEM =
R"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Rescue Rene Ocean Rescue</title>
<style>
*{box-sizing:border-box}
html,body{margin:0;min-height:100%;font-family:monospace;background:#010914;color:#effcff}
body{padding:8px}
.game{width:min(1500px,98vw);margin:auto;border:4px solid #effcff;background:#031523;box-shadow:0 0 45px #005274,0 18px 50px #000}
header{padding:18px 20px;text-align:center;border-bottom:4px solid #effcff;background:linear-gradient(180deg,#082845,#061626);position:relative;overflow:hidden}
header b{display:block;color:#9eeeff;letter-spacing:4px;font-size:12px}
h1{margin:4px 0;font-size:clamp(38px,8vw,76px);text-shadow:5px 5px #075375}
header small{opacity:.82;letter-spacing:2px}
.status{display:grid;grid-template-columns:1fr auto 1fr;gap:16px;padding:12px 20px;align-items:center;border-bottom:4px solid #effcff;background:#061522}
#state{font-size:clamp(26px,4vw,50px);font-weight:900;color:#7ee9ff;border:3px solid #7ee9ff;padding:8px 22px;text-align:center;background:#020b13;box-shadow:inset 0 0 18px #00344a,0 0 14px #0bc7ff44}
.r{text-align:right}
.p1{color:#20dcff} .p2{color:#ffb238}
.ocean{position:relative;height:740px;overflow:hidden;background:linear-gradient(#39c5e8 0%,#1594c2 18%,#0a76a4 40%,#05567b 65%,#023852 83%,#01283d 100%)}
.ocean:before{content:"";position:absolute;inset:0;background:radial-gradient(circle,#fff8 0 2px,transparent 3px);background-size:100px 120px;animation:bubbles 5s linear infinite}
@keyframes bubbles{to{background-position:0 -120px}}
.surface{height:44px;background:repeating-linear-gradient(90deg,#b9f7ff 0 36px,#54d8f2 36px 72px,#168db8 72px 108px);animation:wave 1.25s linear infinite;box-shadow:0 7px 20px #8cefff55}
@keyframes wave{to{transform:translateX(90px)}}
.midline{position:absolute;left:50%;top:50px;bottom:54px;width:7px;background:repeating-linear-gradient(#ffe45d 0 18px,#173b52 18px 36px);box-shadow:0 0 18px #ffe45d77;z-index:3}
.rene{position:absolute;left:50%;top:285px;transform:translateX(-50%);text-align:center;z-index:8;padding:16px;border-radius:50%;background:radial-gradient(circle,#ffe45d33 0 30%,transparent 68%)}
canvas{image-rendering:pixelated;image-rendering:crisp-edges}
#rene{width:190px;height:190px;filter:drop-shadow(0 8px 0 #00131f99)}
.tag{padding:8px 14px;background:#03111bee;border:3px solid #ffe45d;color:#ffe45d;font-weight:900;font-size:14px;box-shadow:0 0 18px #ffe45d55}
.lane{position:absolute;left:2.5%;right:2.5%;height:220px;border:3px solid #ffffff22;border-left:0;border-right:0;background:linear-gradient(90deg,#ffffff05,#ffffff02 35%,#ffffff00 50%,#ffffff02 65%,#ffffff05);box-shadow:inset 0 0 25px #00182455}
#lane1{top:92px} #lane2{bottom:92px}
.diver{position:absolute;top:38px;width:245px;transition:left .055s linear,right .055s linear;z-index:7}
#d1{left:0} #d2{right:0}
.diver canvas{width:215px;height:142px;filter:drop-shadow(6px 8px 0 #00131f88)}
.dist{text-align:center;padding:7px 10px;background:#03111bee;border:3px solid currentColor;font-weight:900;font-size:13px;box-shadow:0 4px 0 #00111a}
#d1 .dist{color:#20dcff} #d2 .dist{color:#ffb238}
.info{display:grid;grid-template-columns:1fr 1fr;border-top:4px solid #effcff;background:#041522}
.card{padding:18px 22px;background:linear-gradient(180deg,#071d30,#04131f)} .card:first-child{border-right:4px solid #effcff}
.meter{height:31px;border:3px solid #effcff;background:#02070c;margin-top:11px;padding:3px;box-shadow:inset 0 0 10px #000}
.fill{height:100%;width:0;transition:width .055s linear;box-shadow:0 0 15px currentColor}
#b1{background:linear-gradient(90deg,#087f9c,#20dcff,#b8f8ff);color:#20dcff} #b2{background:linear-gradient(90deg,#9a5a00,#ffb238,#ffe0a0);color:#ffb238}
.controls{display:flex;gap:16px;justify-content:center;flex-wrap:wrap;padding:20px;border-top:4px solid #effcff;background:#03111d}
button{padding:16px 28px;min-width:260px;font:900 17px monospace;border:3px solid #effcff;background:linear-gradient(#0d4268,#08283f);color:#fff;box-shadow:6px 6px #01070b;letter-spacing:.5px}
button:active{transform:translate(3px,3px);box-shadow:2px 2px #01070b}
#win{display:none;position:absolute;left:12%;right:12%;top:125px;bottom:125px;z-index:30;background:radial-gradient(circle at 50% 45%,#0b3045ee,#020b14fa 68%);border:8px solid #59f29a;text-align:center;padding:30px;box-shadow:0 0 55px #59f29a66}
#win h2{font-size:clamp(34px,7vw,70px);margin:8px 0;color:#59f29a}
#winRene{width:220px;height:220px;filter:drop-shadow(0 8px 0 #00131f99)}

.trackLabel{position:absolute;z-index:6;top:58px;padding:7px 10px;background:#02101aee;border:2px solid #d7f8ff;color:#d7f8ff;font-weight:900;font-size:11px;letter-spacing:1px}
.trackStartL{left:3%}.trackStartR{right:3%}.trackMid{left:50%;transform:translateX(-50%);color:#ffe45d;border-color:#ffe45d}
.checkpoint{position:absolute;top:52px;bottom:55px;width:2px;background:repeating-linear-gradient(to bottom,#ffffff44 0 9px,transparent 9px 18px);z-index:2}
.cp1{left:25%}.cp2{left:75%}
.depth{position:absolute;z-index:2;left:12px;color:#bfeeffaa;font-size:10px;letter-spacing:1px}.depth1{top:165px}.depth2{top:360px}.depth3{top:555px}
.reef{position:absolute;bottom:0;width:240px;height:135px;z-index:4;opacity:.95}
.reefL{left:-10px}.reefR{right:-10px;transform:scaleX(-1)}
.rock{position:absolute;bottom:0;background:#23485a;border:4px solid #173443;border-radius:45% 55% 12% 10%;box-shadow:inset 10px 8px 0 #315d6f}
.rock.a{left:5px;width:95px;height:70px}.rock.b{left:78px;width:115px;height:92px}.rock.c{left:155px;width:72px;height:58px}
.weed{position:absolute;bottom:42px;width:14px;border-radius:50% 50% 0 0;background:#27b976;transform-origin:bottom center;animation:sway 1.8s ease-in-out infinite alternate}
.weed.w1{left:34px;height:82px}.weed.w2{left:68px;height:105px;animation-delay:.3s}.weed.w3{left:126px;height:72px;animation-delay:.6s}.weed.w4{left:176px;height:98px;animation-delay:.9s}
@keyframes sway{from{transform:rotate(-6deg)}to{transform:rotate(8deg)}}
.school{position:absolute;z-index:2;color:#9cecff88;font-size:20px;letter-spacing:10px;white-space:nowrap;animation:school 18s linear infinite}.school.s1{top:195px;left:-260px}.school.s2{top:515px;left:-360px;animation-duration:24s;animation-delay:-8s}
@keyframes school{to{transform:translateX(1900px)}}
.ray2{position:absolute;top:38px;width:180px;height:620px;background:linear-gradient(180deg,#ffffff20,transparent 75%);clip-path:polygon(20% 0,80% 0,100% 100%,0 100%);opacity:.28;z-index:1;pointer-events:none}.rayL{left:10%}.rayC{left:42%;width:150px;opacity:.18}.rayR{right:10%}
@media(max-width:700px){
  .status{grid-template-columns:1fr;text-align:center}
  .r{text-align:center}
  .ocean{height:620px}
  .diver{width:155px;top:42px}
  .diver canvas{width:140px;height:92px}
  .info{grid-template-columns:1fr}
  .card:first-child{border-right:0;border-bottom:4px solid #effcff}
}
</style>
</head>
<body>

<div class="game">
<header>
  <b>MX CLUB ARCADE</b>
  <h1>RESCUE RENE</h1>
  <small>OCEAN RESCUE // DEEP-SEA SPRINT // FIRST DIVER TO 60M WINS</small>
</header>

<div class="status">
  <strong class="p1">PLAYER 1 // GPIO25</strong>
  <div id="state">READY</div>
  <strong class="p2 r">PLAYER 2 // GPIO26</strong>
</div>

<div class="ocean">
  <div class="surface"></div>

  <div class="ray2 rayL"></div>
  <div class="ray2 rayC"></div>
  <div class="ray2 rayR"></div>

  <div class="checkpoint cp1"></div>
  <div class="checkpoint cp2"></div>

  <div class="trackLabel trackStartL">P1 START</div>
  <div class="trackLabel trackMid">RENE RESCUE ZONE</div>
  <div class="trackLabel trackStartR">P2 START</div>

  <div class="depth depth1">DEPTH 10M</div>
  <div class="depth depth2">DEPTH 20M</div>
  <div class="depth depth3">DEPTH 30M</div>

  <div class="school s1">>< >  ><   >  ><</div>
  <div class="school s2">><   ><  >   ><</div>

  <div class="reef reefL">
    <div class="rock a"></div><div class="rock b"></div><div class="rock c"></div>
    <div class="weed w1"></div><div class="weed w2"></div><div class="weed w3"></div><div class="weed w4"></div>
  </div>

  <div class="reef reefR">
    <div class="rock a"></div><div class="rock b"></div><div class="rock c"></div>
    <div class="weed w1"></div><div class="weed w2"></div><div class="weed w3"></div><div class="weed w4"></div>
  </div>

  <div class="midline"></div>

  <div class="rene">
    <canvas id="rene" width="40" height="40"></canvas>
    <div class="tag" id="reneTag">RENE // NEEDS HELP</div>
  </div>

  <div class="lane" id="lane1">
    <div class="diver" id="d1">
      <canvas id="c1" width="64" height="42"></canvas>
      <div class="dist" id="dist1">60M TO RENE</div>
    </div>
  </div>

  <div class="lane" id="lane2">
    <div class="diver" id="d2">
      <canvas id="c2" width="64" height="42"></canvas>
      <div class="dist" id="dist2">60M TO RENE</div>
    </div>
  </div>

  <div id="win">
    <div>MISSION SUCCESS</div>
    <h2 id="winText">PLAYER 1 RESCUED RENE!</h2>
    <canvas id="winRene" width="40" height="40"></canvas>
    <div>RENE IS SAFE!</div>
  </div>
</div>

<div class="info">
  <div class="card">
    <b class="p1">PLAYER 1</b>
    <span id="m1" style="float:right">60M AWAY</span>
    <div class="meter"><div class="fill" id="b1"></div></div>
  </div>
  <div class="card">
    <b class="p2">PLAYER 2</b>
    <span id="m2" style="float:right">60M AWAY</span>
    <div class="meter"><div class="fill" id="b2"></div></div>
  </div>
</div>

<div class="controls">
  <button id="start">START RESCUE MISSION</button>
  <button id="reset">RESET MISSION</button>
</div>
</div>

<audio id="mama" preload="auto" src="data:audio/mpeg;base64,
)HTML"
"SUQzBAAAAAABBVRYWFgAAAASAAADbWFqb3JfYnJhbmQAaXNvbQBUWFhYAAAAEwAAA21pbm9yX3ZlcnNpb24ANTEyAFRYWFgAAAAgAAADY29tcGF0"
"aWJsZV9icmFuZHMAaXNvbWlzbzJtcDQxAFRTU0UAAAAOAAADTGF2ZjYxLjcuMTAzAAAAAAAAAAAAAAD/4zjAAAAAAAAAAAAASW5mbwAAAA8AAAAs"
"AAANOAAVFRsbICAmJiYrKzExNjY7OztBQUZGTExRUVFXV1xcYmJiZ2dsbHJyd3d3fX2CgoiIjY2Nk5OYmJ2dnaOjqKiurrOzs7m5vr7ExMnJyc7O"
"1NTZ2dnf3+Tk6urv7+/19fr6//8AAAAATGF2YzYxLjE5AAAAAAAAAAAAAAAAJAJrAAAAAAAADTg8VT6hAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD/4xjEAAxwdewAwkYgQDF8ShQkRDdTRY6wjK3piziAHGFZZajiHnxSHBNlBrNGXUmDWaMu/QZdo/zHQ7//"
"/m048s2IY7VYQ4T/4xjECQ0IilQBWhgB8p0yBoSCOygHas0tTeXPo7mMrl9fPuql6n79vVSk3b7wMED9eRH8Zdnh7aoBFDCpSSAWIKEijA3VBFT/"
"4xjEDxDSOpgBkSgAiBi7Iz61yMIgUjT/TLhcb/87kF0b//yN5Cf//7ISTIr////yEFznA4ucUb//0////UGHgOoajDQqK2b/4xjEBg6qIswBhxAA"
"PDb3LnjCuPi2aEJm/X0f+rpbb/////2+9rpbs97WY5qekpkM52VrMYMDQKf/3feev///o11WPZWh2EH/4xjEBg6RGup5xRAAQyGHIiGSjGo4qKOQ"
"CivR6PtUUQqoZ3omiPZWRlUMQ/fjxUjI1lE2hf/8VMsSVIIX///kOpUJqpIHqDr/4xjEBg5QlsTwQ9AgHzQn3faspmCwPAjFLOnqP6yx4oZRgGp5"
"327Qw4n6z5uth7Sj1qIwaEdNj/7v/0GBOD9TVVWW/9S3JgX/4xjEBw4JguAAaAzkeCzJAfy8aF9/1tcuHl3W5gdJcvjCD0JRb//78R7vDAQAKJgy"
"bZ/T////+x6ROooAyCr/tR2Zah0L/rn/4xjECQ3ZlvgAAZQkuqD4VQugkjUEM0ehfj///+lzG//5QoN3LBeCsFgG8LwWAH/oNf////8eXMkLvdXD"
"SgQV0ABGPX/606T/4xjEDAuRr0JYOJjq1Dm///HjzAVAIKHmf/+mUQHA0KBi//1L26qhGHInFURz99XDbAKSC1tkY6n/VZys08iaAWZ/nIPzLf//"
"4xjEGAzqFzZYOA6G/nHPFQrCbt/pVP+r///805SQ6//2C7//qP//WmCGaM483Y5juV7Ic1m1NAYa3qn+v33PZv///HxWQNL/4xjEHwxxrz74GA4C"
"af70/ZbREf/8Uf/9R7/5cDf/qqLaA26nG7Iv/6rXyCAH/5g4IqN///x8SyILheaRf/U4470YwcUam///4xjEKAy5rzJYKA6GlzQNf/lf/pJ/+CF4"
"iFgIKuCQ5RQTWc7ppeRne0IBw/15//oQhyf+hbO7M//TJqJFBShDt/yYBt///6//4xjEMAyxp0L4KATi/9xlaHInAIjGMOMRGNm/OPT4mNZ/6p/8"
"eEQAkWH3+XVWM///MgYwYI4oXPGf5ZH///TQPd/g0tWNolr/4xjEOA0RmwbQOATggUJjfBCGBFUPrCOTf1OFF8zbfoxPqxz6///q4MDIIFguIHH6"
"R1zijnv/4gM//q/9YeUJyGFt//+2pxP/4xjEPg0hRxcYGEQgh2Y3//2Uw0VBXI3b//NY1XMOChCQMyYyf///ISIIHP/p/9Ywb/4NBUQABfxZwICY"
"gEUpUdf//eNAQjf/4xjERAziDvyoAJRga/+IxUsBQ1u///zVMKN///ygjA+HxHEt/+uFQQZ///8cFRECCoCgWhSQHEHSve/+7IAI+JksRf//GAb/"
"4xjESw0Zuw5QKA6gCAJ///9SjRV////U5gEA/v/S8OJ//Z/9yTP/6fNFCNQDK85H01Rl0mv2MtajmtgqOoSnkpxuunzDloP/4xjEUQv5ww6YSAoA"
"IiH4m////RQAEv///4mAz/VXR/9NUoHD////oRf///ziZh//wgqJhVqSUBNzkjJLFRMi/EL4GYMg2dT/4xjEXBGbCyJYYooegKM7JUxmef5rMccL"
"hDhSs2crf/+hUBMKVv/1RnorGmE4UABAeM9969/XNU8wqDaRI///0uWLwLxNJmT/4xjEUBbzAv8YMFQI//6eaooJhZ9P3A22abM65hAeTBDrOQUa"
"hamyDB1+tg8CNZMPT5D+ejeiaVxqA5rdkP//1JKH0fzF////4xjELxcDZvb4MpoUXXUdOmZmCgBMnlfZVf/ZdSg6kBfSbV/9rLUOMc6LrX///nRw"
"GqTN//rf7k83aIVX6jSjkFsGWwmRl87/4xjEDhCTZyr4Eo5+Cf61DwNtjar+h3/kAUzG///2zyo6///9KzQeDVf0p//RQjElDP///3///+qiMHO/"
"///SVepwhlgBZAz/4xjEBg569wr4OBSgRUeZE/9Vf64WD2e6//jIU///MocYVNIhEnv///6IBoeH////Qc////lCy////jMUo/+sJaAIB/bcjDD/"
"4xjEBw4rYyo4CBQLGsjHf///5GNv//+rKoTid///6mlSVQXRYMb///0EOSf///oXb///zUEkW2a////F7SqAAgCADEOHadv/4xjECQ6TYv5QEBQJ"
"EtP//96TwZKUb//+8jC/Ob//3+SDMGApS////oRiAb///885v///ZR+Nf///sRk5pWqKpzqSg0CiQM//4xjECQ87XxMYME4EGbg/+B/XCraurN/r"
"7WqIAwM71f//0EIEC7f//+ghIIv///xOR////mGr///8qLG////Km6CwewGnAJr/4xjEBw5y8xcYGA4AjAbwFOv/UBhZ2PMr///FIZAOZP7+3nHq"
"UEIc////4VHZv///qOHf///7////qLHf/ApNgJU544IQAp7/4xjECA2yHwbwMBQAzKfq6fVUDwvqj/+f/oJAal2df//8qC2Nm///7HCwCCWJH///"
"+Tv/+VCn/4I//nGABwCMFgbIlsF+5tP/4xjEDA+LVv5QCBQJ//9kUYCONx////VmQRIXx6f//9GAXESLB57///rMC0XNb///WKhEv///yAelv///"
"yAgqw8DAd32///v/4xjECA6jWv2QOBqEIgMHf25kNx02///9Q0Emg3//9S1jHCdLMU0v//84MQ2f///6Bqh///+bF1Fv//+owMUKoAYAoYVKbUf/"
"4xjECA5bVwZYCA4IwTLv///9w0VLG///9Qu4ikjv//+BoeJKHf//6QfnH///+YERU42///+oijNP///JCPXAQdBizLoIV///4xjECQ47VwGQCBQJ"
"//rMJBaJ1///5gzHgnnf//9jwRi8sTob///qIVHb///oPC////zyMZHv///7Dw8s4GoE863Fej///2r/4xjECw5rUx5YOBSFBP/TMFokHh3///hm"
"RA2uSG///54Wjjj1b//+eFCOuz///7uSM2///9Tv///5DG2ABACBjjImWom/////4xjEDA2bUwZQCA4IaohHDzv///EUSCw46///5otEpzD1///o"
"PE0b///x8i////1HRfX///ziaoCAhQOb5gSmr///8Ujhp6f/4xjEEAxq6wWICA4K///GgZUef///NBeAcMlXN///xwWHP///9Tv///0LHf/g7YiP"
"84////5mOrnm///rWcGsun1///5wuBb/4xjEGQyzTvzACBoIBscTRb//+mfUn///9X///6ZfLFv///0EDZ2ruosGhn/GyEEPWnpvb6TAofzD//Q1"
"q///88bS6P//+qH/4xjEIQ0CC3MYOA6iAA8MDLjVU//+PkyCf///w9/+H5AKQAGQQKlCGh3sxQwRRX///s8kjkWzo+/+hSt7f/6EMwYMb/g+HA3/"
"4xjEKAwhox8YCAQEf+oTf/zH/yyVwGIDEOgv1nq4vJ///nEnVjIRv/+4uHS94i3/yKYUAxy6ncWCKOkuDLjH////r6jIwO//4xjEMgzRqwogMAoE"
"+hWQBXD3SSgUXUMzPu9TCiAX//oR2ZRgmE2+pRkNMyhDwiKho8zdiByAaBqY6v/////9ancEWI40pGD/4xjEOQxYy1b4YAQGNuRHD0O//1dmcuWz"
"G/+h83ahrk/8xW2o/KQKna/6i7j4i1ZtmoSG//q//V+mb9tzLtQVfo8EDXuHkd7/4xjEQgzxRzb5RxACMSYEi6/D2HVzAj0IfKLY4z82+BF+D7oY"
"OvvHJJTXOWLIADBv3Iwh6UULAs+BVYmGGhdqtOOPwg/iXb//4xjESRhCfvABiEAA7jlLpn////+JG/wKz8NHgOof6Cals/GJWGiZrqewAgqCp0qG"
"hEDSwaeJTp4Gh4Ku5bwaGA1Ep1eW5UP/4xjEIwzwAu75wBgAZUNRFxEe1gq8FQ1koNA1yroIBhgaORq1pGXZfVlloiKFCyCakiJwksEwyAgkz///"
"WKN//6xT///+sU//4xjEKgtglbAwGYxg/oFRakxBTUUzLjEwMKqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo="
R"HTML(
"></audio>

<script>
const d1=document.getElementById('d1'),d2=document.getElementById('d2');
const state=document.getElementById('state'),reneTag=document.getElementById('reneTag');
const dist1=document.getElementById('dist1'),dist2=document.getElementById('dist2');
const m1=document.getElementById('m1'),m2=document.getElementById('m2');
const b1=document.getElementById('b1'),b2=document.getElementById('b2');
const win=document.getElementById('win'),winText=document.getElementById('winText');
let lastState=null,frame=0;

const playReneSound=()=>{
  const a=document.getElementById('mama');
  try{a.pause();a.currentTime=0;const p=a.play();if(p)p.catch(()=>{})}catch(e){}
}

const C=(id)=>{const c=document.getElementById(id),x=c.getContext('2d');x.imageSmoothingEnabled=false;return [c,x]}
const px=(x,a,b,c,d,k)=>{x.fillStyle=k;x.fillRect(a,b,c,d)}

const diver=(id,p,f)=>{
  const [c,x]=C(id);x.clearRect(0,0,c.width,c.height);
  const flip=p===2,main=p===1?'#20dcff':'#ffb238',dark=p===1?'#075e75':'#945400';
  x.save();if(flip){x.translate(c.width,0);x.scale(-1,1)}
  px(x,2,7+f%3,2,2,'#dffcff');px(x,8,3+(f+1)%3,1,1,'#dffcff');
  px(x,4,28+f%2,10,4,dark);px(x,0,30+f%2,7,3,main);
  px(x,14,11,8,17,'#b6c5d8');px(x,22,13,19,16,main);
  px(x,39,6,12,12,'#e9a16d');px(x,42,4,7,3,'#4f2c23');
  px(x,43,8,11,5,'#172b3c');px(x,45,9,7,3,'#9bf6ff');
  px(x,35,25,15,4,'#e9a16d');px(x,25,14,10,2,'#fff');
  x.restore();
}

const rene=(id,safe,f)=>{
  const [c,x]=C(id);x.clearRect(0,0,c.width,c.height);const bob=safe?0:f%2;
  px(x,13,5+bob,14,4,'#4f2c23');px(x,11,8+bob,18,5,'#4f2c23');
  px(x,13,11+bob,14,10,'#e9a16d');px(x,16,14+bob,2,2,'#101820');px(x,23,14+bob,2,2,'#101820');
  px(x,11,21+bob,18,10,safe?'#59f29a':'#ffe45d');px(x,15,22+bob,10,2,'#fff');
  if(safe){
    px(x,5,9,4,10,'#e9a16d');px(x,31,9,4,10,'#e9a16d');
    px(x,2,6,5,1,'#fff36b');px(x,4,4,1,5,'#fff36b');
    px(x,33,6,5,1,'#fff36b');px(x,35,4,1,5,'#fff36b');
  }else{
    px(x,7,22+bob,5,4,'#e9a16d');px(x,28,22+bob,5,4,'#e9a16d');
  }
  px(x,13,31+bob,6,6,'#25355d');px(x,22,31+bob,6,6,'#25355d');
}

const move=(el,score,p,target)=>{
  const travel=Math.min(100,score/target*100)*.405;
  if(p===1)el.style.left=travel+'%';else el.style.right=travel+'%';
}

const render=(data)=>{
  const p1=Number(data.p1)||0,p2=Number(data.p2)||0,t=Number(data.target)||60;
  const r1=Math.max(0,t-p1),r2=Math.max(0,t-p2);
  const justWon=lastState!==null && data.state==='FINISHED' && lastState!=='FINISHED';

  move(d1,p1,1,t);move(d2,p2,2,t);
  dist1.textContent=r1+'M TO RENE';dist2.textContent=r2+'M TO RENE';
  m1.textContent=r1+'M AWAY';m2.textContent=r2+'M AWAY';
  b1.style.width=Math.min(100,p1/t*100)+'%';b2.style.width=Math.min(100,p2/t*100)+'%';

  win.style.display='none';

  if(data.state==='IDLE'){
    state.textContent='READY';reneTag.textContent='RENE // NEEDS HELP';
  }else if(data.state==='COUNTDOWN'){
    state.textContent=data.countdown>0?data.countdown:'GO!';reneTag.textContent='RENE // HOLD ON!';
  }else if(data.state==='RACING'){
    state.textContent='RESCUE!';reneTag.textContent='RENE // NEEDS HELP';
  }else{
    state.textContent='SAFE!';reneTag.textContent='RENE // SAFE';
    win.style.display='block';
    winText.textContent='PLAYER '+data.winner+' RESCUED RENE!';
    if(justWon)playReneSound();
  }

  lastState=data.state;
}

let pollBusy=false;
let pollTimer=null;
let connectionFailures=0;

const schedulePoll=(delayMs)=>{
  if(pollTimer!==null)clearTimeout(pollTimer);
  pollTimer=setTimeout(poll,delayMs);
};

const requestWithTimeout=async(url,options={})=>{
  const controller=new AbortController();
  const timer=setTimeout(()=>controller.abort(),1800);

  try{
    return await fetch(
      url,
      {
        ...options,
        cache:'no-store',
        signal:controller.signal
      }
    );
  }finally{
    clearTimeout(timer);
  }
};

const poll=async()=>{
  if(pollBusy){
    schedulePoll(120);
    return;
  }

  pollBusy=true;

  try{
    const r=await requestWithTimeout('/state?t='+Date.now());

    if(!r.ok)throw new Error('HTTP '+r.status);

    const data=await r.json();

    render(data);

    connectionFailures=0;

    const nextDelay=
      data.state==='RACING'
      ? 90
      : data.state==='COUNTDOWN'
        ? 120
        : 180;

    schedulePoll(nextDelay);
  }catch(e){
    connectionFailures++;

    state.textContent=
      connectionFailures<3
      ? 'RECONNECTING...'
      : 'CONNECTION LOST';

    schedulePoll(
      Math.min(
        1200,
        250+(connectionFailures*150)
      )
    );
  }finally{
    pollBusy=false;
  }
};

document.getElementById('start').onclick=async()=>{
  playReneSound();

  try{
    const r=await requestWithTimeout(
      '/start?t='+Date.now(),
      {method:'POST'}
    );

    if(!r.ok)throw new Error('START FAILED');

    lastState=null;
    connectionFailures=0;

    schedulePoll(30);
  }catch(e){
    state.textContent='RECONNECTING...';
    schedulePoll(300);
  }
};

document.getElementById('reset').onclick=async()=>{
  try{
    const r=await requestWithTimeout(
      '/reset?t='+Date.now(),
      {method:'POST'}
    );

    if(!r.ok)throw new Error('RESET FAILED');

    lastState=null;
    connectionFailures=0;

    schedulePoll(30);
  }catch(e){
    state.textContent='RECONNECTING...';
    schedulePoll(300);
  }
};

setInterval(()=>{
  frame++;
  diver('c1',1,frame);diver('c2',2,frame);
  const safe=state.textContent==='SAFE!';
  rene('rene',safe,frame);rene('winRene',true,frame);
},220);

diver('c1',1,0);diver('c2',2,0);rene('rene',false,0);rene('winRene',true,0);
schedulePoll(40);
</script>
</body>
</html>
)HTML";

void handleRoot(){
  server.sendHeader("Cache-Control","no-store, no-cache, must-revalidate");
  server.sendHeader("Pragma","no-cache");
  server.send_P(200,"text/html",PAGE);
}

void handleStart(){
  startRace();
  server.send(200,"text/plain","OK");
}

void handleReset(){
  resetGame();
  server.send(200,"text/plain","OK");
}

void handleState(){
  const char* s="FINISHED";

  if(gameState==GAME_IDLE)s="IDLE";
  else if(gameState==GAME_COUNTDOWN)s="COUNTDOWN";
  else if(gameState==GAME_RACING)s="RACING";

  char json[220];

  snprintf(
    json,
    sizeof(json),
    "{\"state\":\"%s\",\"p1\":%d,\"p2\":%d,\"target\":%d,\"winner\":%d,\"countdown\":%d}",
    s,
    player1Score,
    player2Score,
    TARGET_DISTANCE,
    winner,
    countdownValue()
  );

  server.sendHeader("Cache-Control","no-store, no-cache, must-revalidate");
  server.sendHeader("Pragma","no-cache");
  server.send(200,"application/json",json);
}

void handleNotFound(){
  server.send(404,"text/plain","NOT FOUND");
}

void setup(){
  Serial.begin(115200);
  delay(300);

  pinMode(PLAYER1_PIN,INPUT_PULLUP);
  pinMode(PLAYER2_PIN,INPUT_PULLUP);

  resetButton(p1Button);
  resetButton(p2Button);

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAPConfig(localIP,gateway,subnet);

  bool ok=WiFi.softAP(WIFI_NAME,WIFI_PASSWORD);

  server.on("/",HTTP_GET,handleRoot);
  server.on("/state",HTTP_GET,handleState);
  server.on("/start",HTTP_POST,handleStart);
  server.on("/reset",HTTP_POST,handleReset);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println();
  Serial.println("===============================");
  Serial.println("RESCUE RENE OCEAN CONNECTION FIX V4");
  Serial.println("===============================");
  Serial.println(ok ? "WIFI READY" : "WIFI FAILED");
  Serial.print("OPEN: http://");
  Serial.println(WiFi.softAPIP());
  Serial.println("P1 GPIO25 -> BUTTON -> GND");
  Serial.println("P2 GPIO26 -> BUTTON -> GND");
}

void loop(){
  updateButtons();
  updateGame();

  maintainWiFiAP();

  server.handleClient();

  updateButtons();
  updateGame();

  yield();
  delay(1);
}
