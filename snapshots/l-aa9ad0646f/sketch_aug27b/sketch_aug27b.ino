#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

// Harmless local prank access point.
// It does not collect passwords or disrupt other Wi-Fi networks.
const char* AP_NAME = "OpenWifi";

IPAddress apIP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

DNSServer dnsServer;
WebServer server(80);

const char MAIN_PAGE[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Wi-Fi Loading...</title>
  <style>
    * { box-sizing: border-box; }
    body {
      margin: 0; min-height: 100vh; display: grid; place-items: center;
      padding: 24px; background: #111827; color: white;
      font-family: Arial, sans-serif; text-align: center;
    }
    .card {
      width: min(420px, 100%); padding: 32px 24px; border-radius: 24px;
      background: linear-gradient(145deg, #1f2937, #374151);
      box-shadow: 0 20px 60px #0008;
    }
    .emoji { font-size: 64px; }
    h1 { margin: 12px 0 8px; font-size: 30px; }
    p { color: #d1d5db; line-height: 1.5; }
    button {
      border: 0; border-radius: 12px; padding: 14px 22px;
      background: #facc15; color: #111827; font-weight: 800;
      font-size: 16px; cursor: pointer;
    }
    button:active { transform: scale(.97); }
    .small { margin-top: 18px; font-size: 12px; color: #9ca3af; }
  </style>
</head>
<body>
  <main class="card">
    <div class="emoji">📶😭</div>
    <h1>Way ka load haw!</h1>
    <p>Your device connected successfully.<br>But the load is emotionally unavailable.</p>
    <button onclick="playMeme()">🔊 Tap to hear the meme</button>
    <div class="small">Disconnect and reconnect to escape.</div>
  </main>

  <script>
    function playMeme() {
      if (!('speechSynthesis' in window)) {
        alert('Your browser does not support voice playback.');
        return;
      }
      speechSynthesis.cancel();
      const voice = new SpeechSynthesisUtterance('Way ka load haw!');
      voice.lang = 'fil-PH';
      voice.rate = 0.9;
      voice.pitch = 1.15;
      speechSynthesis.speak(voice);
    }

    // Attempt automatic playback. Some phones require the button to be tapped.
    setTimeout(playMeme, 900);
  </script>
</body>
</html>
)rawliteral";

void showPrankPage() {
  server.send(200, "text/html; charset=utf-8", MAIN_PAGE);
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, gateway, subnet);
  // Open network: no password required.
  WiFi.softAP(AP_NAME);

  // Redirect DNS requests to the ESP32 so common captive-portal checks open the page.
  dnsServer.start(53, "*", apIP);

  server.on("/", HTTP_GET, showPrankPage);
  server.onNotFound(showPrankPage);
  server.begin();

  Serial.println();
  Serial.println("Prank Wi-Fi is ready.");
  Serial.print("Wi-Fi name: ");
  Serial.println(AP_NAME);
  Serial.println("Password: none (open network)");
  Serial.print("Open: http://");
  Serial.println(apIP);
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}
